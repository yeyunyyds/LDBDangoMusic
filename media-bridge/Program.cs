using System.Diagnostics;
using Windows.Media.Control;
using DangoMedia;
using System.Text.Json;

// Private per-game file protocol: no network listener, no player controls.
if (args.Length != 3 || !int.TryParse(args[0], out var parentId) || !long.TryParse(args[1], out var parentStart)) return;
var folder = Path.GetFullPath(args[2]);
Directory.CreateDirectory(folder);
using var singleton = new Mutex(true, @"Local\L4D2DangoMusic-media-" + parentId, out var owns);
if (!owns) return;
var output = Path.Combine(folder, "media-state-" + parentId + ".json");
var log = Path.Combine(folder, "media-bridge.log");
void Log(string message) { try { if (File.Exists(log) && new FileInfo(log).Length > 1024 * 1024) File.Delete(log); File.AppendAllText(log, DateTimeOffset.Now.ToString("O") + " " + message + "\n"); } catch { } }
bool ParentAlive() { try { using var p = Process.GetProcessById(parentId); return !p.HasExited && p.StartTime.ToUniversalTime().ToFileTimeUtc() == parentStart; } catch { return false; } }
int Source() { try { using var c = JsonDocument.Parse(File.ReadAllText(Path.Combine(folder, "media-request-" + parentId + ".json"))); return c.RootElement.GetProperty("source").GetInt32(); } catch { return 0; } }
var service = new LyricsService();
string key = "", app = "", title = "", artist = "";
long generation = 0, retryAt = 0;
Task<List<LrcLine>?>? search = null;
string searchKey = "";
List<LrcLine> lyrics = new();
string message = "Waiting for Windows media session";
GlobalSystemMediaTransportControlsSessionManager? manager = null;
Log("Bridge started; Windows SMTC, Netease/QQ/Kugou/LRCLIB; parent=" + parentId);
while (ParentAlive())
{
    try
    {
        manager ??= await GlobalSystemMediaTransportControlsSessionManager.RequestAsync().AsTask().WaitAsync(TimeSpan.FromSeconds(3));
        int source = Source();
        var sessions = manager.GetSessions().Where(s => Protocol.Matches(s.SourceAppUserModelId, source)).ToList();
        var session = sessions.FirstOrDefault(s => s.GetPlaybackInfo().PlaybackStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing)
            ?? sessions.FirstOrDefault(s => s.SourceAppUserModelId == app)
            ?? sessions.FirstOrDefault();
        if (session == null)
        {
            if (key != "") { key = ""; lyrics.Clear(); service.Cancel(); search = null; generation++; Log("Media session closed; lyrics cleared"); }
            Protocol.Write(output, new { schema = 1, parent = parentId, selection = source, tick = Environment.TickCount64, status = "stopped", name = "", singer = "", duration = 0, progress = 0, sourceId = "", generation, message = "No compatible SMTC session", lines = Array.Empty<object>() });
            await Task.Delay(250); continue;
        }
        var props = await session.TryGetMediaPropertiesAsync().AsTask().WaitAsync(TimeSpan.FromSeconds(2));
        var info = session.GetPlaybackInfo();
        var timeline = session.GetTimelineProperties();
        app = session.SourceAppUserModelId; title = props.Title; artist = props.Artist;
        double duration = (timeline.EndTime - timeline.StartTime).TotalSeconds;
        double position = (timeline.Position - timeline.StartTime).TotalSeconds;
        bool playing = info.PlaybackStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing;
        double rate = info.PlaybackRate ?? 1;
        // Use LastUpdatedTime to extrapolate sparse SMTC timeline samples; cap at song end.
        if (playing) position += Math.Max(0, (DateTimeOffset.Now - timeline.LastUpdatedTime).TotalSeconds) * rate;
        position = Math.Clamp(position, 0, Math.Max(0, duration));
        string nextKey = Protocol.Key(app, title, artist, duration);
        if (nextKey != key)
        {
            key = nextKey; lyrics = new(); search = null; retryAt = 0; service.Cancel(); generation++;
            message = "Searching synchronized lyrics"; Log("Track source=" + app + " title=" + title + " artist=" + artist + " duration=" + duration);
        }
        if (search is { IsCompleted: true })
        {
            try { var result = await search; if (searchKey == key && result is { Count: > 0 }) { lyrics = result; message = "Synchronized lyrics ready"; Log("Lyrics ready; lines=" + lyrics.Count + " words=" + lyrics.Count(l => l.WordTimings?.Count > 0)); } }
            catch (Exception e) { message = "Lyric search failed"; Log(e.GetType().Name + ": " + e.Message); }
            search = null; retryAt = Environment.TickCount64 + 30000;
            if (lyrics.Count == 0) message = "No synchronized lyrics; retry in 30s";
        }
        if (!string.IsNullOrWhiteSpace(title) && duration > 0 && lyrics.Count == 0 && search == null && Environment.TickCount64 >= retryAt)
        { searchKey = key; search = service.SearchAsync(title, artist, TimeSpan.FromSeconds(duration)); }
        Protocol.Write(output, new { schema = 1, parent = parentId, selection = source, tick = Environment.TickCount64, status = playing ? "playing" : "paused", name = title, singer = artist, duration, progress = position, playbackRate = rate, sourceId = app, generation, message, lines = Protocol.Lines(lyrics) });
    }
    catch (Exception e)
    {
        Log(e.GetType().Name + ": " + e.Message);
        // A failed SMTC query cannot keep the previous track alive.
        try { Protocol.Write(output, new { schema = 1, parent = parentId, selection = Source(), tick = Environment.TickCount64, status = "stopped", name = "", singer = "", duration = 0, progress = 0, sourceId = "", generation, message = "SMTC unavailable: " + e.GetType().Name, lines = Array.Empty<object>() }); } catch { }
    }
    await Task.Delay(250);
}
service.Cancel();
try { File.Delete(output); File.Delete(Path.Combine(folder, "media-request-" + parentId + ".json")); } catch { }
Log("Bridge stopped");
