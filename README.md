# LDBDangoMusic 0.3.0 — 歌词视觉完善

Windows x64 LDB Host 插件。0.2.0 的 demo 和真实 LX 歌词已获用户实测确认。
0.3.0 新增描边、轻量光晕、逐字弹出、换句淡出上浮，并默认提前 500ms 补偿用户反馈的延迟。

## 升级（保留现有 LX 配置）

下载 `releases/LDBDangoMusic-0.3.0-visual.zip`，退出游戏和 Host。
推荐从 ZIP 内 `source` 目录运行：

```powershell
./scripts/install.ps1 -GameRoot 'D:\SteamLibrary\steamapps\common\Left 4 Dead 2'
```

此脚本备份现有 DLL/配置，保留播放器模式、端口及已有视觉偏好，补充新设置，并把 offsetMs 设为 500。
可传 `-OffsetMs 300` 等值微调；正值使歌词提前，负值延后。

手动升级：只替换 `bin/.l4d2bridge/plugins/LDBDangoMusic.dll`，保留现有 config.json，修改 `offsetMs` 为 500。新视觉设置缺省时自动使用默认值。

## 首次安装 / demo

将 ZIP 的 `bin` 合并至游戏根目录；包内实际 config.json 默认 demo=false、offsetMs=500，用于已启用本机 API 的 BLF/LX（23330）。
如需离线演示，将包内 `config.demo.json` 复制为 `bin/.l4d2bridge/plugins/LDBDangoMusic/config.json`。
不存在配置时仍默认 demo。demo 不应用播放器的 500ms 补偿。

需要 LDB `codex/steam-reshade-plugin-entry-test` 的 **db5cb944314bb50c2e61835d5f0e8855c089e2e6** 或确认兼容的后续版本。
确认 `server.plugins = True`，配置修改后完整重启游戏和 Host。
插件仍支持 Device/SwapChain/附加 SwapChain Present，保留绘制错误和生命周期日志。

## 视觉配置

默认 displayMode=character，LX 时间细分到字符；普通 LRC 使用估算节奏。
可改为 word（按词）或 line（整句）。outline/glow 可分别关闭，textColor/accentColor 可自定义。
设置范围、同步说明及限制见 [0.3.0 视觉说明](docs/0.3.0-visuals.md)。

仅访问播放器本机 `/status`、`/lyric-all`；不登录、不播放音乐。
没有世界空间歌词、设置 UI、原版网易云接入或自动多行排版。

## 构建与验证

Windows 安装 MSVC C++ 桌面工具和 Windows SDK，在 x64 开发者 PowerShell：

```powershell
./scripts/build.ps1
./scripts/smoke.ps1
```

Linux 核心及模拟 HTTP 测试：`bash scripts/test.sh`。
MinGW x64 POSIX 交叉编译：`bash scripts/build-mingw.sh`，可通过 DANGO_CXX 指定编译器。
`python scripts/package.py` 打包当前 DLL、Windows 验证程序、对应源码及文件校验清单。
MSVC /MD 产物需要 Visual C++ x64 运行库；ZIP 中 MinGW 静态运行库产物仅依赖 Windows 系统 DLL。

0.3.0 已执行 Linux 动画/字素/时间补偿、解析/Present 路由及模拟 HTTP 测试，并交叉编译。
**新版视觉、同步与帧时间仍待 Windows 游戏实测。** 光晕采用纹理偏移叠加，增加绘制次数；可设置 glow=false 降低开销。
0.2.0 的接口排错记录保留在 [旧版验证说明](docs/0.2.0-validation.md)。

源码 GPL-3.0，来源与独立依赖许可见 NOTICE.md。
