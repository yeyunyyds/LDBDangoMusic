$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
New-Item -ItemType Directory -Force build | Out-Null
# Run from an x64 Native Tools Command Prompt / initialized MSVC PowerShell.
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MD /LD /Isrc /Ivendor src/plugin.cpp /Fobuild/plugin.obj /link /OUT:build/LDBDangoMusic.dll winhttp.lib d3d9.lib gdi32.lib user32.lib
if ($LASTEXITCODE -ne 0) { throw 'Plugin build failed' }
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/core_test.cpp /Fobuild/core_test.obj /Febuild/core_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
& ./build/core_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Ivendor tests/host_smoke.cpp /Fobuild/host_smoke.obj /Febuild/host_smoke.exe /link d3d9.lib user32.lib
if ($LASTEXITCODE -ne 0) { throw 'Smoke harness build failed' }
