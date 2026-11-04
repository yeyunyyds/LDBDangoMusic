# Windows 音乐会话与多平台歌词

本功能在 `feature/multi-platform-lyrics` 分支提供，基于正式版 v0.9.5；暂不改动正式版 Release。歌词获取参考 [Epi-Lo/desktop-lyric](https://github.com/Epi-Lo/desktop-lyric)，保留其 MIT 声明。

## 安装与验证网易云

1. 退出游戏，将本分支测试安装包内的 bin 合并到游戏根目录。升级时保留自己的 config.json，同时替换 DLL 和 DangoMediaBridge.exe。
2. 使用 Windows 10 2004 或更新的 64 位 Windows。桥接程序已自带 .NET 运行时，不需要额外安装环境，也不需要手工启动。
3. 打开网易云客户端并播放歌曲；先确认 Windows 系统媒体控件可以看到这首歌及播放进度。未提供 SMTC 的客户端版本无法通过本方式读取播放状态。
4. 游戏菜单 → 显示与歌词信息 → 音乐来源 → 网易云音乐。该选择会退出离线 demo 模式；点击主菜单保存设置后，下次仍使用这个来源。
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
| 网易云 | 歌名/歌手检索，LRC、原生翻译、接口返回时的 YRC 逐字时间 | 在线搜索/歌词接口探测、模拟响应解析；待用户验证 Windows 网易云到游戏的完整链路 |
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
