param([switch]$NativeNetEase,[switch]$WithMediaBridge,[switch]$Diagnostics)
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
New-Item -ItemType Directory -Force build | Out-Null
$diagnosticFlags = @()
$dllOutput = "build/L4D2DangoMusic.dll"
if ($Diagnostics) { New-Item -ItemType Directory -Force build/diagnostics | Out-Null; $diagnosticFlags = @("/DDANGO_DIAGNOSTICS=1"); $dllOutput = "build/diagnostics/L4D2DangoMusic.dll" }
# Run from an x86 Native Tools / MSVC developer PowerShell.
& cl.exe @diagnosticFlags /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MD /LD /Isrc /Ivendor /Ivendor/imgui src/plugin.cpp vendor/imgui/imgui.cpp vendor/imgui/imgui_draw.cpp vendor/imgui/imgui_tables.cpp vendor/imgui/imgui_widgets.cpp vendor/imgui/backends/imgui_impl_dx9.cpp /Fobuild/ /link /DEF:src/L4D2DangoMusic.def /OUT:$dllOutput winhttp.lib d3d9.lib gdi32.lib user32.lib dxguid.lib comctl32.lib shell32.lib
if ($LASTEXITCODE -ne 0) { throw 'x86 L4N plugin build failed' }
if ($Diagnostics) { return }
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

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/entity_auto_motion_test.cpp /Fobuild/entity_auto_motion_test.obj /Febuild/entity_auto_motion_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Entity auto/motion build failed' }
& ./build/entity_auto_motion_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Entity auto/motion tests failed' }

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/menu_visual_test.cpp /Fobuild/menu_visual_test.obj /Febuild/menu_visual_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Menu visual build failed' }
& ./build/menu_visual_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Menu visual tests failed' }

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Ivendor /Isrc tests/steam_abi_test.cpp /Fobuild/steam_abi_test.obj /Febuild/steam_abi_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Steam ABI mock build failed' }
& ./build/steam_abi_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Steam ABI mock failed' }
& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Ivendor /Isrc tests/steam_progress_test.cpp /Fobuild/steam_progress_test.obj /Febuild/steam_progress_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Progress protocol build failed' }
& ./build/steam_progress_test.exe
if ($LASTEXITCODE -ne 0) { throw 'Progress protocol failed' }
if ($WithMediaBridge) {
    & dotnet publish media-bridge/DangoMediaBridge.csproj -c Release -o build/media-bridge
    if ($LASTEXITCODE -ne 0) { throw 'Windows media bridge build failed' }
}

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/netease_elog_test.cpp /Fobuild/netease_elog_test.obj /Febuild/netease_elog_test.exe
if ($LASTEXITCODE -ne 0) { throw 'NetEase elog tests build failed' }
& ./build/netease_elog_test.exe
if ($LASTEXITCODE -ne 0) { throw 'NetEase elog tests failed' }

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/auto_listening_test.cpp /Fobuild/auto_listening_test.obj /Febuild/auto_listening_test.exe
if ($LASTEXITCODE -ne 0) { throw "Auto listening tests build failed" }
& ./build/auto_listening_test.exe
if ($LASTEXITCODE -ne 0) { throw "Auto listening tests failed" }

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Itests /Isrc /Ivendor tests/network_isolation_test.cpp /Fobuild/network_isolation_test.obj /Febuild/network_isolation_test.exe
if ($LASTEXITCODE -ne 0) { throw "Network isolation test build failed" }
& ./build/network_isolation_test.exe
if ($LASTEXITCODE -ne 0) { throw "Network isolation test failed" }

& cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /Isrc /Ivendor tests/visual_presets_test.cpp /Fobuild/visual_presets_test.obj /Febuild/visual_presets_test.exe
if ($LASTEXITCODE -ne 0) { throw "Visual presets tests build failed" }
& ./build/visual_presets_test.exe
if ($LASTEXITCODE -ne 0) { throw "Visual presets tests failed" }
