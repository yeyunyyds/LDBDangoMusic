# L4D2DangoMusic

> 此分支为基于 v0.9.3 的实体接口实验，已合入原生 HUD、菜单归类、保存状态和缺字修正。实体诊断入口位于“更多设置 → 实验：实体接口只读诊断”，尚未实现实机实体挂载。稳定版仍见 [v0.9.3 Release](https://github.com/yeyunyyds/LDBDangoMusic/releases/tag/v0.9.3)。

为《求生之路 2》（Left 4 Dead 2）提供音乐歌词显示的 L4N 插件。通过兼容 LX / BLF 的本地音乐 API 获取播放进度和歌词，在游戏中呈现带逐字动画的歌词，并支持世界空间定位、歌名与作者显示以及游戏内调节。

插件由 L4N 加载，在 **32 位游戏进程**中直接读取 Source Engine 相机信息并使用 D3D9 绘制。运行依赖为 L4N 和音乐播放器的本地 API，不需要 LDB 或 DXVK。

## 功能

- 同步显示 LRC / LX 歌词，支持逐字时间信息及翻译。
- 场景挂画：自动沿附近墙面放置当前歌词，各句独立停留、淡出，支持手动放置与轮换。
- 世界空间歌词：歌词随相机移动产生视差，也可使用屏幕显示模式。
- 支持 1～9 个歌词位置依次轮换，分别调整高度、左右、距离和大小。
- 歌名、作者独立显示与定位，歌词支持整体位置调节。
- 提供六套视觉预设、自定义颜色、柔光、逐字弹跳和抖动效果。
- 通过 L4N 菜单实时设置，支持固定内容预览、动画预览、位置重置与配置保存。
- 支持暂停、恢复播放、换歌及播放器断开后的重新同步。

本插件负责显示歌词，不播放音乐，也不提供音乐资源。

## 运行要求

- Windows 版《求生之路 2》。
- 已安装并正常工作的 L4N，支持插件接口 v2 的 HUD 菜单功能。
- 提供兼容 `/status` 和 `/lyric-all` HTTP 接口的 LX / BLF 音乐播放器，并已开启本地 API。

默认连接 `127.0.0.1:23330`。不同播放器版本提供的 API 可能不同，请先确认所用版本支持上述接口。

世界空间模式依赖受支持的 Source 相机接口与游戏构建；无法取得有效相机信息时，插件会回退到屏幕显示。普通世界空间歌词不提供墙体遮挡；场景挂画使用世界几何射线做整幅遮挡检查，范围和限制见挂画指南。

## 安装

[下载实体诊断实验包](https://github.com/yeyunyyds/LDBDangoMusic/raw/refs/heads/experiment/entity-attachments/artifacts/L4D2DangoMusic-entity-diagnostics.zip) · [诊断指南](docs/entity-inventory-test.md) · [稳定版 v0.9.3](https://github.com/yeyunyyds/LDBDangoMusic/releases/tag/v0.9.3)

安装压缩包内仅包含 **bin 文件夹和 README.md**，不附带源码、构建工具、测试程序或额外依赖。

1. 退出游戏，确认 L4N 已安装并正常工作。
2. 将压缩包内的 `bin` 文件夹合并到游戏根目录。
3. 首次安装复制全部文件。升级时替换 DLL，保留已有的 `config.json`，避免覆盖设置。
4. 完整重启游戏，在 L4N 菜单进入 L4D2DangoMusic。

安装路径：

```text
bin/neko/plugins/L4D2DangoMusic.dll
bin/neko/plugins/L4D2DangoMusic/config.json
```

源码和构建脚本维护在 [`main` 分支](https://github.com/yeyunyyds/LDBDangoMusic/tree/main)，[`zip` 分支](https://github.com/yeyunyyds/LDBDangoMusic/tree/zip) 仅提供安装压缩包。

从源码构建的用户仍可使用 `scripts/install.ps1 -GameRoot "游戏根目录"` 安装；运行目录为源码根目录。安装包用户直接复制 `bin` 即可。

请只保留一份启用的歌词插件。旧的 `LDBDangoMusic.dll` 和改名副本应移出加载目录，否则可能出现两份歌词或菜单分别控制不同副本的情况。

## 使用与设置

1. 启动音乐播放器并开启本地 API，播放一首有歌词的歌曲。
2. 启动游戏，在 L4N 菜单中选择 **L4D2DangoMusic**。
3. 按需调整显示、翻译、预设、颜色、动画及同步偏移。
4. 点击 **保存设置**，使修改在下次启动时继续生效。

进入 **位置与数量** 页面后，实时歌词会暂时替换为固定的歌名、作者和编号歌词，方便调节位置；音乐仍继续播放。各级页面均可选择 **保存设置并结束预览**，恢复当前歌曲的歌词显示。

默认使用两个轮换位置，歌词位于屏幕中心稍上方，长句自动回到视野的功能默认关闭。换句复用世界位置时，旧锚点必须仍在视野内，且到相机的距离小于该歌词位置配置距离的 3 倍；超出时按预设重新放置。可在“显示与歌词信息 → 世界位置与回归 → 复用距离上限”选择 1、1.5、2、3（默认）、4、5 或 10 倍。歌名和作者可在显示菜单中分别开启。

主菜单共 8 项：歌词显示、显示与歌词信息、位置与数量、歌词颜色、动画与停留、场景挂画、保存设置、更多设置。单行长度与同步偏移位于“显示与歌词信息”；颜色预设、正文颜色、光晕颜色与柔光合并在“歌词颜色”。颜色预设仅更改配色和柔光，手动修改后显示“自定义”，不重置动画。

菜单由插件原生 HUD 绘制，L4N 仅提供入口。设置即时生效，当前值逐帧刷新，返回父页也显示最新状态，不会因开关或调参数叠加层级。

- `1..9`：选择当前页项目；`-` / `=`：上页 / 下页。
- `0`：返回一级，根页关闭；`Esc` 或 `/`：关闭并退出位置预览。
- “更多设置 → 菜单外观”提供深黑、烟灰、石墨、高透明四套背景；选中透明度或字号后用 `-` / `=` 调节，位置页用方向键移动。
- 默认从 L4N 进入，F10 默认关闭，可在外观页启用。菜单外观和歌词设置均通过主菜单“保存设置”持久化；任何实际修改都会标记 `[未保存]`，写入成功后显示 `[已保存]`。

仅在菜单打开且游戏窗口获得焦点时读取菜单按键，入口按键释放后才接收下一次操作；不捕获鼠标或 WASD。

挂画背景默认关闭，可在“场景挂画 → 挂画外观与维护 → 背景面板”开启；升级时保留已有配置中的选择。开启 **场景挂画歌词** 后，可选择自动寻找墙面，或用 F8 手动放置挂画。挂画参数、旧句停留、背景、固定世界歌词回退及完整设置说明见 [场景挂画指南](docs/wall-experiment-guide.md)。

“靠近时后退”作用于悬浮世界歌词、悬浮歌名和作者；进入该组件配置距离的 60% 以内时持续推远。固定墙面挂画保持贴墙。无逐字时间轴的“估算渐显”独立生效，短句、无空格长词也会分字渐显；统一词条起点等于整句起点、没有进一步细分的数据也按无细分时间处理。

“显示与歌词信息 → 世界位置与回归 → 长句回到视野 → 自定义秒数”提供 ±0.1、±1、±10 秒调节，最大 300 秒，0 为关闭；保留 3/5/10 秒预设，修改后可保存设置。

### 配置文件

配置路径为 `bin/neko/plugins/L4D2DangoMusic/config.json`。通常使用游戏内菜单即可；手动编辑配置时，请先退出游戏。

| 配置项 | 用途 | 默认值 |
| --- | --- | --- |
| `port` | 本地音乐 API 端口 | `23330` |
| `demo` | 使用离线示例歌词 | `false` |
| `translation` | 显示翻译 | `false` |
| `worldLyrics` | 使用世界空间显示 | `true` |
| `offsetMs` | 歌词同步偏移，正值提前 | `500` |
| `layout.lyricCount` | 歌词轮换位置数量 | `2` |
| `reuseDistanceFactor` | 可见旧锚点的复用距离上限倍率，分别乘以各歌词位置配置的距离 | `3` |
| `reanchorMs` | 长句回到视野的间隔，`0` 表示关闭 | `0` |
| `wallEnabled` | 启用场景挂画 | `false` |
| `wallAutomatic` | 自动寻找墙面；关闭为手动位置轮换 | `true` |
| `wallCapacity` | 挂画数量上限 | `6` |
| `wallLingerMs` / `wallFadeMs` | 旧句额外停留 / 淡出时间 | `4000` / `1000` |
| `wallFallback` | 新句找不到墙面时使用固定世界歌词 | `true` |
| `showTitle` / `showArtist` | 显示歌名 / 作者 | `false` |

完整配置见 [config.example.json](config.example.json)，视觉预设见 [presets](presets/)。如需独立检查显示效果，可使用游戏内预览，或将 `demo` 设为 `true`。

### 问题排查

- **没有歌词**：确认播放器 API 已开启、端口一致，且歌曲带有同步歌词。正常播放模式在暂停、停止或断开时会隐藏歌词。
- **歌词有提前或延后**：在菜单中调整同步偏移；正值使歌词提前，负值使歌词延后。
- **出现双份歌词**：检查 L4N 插件目录，移出旧副本后完整重启游戏。若曾安装旧版 Host 歌词插件，也需停用该副本并重启对应 Host。
- **动画不明显**：部分预设会关闭动画，可选择“恢复默认动画”，再进入“动画预览”检查。

菜单中的 **版本、路径与播放器诊断** 可查看实际加载的 DLL 路径和播放状态。日志位于 `bin/neko/plugins/L4D2DangoMusic/L4D2DangoMusic.log`；反馈问题时，请附上日志、插件版本和复现步骤。

## 从源码构建

### Windows / MSVC

安装 Visual Studio C++ 构建工具，在 **x86 开发者 PowerShell** 中进入源码根目录并运行：

```powershell
.\scripts\build.ps1
```

生成文件位于 `build` 目录。构建使用项目内附带的 SDK 头文件，无需提供游戏 DLL。

### Linux / MinGW

安装支持 C++17 的 GCC、Python 3，以及 i686 MinGW-w64 POSIX 工具链：

```bash
bash scripts/test.sh
bash scripts/build-mingw.sh
```

可通过 `DANGO_CXX` 环境变量指定 MinGW 编译器路径。Windows D3D9 验证程序可通过 `scripts/smoke.ps1` 运行；打包使用 Python 3：

```bash
python scripts/package.py
```

最小安装包输出到 `build/packages`。包内只包含 `bin/` 与 `README.md`；许可和对应源码链接写入安装说明。生成后将安装包单独发布到 `zip` 分支。

跨平台回归测试和交叉编译不能替代实际 L4N 游戏环境验证。

## 参考项目与第三方代码

| 项目 | 用途 |
| --- | --- |
| [Sorihiko/DangoMusic](https://github.com/Sorihiko/DangoMusic) | 歌词呈现与音乐 API 集成参考。 |
| [ZHIZHU4410/PeakDangoMusic](https://github.com/ZHIZHU4410/PeakDangoMusic) | 歌词动画、逐字时序、排版与位置规则参考；相关逻辑适配为原生 C++ 实现。 |
| [yeyunyyds/L4D2_Dxvk_32to64_Bridge](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge) | 本项目所用 L4N SDK 头文件的取得来源；不作为运行依赖。 |
| [AlliedModders/hl2sdk · l4d2](https://github.com/alliedmodders/hl2sdk/tree/l4d2) | Source 接口资料参考。 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 配置与 API 数据解析。 |

具体来源、参考提交及第三方许可见 [NOTICE.md](NOTICE.md)。本项目不打包 Unity / TextMesh Pro 材质、上游游戏二进制、字体或音乐资源。

## 许可

本项目采用 [GPL-3.0](LICENSE) 许可。第三方代码与 SDK 头文件遵循各自的许可及声明，详见 [NOTICE.md](NOTICE.md)。

本版新增：原文与翻译按演唱进度显字（无逐字时间轴默认整句，可选择估算渐显）、单行长度倍率，以及世界歌词换句时沿用视野内原位置。墙面挂画也接入歌词和翻译动画。设置路径和说明见 [歌词设置说明](docs/0.8.1-lyrics-controls.md)。
