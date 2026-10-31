$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('l4d2dango-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force (Join-Path $testRoot 'build'), (Join-Path $testRoot 'build/L4D2DangoMusic') | Out-Null
try {
    & (Join-Path $repo 'build/source_entity_backend_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Entity diagnostic producer mock failed' }
    & (Join-Path $repo 'build/wall_scene_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Wall scene tests failed' }
    & (Join-Path $repo 'build/source_walls_abi_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'x86 trace ABI smoke failed' }
    Copy-Item (Join-Path $repo 'build/L4D2DangoMusic.dll') (Join-Path $testRoot 'build')
    '{"demo":true,"cameraDiagnostics":true}' | Set-Content -Encoding ASCII (Join-Path $testRoot 'build/L4D2DangoMusic/config.json')
    Push-Location $testRoot
    try {
        & (Join-Path $repo 'build/l4n_smoke.exe')
        if ($LASTEXITCODE -ne 0) { throw 'x86 L4N smoke failed' }
    } finally { Pop-Location }
} finally { Remove-Item -Recurse -Force $testRoot }
