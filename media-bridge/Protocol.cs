using System.Text.Json;
namespace DangoMedia;

public static class Protocol
{
    public static string Key(string app, string title, string artist, double duration) =>
        app + "\u001f" + title + "\u001f" + artist + "\u001f" + Math.Round(duration * 1000);
    public static bool Matches(string app, int source)
    {
        app = new string(app.ToLowerInvariant().Where(char.IsLetterOrDigit).ToArray());
        return source switch
        {
            3 => app.Contains("cloudmusic") || app.Contains("netease") || app.Contains("music163"),
            4 => app.Contains("qqmusic"),
            5 => app.Contains("kugou"),
            6 => app.Contains("spotify"),
            7 => app.Contains("tidal"),
            _ => !app.Contains("left4dead") && !app.Contains("l4d2") && !app.Contains("dangomedia")
        };
    }
    public static object[] Lines(IEnumerable<LrcLine> lines) => lines.Select(l => (object)new
    {
        start = (long)l.Time.TotalMilliseconds, text = l.Text, translation = l.TranslatedText ?? "",
        words = l.WordTimings?.Select(w => new { start = (long)l.Time.TotalMilliseconds + w.StartMs, duration = w.DurationMs, text = w.Text }).ToArray()
    }).ToArray();
    public static void Write(string path, object data)
    {
        var temp = path + ".tmp";
        File.WriteAllText(temp, JsonSerializer.Serialize(data));
        File.Move(temp, path, true);
    }
}
