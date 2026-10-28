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
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/settings_lifecycle_test.cpp -o build/settings_lifecycle_test
./build/settings_lifecycle_test
python3 tests/test_presets.py
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/lyric_lifecycle_test.cpp -o build/lyric_lifecycle_test
./build/lyric_lifecycle_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/component_layout_test.cpp -o build/component_layout_test
./build/component_layout_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/player_session_test.cpp -o build/player_session_test
./build/player_session_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/visible_animation_test.cpp -o build/visible_animation_test
./build/visible_animation_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/wall_panel_test.cpp -o build/wall_panel_test
./build/wall_panel_test
