$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('ldbdango-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force (Join-Path $testRoot 'build'), (Join-Path $testRoot 'plugins/LDBDangoMusic') | Out-Null
try {
    Copy-Item (Join-Path $repo 'build/LDBDangoMusic.dll') (Join-Path $testRoot 'build')
    '{"demo":true,"cameraDiagnostics":true}' | Set-Content -Encoding ASCII (Join-Path $testRoot 'plugins/LDBDangoMusic/config.json')
    Push-Location $testRoot
    try {
        & (Join-Path $repo 'build/host_smoke.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Windows D3D9 smoke failed' }
    } finally { Pop-Location }
} finally { Remove-Item -Recurse -Force $testRoot }
