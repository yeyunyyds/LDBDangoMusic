#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
compiler="${DANGO_CXX:-i686-w64-mingw32-g++}"
mkdir -p build
"$compiler" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-misleading-indentation -Wno-cast-function-type -shared -static -Ivendor -Isrc src/plugin.cpp src/L4D2DangoMusic.def -o build/L4D2DangoMusic.dll -lwinhttp -ld3d9 -lgdi32 -luser32 -luuid
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/l4n_smoke.cpp -static -o build/l4n_smoke.exe -ld3d9 -luser32
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/core_test.cpp -static -o build/core_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/interface_inventory_test.cpp -static -o build/interface_inventory_test.exe
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/source_walls_abi_test.cpp -static -o build/source_walls_abi_test.exe -luser32

"$compiler" -std=c++17 -O2 -Isrc tests/wall_scene_test.cpp -static -o build/wall_scene_test.exe
