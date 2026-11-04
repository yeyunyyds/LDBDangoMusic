using DangoMedia;
using System.Text;
using System.Text.Json;
using System.Net;

void Check(bool condition, string text) { if (!condition) throw new Exception(text); }
var title = "Test Song";var artist = "Test Artist";
foreach(var source in new[]{"netease","qq","kugou","lrclib"})
{
 var handler = new Fixture(source);
 var lyrics = await new LyricsService(handler).SearchAsync(title,artist,TimeSpan.FromSeconds(100));
 Check(lyrics?.Count==2,source+" synchronized lines");
 Check(lyrics![0].Text=="Hello world",source+" base64/LRC");
 if(source is "netease" or "qq") Check(lyrics[0].TranslatedText=="你好 世界",source+" translation");
 if(source=="netease")
 {
  Check(lyrics[0].WordTimings?.Count==2,"YRC count");
  var json=JsonSerializer.Serialize(Protocol.Lines(lyrics));using var doc=JsonDocument.Parse(json);
  Check(doc.RootElement[0].GetProperty("words")[1].GetProperty("start").GetInt64()==1500,"YRC absolute timing");
 }
 Check(handler.Requests>0,source+" requests");
}
Check(Protocol.Matches("CloudMusic.exe",3)&&!Protocol.Matches("QQMusic.exe",3),"Netease filter");
Check(Protocol.Matches("Netease.CloudMusic_123!App",3)&&Protocol.Matches("cloud-music.exe",3)&&Protocol.Matches("music.163.com",3),"Netease SMTC identifier variants");
Check(Protocol.Matches("QQMusic.exe",4)&&Protocol.Matches("KugouMusic.exe",5),"QQ/Kugou filters");
Check(!Protocol.Matches("left4dead2.exe",0),"game excluded");
Check(Protocol.Key("a","t","r",30)!=Protocol.Key("b","t","r",30),"source identity");
var delayed=new Fixture("netease",true);var service=new LyricsService(delayed);
var stale=service.SearchAsync(title,artist);service.Cancel();Check(await stale==null,"cancelled generation discarded");
Console.WriteLine("Passed: Netease YRC/translation, QQ/base64/translation, Kugou search/download, LRCLIB, filters, source identity, stale-search cancellation");

sealed class Fixture(string provider,bool delayed=false):HttpMessageHandler
{
 public int Requests;
 const string Lrc="[00:01.00]Hello world\n[01:30.00]Last line";
 const string Trans="[00:01.00]你好 世界\n[01:30.00]末句";
 static string B64(string text)=>Convert.ToBase64String(Encoding.UTF8.GetBytes(text));
 protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage req,CancellationToken token)
 {
  Interlocked.Increment(ref Requests);if(delayed)await Task.Delay(50,token);
  var uri=req.RequestUri!.ToString();object data=new{};bool handled=false;
  if(provider=="netease" && uri.Contains("api/search/get")) { data=new {result=new {songs=new[]{new {id=1,name="Test Song",artists=new[]{new{name="Test Artist"}}}}}};handled=true; }
  else if(provider=="netease"&&uri.Contains("api/song/lyric")){data=new {lrc=new{lyric=Lrc},tlyric=new{lyric=Trans},yrc=new{lyric="[1000,1000](1000,500,0)Hello (1500,500,0)world\n[90000,1000](90000,1000,0)Last line"}};handled=true;}
  else if(provider=="qq"&&uri.Contains("musicu.fcg"))
  {
   var body=await req.Content!.ReadAsStringAsync(token);
   data=body.Contains("DoSearchForQQMusicDesktop")?(object)new {req=new {data=new {body=new {song=new {list=new[]{new{name="Test Song",mid="abc",singer=new[]{new{name="Test Artist"}}}}}}}}}:new {req=new{data=new{lyric=B64(Lrc),trans=B64(Trans)}}};handled=true;
  }
  else if(provider=="kugou"&&uri.Contains("mobilecdn.kugou")){data=new {data=new{info=new[]{new{songname="Test Song",singername="Test Artist",hash="hash"}}}};handled=true;}
  else if(provider=="kugou"&&uri.Contains("lyrics.kugou.com/search")){data=new {candidates=new[]{new{id="1",accesskey="key"}}};handled=true;}
  else if(provider=="kugou"&&uri.Contains("lyrics.kugou.com/download")){data=new {content=B64(Lrc)};handled=true;}
  else if(provider=="lrclib"&&uri.Contains("lrclib.net")){data=new[]{new{syncedLyrics=Lrc}};handled=true;}
  return new HttpResponseMessage(handled?HttpStatusCode.OK:HttpStatusCode.NotFound){Content=new StringContent(JsonSerializer.Serialize(data),Encoding.UTF8,"application/json")};
 }
}
