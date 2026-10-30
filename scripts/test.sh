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
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/source_trace_test.cpp -o build/source_trace_test
./build/source_trace_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/wall_scene_test.cpp -o build/wall_scene_test
./build/wall_scene_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/hud_menu_policy_test.cpp -o build/hud_menu_policy_test
./build/hud_menu_policy_test

g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/native_menu_test.cpp -o build/native_menu_test
./build/native_menu_test

g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/menu_pages_test.cpp -o build/menu_pages_test
./build/menu_pages_test

python3 scripts/generate_menu_glyphs.py --check
g++ -std=c++17 -O2 -Isrc -Ivendor -Ivendor/imgui tests/menu_font_test.cpp vendor/imgui/imgui.cpp vendor/imgui/imgui_draw.cpp vendor/imgui/imgui_tables.cpp vendor/imgui/imgui_widgets.cpp -o build/menu_font_test
./build/menu_font_test

g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/scene_attachment_test.cpp -o build/scene_attachment_test
./build/scene_attachment_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/source_entity_contract_test.cpp -o build/source_entity_contract_test
./build/source_entity_contract_test
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/source_entity_binding_test.cpp -o build/source_entity_binding_test
./build/source_entity_binding_test

# Optional replay input is generated from user DLLs, which are not redistributed.
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -Ivendor tests/source_entity_slot_replay.cpp -o build/source_entity_slot_replay

g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/entity_follow_test.cpp -o build/entity_follow_test
./build/entity_follow_test
