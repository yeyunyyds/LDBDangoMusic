#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/core_test.cpp -o build/core_test
./build/core_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/api_fixture.cpp -o build/api_fixture
python3 tests/test_api_fixture.py
python3 tests/test_camera_analysis.py
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/world_camera_test.cpp -o build/world_camera_test
./build/world_camera_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/peak_style_test.cpp -o build/peak_style_test
./build/peak_style_test
