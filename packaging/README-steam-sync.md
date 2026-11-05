# L4D2DangoMusic 0.9.5 · Steam 进度同步实验

基于正式版 v0.9.5 的独立实验分支。只共享歌曲进度与播放状态，不分发音乐、歌词全文、下载链接或账号信息；两端各自在自己的落雪音乐里选择同一版本歌曲。

## 安装

退出游戏，合并 bin 到游戏根目录；升级仅替换 bin/neko/plugins/L4D2DangoMusic.dll，保留自己的 config.json。只加载一份歌词 DLL。无需额外运行时或下载 Steam DLL，Steam 接口来自游戏已加载的版本。原网易云原生读取实验不包含在本分支中。

## 测试入口

更多设置 → 落雪控制与 Steam 同步实验：

- 落雪 API 控制回调测试：播放、暂停、停止（暂停后归零）、跳转 0 秒/30 秒/后移 10 秒/歌曲中间。要求 LX 官方桌面版开启 OpenAPI，默认 127.0.0.1:23330。HTTP accepted 仅表示接收，Player state verified 才表示状态验证通过。
- 共享歌曲进度：角色关闭/房主/跟随者，应用收到的进度默认关闭，显示 SteamID、RTT 和漂移。
- Steam 自指与本地创建测试：绑定游戏 Steam 接口、真实 SteamID 自指、CreateLocalUser 禁用提示、真实 Steam CreateSocketPair 本地双端收发。另有无需其他账号的“虚拟多玩家模拟”按钮（内存模拟，不冒充 Steam 实测）。这些测试不会控制音乐。

## 配置允许玩家

配置文件的 steamSync 默认如下。退出游戏后编辑，ID 为十进制字符串。

```json
"steamSync": {"role": 0, "apply": false, "thresholdMs": 350, "host": "", "peers": []}
```

先从绑定检测菜单获得真实 SteamID64。房主 role=1，peers 列出允许跟随者；跟随者 role=2，host 填房主 ID，peers 包含房主。最多 8 个对端。两端各自选好同一版本歌曲，先保持 apply=false 观察握手/校时和预测漂移，确认后跟随端开启“应用收到的进度”，返回主菜单保存设置。

配置中的角色和远端应用默认关闭。专用 Steam 通道 45631，每 500ms 发布一次状态、每 2 秒校时；默认漂移超过 350ms 才校正。曲目不匹配、倍率不一致、校时/状态过期时不控制本机；不会自动切歌或选新房主。网络中断后停止校正，不强制停止本地音乐。

## 验证范围

协议模拟与本机 HTTP 控制 fixture 已提供；Windows x86 构建检查包括 Steam 返回 ABI mock。用户日志已确认 LX 控制回读和真实 SteamID 自指成功（469ms）；SocketPair 和跨账号联机仍需验收，不能把模拟通过当作 Steam 实测通过。CreateLocalUser 游戏内禁用，不再测试。

旧版 User 探测曾导致游戏崩溃：SDK 将 CreateLocalUser 及相关释放标记为面向服务器/非线程安全操作。修正版彻底移除游戏内调用，原入口只显示 BLOCKED，不再触碰 Steam 用户/pipe 生命周期。CreateSocketPair 创建本地双端连接；游戏未暴露所需接口时显示 UNSUPPORTED。自指发送被 Steam 拒绝或没有回包时显示 rejected/TIMEOUT，不伪造 PASS。

日志为 bin/neko/plugins/L4D2DangoMusic.log，包含 Steam 绑定信息及 LX 请求/状态回读结果。实际测试失败时请附日志和对应菜单状态。

完整设计、配置步骤、模块复用 API 与测试说明：@SOURCE_URL@/docs/sync
对应源码：@SOURCE_URL@
