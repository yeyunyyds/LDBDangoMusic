#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
compiler="${DANGO_CXX:-x86_64-w64-mingw32-g++}"
mkdir -p build
"$compiler" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -shared -static -Ivendor -Isrc src/plugin.cpp -o build/LDBDangoMusic.dll -lwinhttp -ld3d9 -lgdi32 -luser32
"$compiler" -std=c++17 -O2 -Wall -Wextra -Werror -Ivendor tests/host_smoke.cpp -o build/host_smoke.exe -static -ld3d9 -luser32
"$compiler" -std=c++17 -O2 -Ivendor -Isrc tests/core_test.cpp -static -o build/core_test.exe
