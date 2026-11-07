#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
compiler="${DANGO_CXX:-i686-w64-mingw32-g++}"
mkdir -p build
build_dir=build
diagnostic_flags=()
if [[ "${DANGO_DIAGNOSTICS:-0}" == 1 ]]; then
 build_dir=build/diagnostics
 mkdir -p "$build_dir"
 diagnostic_flags=(-DDANGO_DIAGNOSTICS=1)
fi
"$compiler" "${diagnostic_flags[@]}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-misleading-indentation -Wno-cast-function-type -shared -static -Ivendor -Ivendor/imgui -Isrc src/plugin.cpp vendor/imgui/imgui.cpp vendor/imgui/imgui_draw.cpp vendor/imgui/imgui_tables.cpp vendor/imgui/imgui_widgets.cpp vendor/imgui/backends/imgui_impl_dx9.cpp src/L4D2DangoMusic.def -o "$build_dir/L4D2DangoMusic.dll" -lwinhttp -ld3d9 -lgdi32 -luser32 -luuid -lcomctl32 -lshell32
if [[ "${DANGO_DIAGNOSTICS:-0}" == 1 ]]; then exit 0; fi
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/l4n_smoke.cpp -static -o build/l4n_smoke.exe -ld3d9 -luser32
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/core_test.cpp -static -o build/core_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/interface_inventory_test.cpp -static -o build/interface_inventory_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/source_walls_abi_test.cpp -static -o build/source_walls_abi_test.exe -luser32

"$compiler" -std=c++17 -O2 -Isrc tests/wall_scene_test.cpp -static -o build/wall_scene_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/source_entity_backend_test.cpp -static -o build/source_entity_backend_test.exe -luser32
"$compiler" -std=c++17 -O2 -Isrc tests/gnome_follow_test.cpp -static -o build/gnome_follow_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/entity_auto_motion_test.cpp -static -o build/entity_auto_motion_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/menu_visual_test.cpp -static -o build/menu_visual_test.exe

"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/steam_abi_test.cpp -static -o build/steam_abi_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/steam_progress_test.cpp -static -o build/steam_progress_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/netease_elog_test.cpp -static -o build/netease_elog_test.exe

"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/auto_listening_test.cpp -static -o build/auto_listening_test.exe

"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/visual_presets_test.cpp -static -o build/visual_presets_test.exe

"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/menu_navigation_test.cpp -static -o build/menu_navigation_test.exe
