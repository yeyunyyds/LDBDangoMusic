LDBDangoMusic is distributed under GPL-3.0 (see LICENSE).
The lyric presentation and LX integration are developed with reference to:
- Sorihiko/DangoMusic, da696d0871fa348323782d7d299ba10df46e07db (GPL-3.0)
- ZHIZHU4410/PeakDangoMusic, c9fe555b53f7d3cea85bfdcb92476a1ed6853ad0 (GPL-3.0)
This project implements a native C++ D3D9 Host plugin, not a Minecraft or Unity mod.
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
