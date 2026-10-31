$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
New-Item -ItemType Directory -Force build | Out-Null
# Run from an x86 Native Tools / MSVC developer PowerShell.
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MD /LD /Isrc /Ivendor /Ivendor/imgui src/plugin.cpp vendor/imgui/imgui.cpp vendor/imgui/imgui_draw.cpp vendor/imgui/imgui_tables.cpp vendor/imgui/imgui_widgets.cpp vendor/imgui/backends/imgui_impl_dx9.cpp /Fobuild/ /link /DEF:src/L4D2DangoMusic.def /OUT:build/L4D2DangoMusic.dll winhttp.lib d3d9.lib gdi32.lib user32.lib dxguid.lib comctl32.lib
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
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/source_walls_abi_test.cpp /Fobuild/source_walls_abi_test.obj /Febuild/source_walls_abi_test.exe /link user32.lib
if ($LASTEXITCODE -ne 0) { throw 'Trace ABI test build failed' }
& ./build/source_walls_abi_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Trace ABI test failed' }

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc tests/wall_scene_test.cpp /Fobuild/wall_scene_test.obj /Febuild/wall_scene_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Wall scene test build failed' }
& ./build/wall_scene_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Wall scene tests failed' }
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /Isrc /Ivendor tests/source_entity_backend_test.cpp /Fobuild/source_entity_backend_test.obj /Febuild/source_entity_backend_test.exe /link user32.lib
if ($LASTEXITCODE -ne 0) { throw 'Entity diagnostic producer test build failed' }
& ./build/source_entity_backend_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Entity diagnostic producer mock failed' }

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc tests/gnome_follow_test.cpp /Fobuild/gnome_follow_test.obj /Febuild/gnome_follow_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Gnome representation test build failed' }
& ./build/gnome_follow_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Gnome representation tests failed' }
