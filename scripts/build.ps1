$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
New-Item -ItemType Directory -Force build | Out-Null
# Run from an x86 Native Tools / MSVC developer PowerShell.
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MD /LD /Isrc /Ivendor src/plugin.cpp /Fobuild/plugin.obj /link /DEF:src/LDBDangoMusic.def /OUT:build/LDBDangoMusic.dll winhttp.lib d3d9.lib gdi32.lib user32.lib dxguid.lib
if ($LASTEXITCODE -ne 0) { throw 'x86 L4N plugin build failed' }
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/core_test.cpp /Fobuild/core_test.obj /Febuild/core_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Core build failed' }
& ./build/core_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Ivendor tests/l4n_smoke.cpp /Fobuild/l4n_smoke.obj /Febuild/l4n_smoke.exe /link d3d9.lib user32.lib
if ($LASTEXITCODE -ne 0) { throw 'L4N smoke build failed' }
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/interface_inventory_test.cpp /Fobuild/interface_inventory_test.obj /Febuild/interface_inventory_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Interface inventory test build failed' }
& ./build/interface_inventory_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Interface inventory tests failed' }
