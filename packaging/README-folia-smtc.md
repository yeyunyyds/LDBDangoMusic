# L4D2DangoMusic @VERSION@ — Folia SMTC 歌词实验包

**实验分支，非正式 Release。未完成 Folia + L4D2 实机验证。** 基于 v0.9.8 的歌词显示系统，新增 Folia 桌面播放器适配。原有 LX / 网易云路径不变，Steam P2P 一起听仍然仅限 LX，默认关闭。安装该实验包需 Windows 10 2004+ x64、L4N 和 Folia 桌面版；LDB / DXVK 不是运行依赖。

## 方案 A：SMTC + Folia 歌词接口

- **歌曲、歌手、暂停状态、进度**：通过 Windows SMTC 读取 Folia 系统媒体会话（由独立的 x64 `DangoMediaBridge.exe` 读取）。
- **逐行 / 逐字歌词、翻译**：请求 Folia 本机只读 `http://127.0.0.1:32109/v1/lyric`；不访问 Stage API，不需要 token。
- **歌词时间**：将 Folia 秒转换为内部毫秒时间，并按照官方规则处理 `offset` 与 `clipStartMs`。仅当 Folia `wordByWord=true` 才使用其逐字时间戳；合成的逐字时间继续由 DangoMusic 的“无精确逐字时间”外观设置处理。
- **状态隔离**：只有明确选中 Folia 来源时才匹配 Folia SMTC 会话并访问其歌词端口；不对 Folia 发送播放控制、不注入其进程、不使用 Steam / 网络端口转发。非 Folia 来源维持既有运行方式。

## 安装

1. 完全退出 L4D2，将本压缩包的 `bin` 合并至游戏根目录。确保 L4N 已安装。
2. 必须同时安装以下文件：
   - `bin/neko/plugins/L4D2DangoMusic.dll`
   - `bin/neko/plugins/L4D2DangoMusic/DangoMediaBridge.exe`
   - `bin/neko/plugins/L4D2DangoMusic/config.json`
3. **升级保留自己已有的 config.json**；在 L4N 菜单「音乐与歌曲信息 → 音乐来源」选择「Folia（SMTC + 歌词 API）」并保存。首次安装本实验包默认选择 Folia。
4. 在 Folia 打开「设置 → 连接与集成 → 歌词接口 → 启用歌词接口」，确认 `http://127.0.0.1:32109/v1/lyric` 可访问。
5. 播放歌曲，确认 Windows 系统媒体控件显示 Folia 的歌曲信息和播放位置。然后在 L4D2 中检查歌词与暂停、切歌、进度变化。

桥接进程由插件按需启动，并在游戏退出时结束。桥接包内包含自带 .NET 8 运行时，无需额外安装；不包含 Folia 或歌曲音源。只提供单 DLL 的正式版安装包**无法使用本实验来源**。

## 排查

- **No matching player / No Windows media sessions (SMTC)**：请先确保 Folia 桌面版提供系统媒体会话，并与游戏运行在同一 Windows 用户会话中；部分独占 HiFi 模式可能不提供 Chromium SMTC。日志 `media-bridge.log` 中列出实际 SMTC SourceAppUserModelId，可用于后续调整匹配规则。
- **Folia lyric API unavailable; enable port 32109**：检查歌词接口开关、端口是否被占用；通过浏览器或 PowerShell 测试本机 `GET /v1/lyric`。
- **Folia lyrics unavailable or still loading**：接口返回 JSON `null`、正在加载、未取得歌词，或歌词标题/作者与 SMTC 不匹配。不会沿用上一首歌词。
- **出现多份插件**：删除旧版或重命名的 DLL 副本，确保 L4N 只加载一份。
- **一起听**：Folia 不参与现有 LX 一起听；当前实现没有 Folia 播放控制。

主日志：`bin/neko/plugins/L4D2DangoMusic/L4D2DangoMusic.log`。桥接日志：`bin/neko/plugins/L4D2DangoMusic/media-bridge.log`。

## 验证与限制

已增加解析 / 歌词相位 / 切换防串歌的单元测试；Windows SMTC 与 Folia 的实机联调仍待进行。存在 Folia 或 Windows 媒体会话不向系统发布准确播放进度的兼容性风险。无原生逐字时间时，歌词效果受本项目显示设置影响。

## 源码与许可

对应完整源码：@SOURCE_URL@

项目主页：https://github.com/yeyunyyds/LDBDangoMusic

Folia API 参考：https://github.com/chthollyphile/folia-major/blob/main/docs/lyric-api.md

第三方来源、许可和项目安全免责声明在后文附录。Folia 源码未嵌入或修改，独立调用其公开本机接口。

@DISCLAIMER@

## 合并实验分支中的闲置运镜

若本包来自 feature/visual-freecamera-abi-client，已同时包含 FreeCamera ABI v2 客户端和闲置运镜。搭配 Visual 同步实验版 DLL（ABI v2），先授权外部接口，再从 DangoMusic「系统与恢复 → FreeCamera 闲置运镜（实验）」启动。F6 或 Visual 菜单关闭会结束当前会话；重新启动才恢复。Folia 音乐来源与运镜均可独立使用。不需要安装 ABI smoke EXE。仅离线测试，实际镜头与 Folia SMTC 兼容性尚待验证。
