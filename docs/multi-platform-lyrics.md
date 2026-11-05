# Windows 音乐会话与多平台歌词

网易云原生功能已由用户验证，并合并至 `experiment/steam-progress-sync` 实验分支；正式 Release 仍为 v0.9.5。一起听和反向控制仅限 LX，网易云来源独立显示歌词。歌词获取参考 [Epi-Lo/desktop-lyric](https://github.com/Epi-Lo/desktop-lyric)，保留其 MIT 声明。

## 网易云修正：不依赖 SMTC 的原生通道

用户提供的状态文件确认桥接已运行，但没有匹配的 SMTC 会话；切到全部 Windows 会话也没有歌曲。原先仅靠 SMTC 的方案不能覆盖这类网易云客户端。

现在选择“网易云音乐”（musicSource=3）时改为原生 C++ 读取 `%LOCALAPPDATA%\NetEase\CloudMusic\cloudmusic.elog` 的播放事件，获取歌曲 ID、名称、歌手、时长、暂停/恢复及拖动进度，再按歌曲 ID 直接获取网易云歌词和原生翻译/可用 YRC。无需 SMTC，无需 .NET 或外部桥接程序。原文、翻译及所有现有显示逻辑复用现有渲染器。客户端日志格式依赖版本，仍需用户实机验收。

提供 `L4D2DangoMusic-0.9.5-netease-native.zip` 小包：只有 DLL、默认配置和 README。首次安装默认网易云；升级保留自己的配置后，请手动重新选择“网易云音乐”，尤其此前临时改成了 Windows 会话时。退出游戏后可以移除旧 DangoMediaBridge.exe，这一路不会启动它。LX 本地 API 也保留在同一个 DLL 中。

解析方式参考 [dsh-pet-qqmusic 的 netease-elog.js](https://github.com/344272586-sketch/dsh-pet-qqmusic/blob/HEAD/lib/netease-elog.js)，保留 MIT 声明。只在本机读取所需元数据，不上传日志、播放地址或 Cookie。有初始读取/单行大小限制，并校验记录的进程仍是活着的 cloudmusic.exe 及创建时间，避免重放已关闭客户端的旧歌曲。歌词请求在独立后台线程进行，切歌后的旧结果会被丢弃。日志缺失时会在菜单显示具体原因；部分旧版本、关闭日志或改变日志格式的客户端尚不支持。

构建原生小包：先执行 `bash scripts/build-mingw.sh`，再 `python3 scripts/package.py --variant netease-native`，不需要 dotnet。Windows/MSVC 可使用 `scripts/build.ps1 -NativeNetEase`；源码安装可使用 `scripts/install.ps1 -NativeNetEase -GameRoot "游戏根目录"`，升级配置仍保留，需在菜单选择网易云。

## 可选 Windows 会话桥接包

QQ、酷狗、Spotify、Tidal 等 SMTC 通道目前仍使用可选的 .NET 桥接，下面的“安装与验证”说明适用于完整桥接包，网易云请选择上述原生通道。

## 安装与验证 Windows 会话

1. 退出游戏，将本分支测试安装包内的 bin 合并到游戏根目录。升级时保留自己的 config.json，同时替换 DLL 和 DangoMediaBridge.exe。
2. 使用 Windows 10 2004 或更新的 64 位 Windows。桥接程序已自带 .NET 运行时，不需要额外安装环境，也不需要手工启动。
3. 打开提供 SMTC 的音乐客户端并播放歌曲，确认 Windows 系统媒体控件可以看到歌曲和播放进度；网易云原生通道不要求 SMTC。
4. 游戏菜单 → 显示与歌词信息 → 音乐来源 → Windows 音乐会话或指定客户端。该选择会退出离线 demo 模式；点击主菜单保存设置后，下次仍使用这个来源。
5. 检查原文、翻译开关、切歌、暂停/恢复、拖动进度、关闭后重新打开客户端。无同步歌词的歌曲不会沿用上一首的歌词。

路径：

```text
bin/neko/plugins/L4D2DangoMusic.dll
bin/neko/plugins/L4D2DangoMusic/config.json
bin/neko/plugins/L4D2DangoMusic/DangoMediaBridge.exe
```

菜单提供：自动（LX 优先）、LX 本地 API、Windows 音乐会话、网易云、QQ、酷狗、Spotify、Tidal。后三类英文播放器等只需要提供 SMTC，歌词仍从下面的四个来源匹配。手动指定客户端可以避免浏览器等其他媒体会话抢占。

默认自动模式优先连接 LX/BLF API `127.0.0.1:23330`；该 API 无法连接时切换 Windows 会话，每 5 秒检查 LX 是否恢复。API 仍连接着但暂停、停止或无歌曲时不会自动抢占为另一个播放器，此时可手动指定 Windows/网易云。LX 模式保留现有逐字数据、翻译和所有显示功能。

## 歌词来源

| 来源 | 接入内容 | 验证范围 |
| --- | --- | --- |
| 网易云 | 原生 elog 播放信息 + 歌曲 ID 直接取词，LRC、原生翻译、可用的 YRC；其他 SMTC 来源仍可搜索网易云曲库 | 独立网易云分支已获用户实机验证；合并包待复验 |
| QQ 音乐 | Desktop 搜索、GetPlayLyricInfo、Base64 LRC/翻译 | 参考实现移植与模拟响应测试，未实机验证 |
| 酷狗 | 歌曲 hash 搜索、歌词候选、accesskey 下载、Base64 LRC | 参考实现移植与模拟响应测试，未实机验证 |
| LRCLIB | 歌名/歌手搜索、同步 LRC | 参考实现移植与模拟响应测试，未实机验证 |

四个来源并行搜索，按参考项目的时长匹配规则选结果；清理括号后缀后可重试。翻译只使用歌词源自带翻译，不自动调用机器翻译。没有真实逐字时间时，继续遵守插件中“整句显示 / 估算渐显”的设置。搜索接口为各服务的公开接口，搜索匹配的版本可能与播放版本不同，且可用性会随平台接口变化。

## 运行与清理

- 游戏插件仍是原生 x86 DLL，桥接为独立的 x64 后台进程；不向音乐软件注入代码，不控制播放，不要求登录或填写 Cookie。
- 桥接在需要 Windows 来源时启动，读取当前游戏 PID 和启动时间，父进程结束后退出；插件正常停止时也结束自己启动的桥接。
- 不启动 HTTP 服务。用独立 PID 的原子 JSON 文件传递状态和歌词，过期 3 秒或身份不符时停止显示。
- SMTC 进度使用媒体会话的最后更新时间推算。某些客户端会发布滞后、不完整或冻结的时间线；可先使用插件同步偏移修正固定误差。
- 歌词搜索在后台异步运行，旧搜索结果不会写入已经切换的歌曲；没有结果每 30 秒重试，不持续轰炸接口。
- 保持原有实体 → 挂画 → 世界的显示优先度和所有动画，不引入另一套歌词绘制。

对于 SMTC 通道，如果没有歌词，先临时选择“Windows 音乐会话”。能显示说明需要核对客户端会话 ID；仍无歌曲时查看状态消息和日志：`No Windows media sessions (SMTC)` 表示系统没有提供媒体会话，单纯更换歌词源或改成原生 DLL 不会生成缺失的播放信息。确认播放器提供系统媒体控件，且游戏与播放器运行在同一个 Windows 用户会话中。状态文件的 `availableSources` 与日志的 `SMTC source=... sessions=...` 会列出全部检测到的会话 ID。

诊断文件与配置在同一目录：`media-bridge.log`、`media-state-游戏PID.json`、`media-request-游戏PID.json`。主插件日志仍为 bin/neko/plugins/L4D2DangoMusic.log。日志只记录来源、歌曲和搜索状态，主插件菜单的音乐来源页也显示连接状态。

## 从源码构建

```bash
bash scripts/test.sh
dotnet run --project tests/media-bridge/MediaBridgeTests.csproj -c Release
DANGO_CXX=i686-w64-mingw32-g++-posix bash scripts/build-mingw.sh
dotnet publish media-bridge/DangoMediaBridge.csproj -c Release -o build/media-bridge
python3 scripts/package.py --variant music-platforms
```

使用 .NET 8 SDK。Windows MSVC 的 scripts/build.ps1 已增加桥接构建；Release CI 同样构建并打包桥接。安装包仍只有 bin 和 README.md，新增桥接 EXE 位于插件配置子目录；不包含源码、测试程序、SDK 或游戏/音乐资源。
