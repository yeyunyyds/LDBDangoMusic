# L4D2DangoMusic 0.9.5 · LX / 网易云与 Steam 同步合并测试版

保留正式版 v0.9.5 的歌词、动画、世界/屏幕显示、挂画和实体跟随，合入已验证的网易云原生通道与 LX 控制、Steam 进度同步实验。

安装包只有 bin 文件夹和 README：插件 DLL 与默认配置，不包含 .NET、桥接 EXE、Steam DLL、源码或游戏/音乐资源。

## 安装与选择来源

退出游戏，将 bin 合并到游戏根目录；升级仅替换 bin/neko/plugins/L4D2DangoMusic.dll，保留自己的 config.json。只启用一份歌词插件。

显示与歌词信息 → 音乐来源：

- LX 本地 API：落雪音乐开启官方 OpenAPI，默认 127.0.0.1:23330。
- 网易云音乐：原生读取本机 cloudmusic.elog，按歌曲 ID 获取歌词；无需 SMTC、.NET 或第三方客户端插件。原网易云独立分支已获用户实机验证，合并版本仍需复验。
- 自动（LX 优先）：LX API 不可用时尝试网易云原生通道，每 5 秒探测 LX 是否恢复。LX API 仍连接但暂停/无歌曲时保持 LX，可手动切换网易云。

Windows 会话、QQ、酷狗、Spotify、Tidal 等既有桥接通道仍保留为可选扩展，需要另外构建完整 music-platforms 包，本小包不附带该 EXE。

## 一起听与反向控制仅限 LX

更多设置 → 落雪控制与 Steam 同步实验：

- 落雪 API 控制测试：播放、暂停、停止（暂停归零）、跳转。回读实际状态验证，不把 HTTP OK 当成执行成功。
- 共享歌曲进度：角色、允许玩家 SteamID、房主、漂移阈值与远端应用开关；默认关闭，先 DRY RUN，再手动开启应用。
- Steam 通信与本地测试：真实 SteamID 自指、本地 SocketPair、内存多玩家协议模拟。虚拟 User 创建入口已经删除，不再调用游戏内的 Steam 用户/pipe 生命周期 API。

网易云、其他 Windows 来源、离线示例和未连接状态不发布/应用歌曲进度，也不允许反向控制。切换来源会清除未发送的控制请求并停止同步会话，回到 LX 后重新握手和校时。即使后台另一个 LX 客户端也在运行，选网易云时也不会控制它。

## 两玩家联调

两端自己在 LX 选择相同版本的歌曲，先从 Steam 绑定菜单获得真实 SteamID64，再退出游戏编辑 config.json 中的 steamSync：

```json
"steamSync": {"role": 0, "apply": false, "thresholdMs": 350, "host": "", "peers": []}
```

房主 role=1，peers 列出允许的跟随者 ID 字符串；跟随者 role=2，host 为房主 ID，peers 包含房主。两端须安装同一合并版本：协议 v2 明确只允许 LX 进度，拒绝旧实验协议和其他来源。

先保持 apply=false 检查曲目匹配、握手、RTT 和漂移，再在跟随端开启应用并保存。只发送歌曲元数据、播放/暂停、进度和校时信息，不传音频、歌词全文、下载地址或账号资料，不会自动为对方获取或切换音乐资源。

## 验证与日志

用户旧版日志已确认 LX 控制和真实 SteamID 自指成功（RTT 469ms）；网易云原生独立分支也已由用户验证。跨账号一起听、当前合并包和 SocketPair 仍需实机验收。

旧 User 探测曾使游戏无响应/崩溃。CreateLocalUser 和相关释放 API 面向服务器并明确非线程安全，现已完整移除游戏内链路，不要求再次复现。不是通过 try/catch 吞掉崩溃。

日志与配置同目录：bin/neko/plugins/L4D2DangoMusic/L4D2DangoMusic.log。源码构建、完整同步配置、复用 API 和测试记录见下面链接。

对应源码：@SOURCE_URL@
指南：@SOURCE_URL@/docs/sync
