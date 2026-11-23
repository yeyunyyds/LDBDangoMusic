using System.Net;
using System.Net.Http;
using System.Text.Json;

namespace DangoMedia;

// Folia desktop exposes only lyrics on 127.0.0.1:32109. Playback metadata
// and the clock continue to come from the selected Windows SMTC session.
// This adapter never sends media controls and never contacts remote servers.
public sealed class FoliaLyrics : IDisposable
{
    public const int Port = 32109;
    public const string Endpoint = "http://127.0.0.1:32109/v1/lyric";
    private readonly HttpClient _http;
    private readonly bool _ownsClient;

    public FoliaLyrics(HttpMessageHandler? handler = null)
    {
        _ownsClient = true;
        _http = new HttpClient(handler ?? new HttpClientHandler
        {
            UseProxy = false, AllowAutoRedirect = false, UseCookies = false,
            UseDefaultCredentials = false
        })
        {
            Timeout = TimeSpan.FromSeconds(2),
            MaxResponseContentBufferSize = 2 * 1024 * 1024
        };
    }

    public async Task<List<LrcLine>?> FetchAsync(string smtcTitle, string smtcArtist)
    {
        using var request = new HttpRequestMessage(HttpMethod.Get, Endpoint);
        using var response = await _http.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);
        response.EnsureSuccessStatusCode();
        const int limit = 2 * 1024 * 1024;
        if (response.Content.Headers.ContentLength > limit)
            throw new InvalidDataException("Folia lyric response is too large");
        await using var stream = await response.Content.ReadAsStreamAsync();
        using var buffer = new MemoryStream();
        var chunk = new byte[8192];
        int received;
        while ((received = await stream.ReadAsync(chunk)) != 0)
        {
            if (buffer.Length + received > limit) throw new InvalidDataException("Folia lyric response is too large");
            buffer.Write(chunk, 0, received);
        }
        using var document = JsonDocument.Parse(buffer.ToArray(), new JsonDocumentOptions { MaxDepth = 16 });
        return Parse(document.RootElement, smtcTitle, smtcArtist);
    }

    private static string Normalize(string value) =>
        string.Concat(value.Where(c => !char.IsWhiteSpace(c))).ToUpperInvariant();

    private static string? OptionalString(JsonElement obj, string key)
    {
        if (!obj.TryGetProperty(key, out var value) || value.ValueKind is JsonValueKind.Null or JsonValueKind.Undefined)
            return null;
        if (value.ValueKind != JsonValueKind.String) throw new InvalidDataException("Invalid Folia lyric text");
        return value.GetString();
    }

    private static double Number(JsonElement obj, string key, double fallback = 0)
    {
        if (!obj.TryGetProperty(key, out var value)) return fallback;
        if (value.ValueKind != JsonValueKind.Number || !value.TryGetDouble(out var n) || !double.IsFinite(n))
            throw new InvalidDataException("Invalid Folia lyric timing");
        return n;
    }

    private static long Milliseconds(double seconds) => checked((long)Math.Round(seconds * 1000));

    public static List<LrcLine>? Parse(JsonElement root, string smtcTitle, string smtcArtist)
    {
        if (root.ValueKind == JsonValueKind.Null) return null;
        if (root.ValueKind != JsonValueKind.Object) throw new InvalidDataException("Invalid Folia lyric object");
        var title = OptionalString(root, "title");
        var artist = OptionalString(root, "artist");
        // A newly loaded track must never inherit the previous track's lyric snapshot.
        // Folia may omit these fields, so also delay the first fetch after an SMTC track change.
        if (!string.IsNullOrWhiteSpace(title) && !string.IsNullOrWhiteSpace(smtcTitle)
            && Normalize(title) != Normalize(smtcTitle)) return null;
        if (!string.IsNullOrWhiteSpace(artist) && !string.IsNullOrWhiteSpace(smtcArtist)
            && Normalize(artist) != Normalize(smtcArtist)) return null;
        var shiftMs = Number(root, "offset") - Number(root, "clipStartMs");
        if (Math.Abs(shiftMs) > 86_400_000) throw new InvalidDataException("Folia offset out of range");
        var accurate = root.TryGetProperty("wordByWord", out var wordByWord) &&
                       wordByWord.ValueKind == JsonValueKind.True;
        if (!root.TryGetProperty("lines", out var lines) || lines.ValueKind != JsonValueKind.Array
            || lines.GetArrayLength() > 10000) throw new InvalidDataException("Invalid Folia lyric lines");
        var result = new List<LrcLine>();
        foreach (var row in lines.EnumerateArray())
        {
            if (row.ValueKind != JsonValueKind.Object) throw new InvalidDataException("Invalid Folia lyric line");
            var text = OptionalString(row, "text") ?? "";
            if (text.Length == 0 || text.Length > 16384) continue;
            var start = Milliseconds(Number(row, "startTime") + shiftMs / 1000);
            var end = Milliseconds(Number(row, "endTime") + shiftMs / 1000);
            if (end <= 0 || end < start || start > 86_400_000) continue;
            var displayedStart = Math.Max(0, start);
            var lyric = new LrcLine(TimeSpan.FromMilliseconds(displayedStart), text)
            {
                TranslatedText = OptionalString(row, "translation") ?? ""
            };
            if (accurate && row.TryGetProperty("words", out var words) && words.ValueKind == JsonValueKind.Array)
            {
                if (words.GetArrayLength() > 4096) throw new InvalidDataException("Too many Folia words");
                var timings = new List<KaraokeWordTiming>();
                foreach (var word in words.EnumerateArray())
                {
                    if (word.ValueKind != JsonValueKind.Object) continue;
                    var wordText = OptionalString(word, "text") ?? "";
                    if (wordText.Length == 0 || wordText.Length > 16384) continue;
                    var ws = Milliseconds(Number(word, "startTime") + shiftMs / 1000);
                    var we = Milliseconds(Number(word, "endTime") + shiftMs / 1000);
                    if (we <= 0 || we < ws || ws > 86_400_000) continue;
                    var clippedStart = Math.Max(0, ws);
                    var clippedEnd = Math.Max(clippedStart, we);
                    if (clippedStart - displayedStart > int.MaxValue || clippedEnd - clippedStart > int.MaxValue) continue;
                    timings.Add(new KaraokeWordTiming((int)(clippedStart - displayedStart),
                        (int)(clippedEnd - clippedStart), wordText));
                }
                if (timings.Count > 0) lyric.WordTimings = timings;
            }
            result.Add(lyric);
        }
        return result.OrderBy(line => line.Time).ToList();
    }

    public void Dispose()
    {
        if (_ownsClient) _http.Dispose();
    }
}
