L4D2DangoMusic is distributed under GPL-3.0 (see LICENSE).
The lyric presentation and LX integration are developed with reference to:
- Sorihiko/DangoMusic, da696d0871fa348323782d7d299ba10df46e07db (GPL-3.0)
- ZHIZHU4410/PeakDangoMusic, c9fe555b53f7d3cea85bfdcb92476a1ed6853ad0 (GPL-3.0)
The current project implements a native C++ L4N plugin in the 32-bit game process,
using Source camera interfaces and D3D9 drawing. LDB and DXVK are not runtime
dependencies. Historical Host SDK files remain attributed below.
No upstream game binaries, audio, fonts, or account data are included.

vendor/ldb_host_plugin.h is copied verbatim from yeyunyyds/L4D2_Dxvk_32to64_Bridge,
db5cb944314bb50c2e61835d5f0e8855c089e2e6, plugins/host-sdk/ldb_host_plugin.h.
It retains its MIT SPDX identifier and copyright notice.

vendor/json.hpp: nlohmann/json v3.11.3, Copyright Niels Lohmann, MIT.
Its full copyright and license text are embedded in the header.

0.4.0 pure L4N migration:
- vendor/l4n/l4n_plugin.h: copied unchanged from LDB main cafc0442446c502a0de0ebb52d5e73ccaae67516,
  plugins/l4n/sdk/l4n_plugin.h. Supplied L4N SDK header has no license declaration;
  it is not relicensed under GPL by this project.
- Source interface names, signatures and slot inventory were checked against
  AlliedModders HL2SDK l4d2 2a31cd007b2d7d2f964dc093eedcf7a812cf9dd6,
  public/cdll_int.h. See vendor/source-interface-contract.json. No Valve SDK
  engine implementation or game binaries are redistributed.

0.4.2 corrects the Source binding using static inspection of user-supplied
client.dll / engine.dll. The verified profile in vendor/source-binary-profile.json
supersedes the old public-SDK slot inventory for runtime binding. Uploaded game
DLLs are not bundled or committed; only hashes and interface metadata are saved.

0.6.0 Peak-inspired visuals:
Native C++ adaptations of LyricAnimation, LyricLineView rules, layout and
placement from ZHIZHU4410/PeakDangoMusic c9fe555b53f7d3cea85bfdcb92476a1ed6853ad0,
GPL-3.0, are in src/peak_style.hpp. Animation math, deterministic wiggle, timing
and lifecycle are adapted for the existing D3D9 renderer. No Unity/TMP engine,
shader, material, font, game, or music resources are included. Cached alpha blur
is a native approximation; see docs/peak-style.md for differences.

0.6.0 renames the active project and artifacts to L4D2DangoMusic. Legacy names
appear only in migration instructions and preserved historical release packages.
L4N controls, settings snapshots, preset migration and scene submission gating
are native implementation additions. Prior third-party notices remain applicable.

Native HUD menu adaptation:
- Architecture, keyboard operation and appearance follow yeyunyyds/L4D2_Dxvk_32to64_Bridge,
  codex/l4n-native-hud b7aa0beb3c490515da48712495853ac50a888dc6,
  plugins/l4n/L4D2BridgePlugin.cpp (MIT, Copyright (c) 2026 yeyunyyds).
  Its license is preserved in vendor/BRIDGE-LICENSE.MIT.
- vendor/imgui contains Dear ImGui v1.91.9b, copied unchanged from the same
  revision. Copyright (c) 2014-2025 Omar Cornut and Dear ImGui contributors,
  MIT; see vendor/imgui/LICENSE.txt. Only the core and D3D9 backend are built.
- The lyric plugin keeps its own ImGui context and uses its existing D3D9
  Present/Reset chain. It does not call or require any Bridge control API.
