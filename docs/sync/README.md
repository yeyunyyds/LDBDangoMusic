# 自动发现与 LX 一起听

自正式版 v0.9.6 起提供，v0.9.7 修正默认关闭与诊断隔离，合入网易云原生歌词；联机功能仍未经过真实多账号实测，不保证 VAC 安全。请先阅读[免责声明](../../DISCLAIMER.md)。

开发分支：`experiment/steam-progress-sync`；正式源码：`main`。

## 使用

见[安装包说明](../../packaging/README-combined.md)。菜单“更多设置 → 一起听与播放控制”提供 LX 控制、一起听、同局发现和 Steam 测试。两端更新同一版本，开启 LX OpenAPI，加入同一局，开启一起听即可；无需填写 SteamID。

音乐来源仅显示自动、LX、网易云。网易云原生通道只显示歌词；一起听、搜索和反向控制严格限于当前实际使用的 LX。切换来源会取消未发送的旧控制；返回 LX 后重新握手和校时。

v0.9.7 默认：同局发现关、一起听关、自动搜索关、进度应用关。关闭总开关阻断全部插件网络路径，不能由发现开关、诊断回调或旧队列绕过。运行中关闭会清理一次已有通道后停止。菜单开启一起听时同时打开进度应用，可单独关闭应用作 DRY RUN。设置须主动保存。

```json
"steamSync": {
  "optInVersion": 1,
  "enabled": false,
  "discover": false,
  "autoSearch": false,
  "apply": false,
  "thresholdMs": 350
}
```

旧 role/host/peers 字段仅兼容读取，不授予任何权限。缺少 optInVersion=1 的旧配置会撤销所有联机开关，必须重新主动开启并保存。

## 同局发现与选主

Source 相机回调在游戏线程每秒获取当前游戏玩家名单，排除 bot、HLTV 和本机，最多三个对端。GetPlayerInfo、GetLocalPlayer、GetMaxClients 均按已提供的 engine.dll 静态核对 RVA、代码字节和结构布局；不依赖网上不同 SDK 的槽位猜测。未知游戏构建拒绝调用。名单快照通过原子共享指针传给工作线程；Steam 收发和音乐网络操作在工作线程执行。

发现使用独立通道 45630，进度使用 45631；发送和接收都限定当前名单。双向 incarnation challenge 验证插件，拒绝旧进程、重放和名单外发送者。名单、地图代次或本机身份改变会重建会话。没有响应只表示“未响应或版本不兼容”，不能证明对方没安装。

用户主动开启总开关与发现后，每个候选最多八次退避探测，可手动重新扫描；确认后每秒 presence，七秒失联撤回资格。只有已确认、一起听已开、当前实际 LX 的参与者可成为房主。

无房主时按游戏玩家顺序自动分配。手动“我来当房主”发布递增逻辑任期，已观察到前一次声明后的新声明覆盖前一次；同时声明按 SteamID 确定性打破平局。声明直发所有已确认对象，presence 重发。切源、关闭一起听或失联后可自动重选。分区期间无法保证所有机器瞬时一致，恢复通信后收敛；房主变化立即重建进度握手，旧会话不能继续控制。

## 搜索、选曲及进度

LX 官方 HTTP OpenAPI 负责 /status、/play、/pause 和 /seek?offset=秒数。停止是暂停后归零；每一步前重新检查当前歌曲，回读验证而非仅看 HTTP 200。

官方 [Scheme 文档](https://lxmusic.toside.cn/desktop/scheme-url) 的 music/searchPlay 接受 URL 编码 JSON：name、singer、interval（mm:ss）、playLater=false。实现按 LX 源码 ad95d5091c9ed689fa72b5e5c849df65f5a679ce 核对；使用 ShellExecuteW 调用已注册的固定协议，不执行任意 URI 或命令。

房主曲目稳定 750ms 后，跟随端根据歌名、歌手、时长请求本机 LX 搜索并播放。LX 自行选择首个候选，没有搜索结果回调；系统接受 URI 只代表已交给播放器。插件回读实际状态，标题/歌手归一化匹配且时长差不超过 1500ms 才允许进度控制。错误版本不会被跳转至房主时间，但 Scheme 已播放的候选无法在发出前验证或撤销。

等待实际选曲最长 20 秒，失败冷却 30 秒，每首最多三次；手动重试可复位。暂停的房主不会触发搜索播放不同曲目。源、房主或成员代次变化会取消未发送的自动操作；已发给系统的 Scheme 无法取消。

房主每 500ms 发布元数据和状态；每两秒做四时间戳校时，不假定两台机器 uptime 相同。默认漂移阈值 350ms，校正最短间隔 1200ms。状态超过三秒、校时超过十五秒或歌曲不匹配时停止写入。播放倍率不一致时提示，不擅自改速度。同步播放器进度，不保证声卡输出采样级一致。

进度协议 v3 / source=lx，包含游戏名单 scope 和独立会话 nonce。两端须一起升级。网络仅传元数据、进度和状态，不发音乐、歌词全文、资源地址或凭据。

## 测试

Steam 自指、SocketPair 和 LX 写操作已由用户日志验证；不再调用 CreateLocalUser、CreateSteamPipe、ReleaseUser、BReleaseSteamPipe，也不接管游戏 Steam 回调队列。

`tests/auto_listening_test.cpp` 覆盖名单布局、bot、授权范围、自动发现、后声明覆盖、并发选主、退出、重启、伪造来源、Scheme 编码和实际选曲验证。`steam_progress_test.cpp` 覆盖进度、时钟差、会话与来源。云端无法替代双 Steam 账号游戏联调，新增自动功能仍需实测。

```bash
bash scripts/test.sh
DANGO_CXX=i686-w64-mingw32-g++-posix bash scripts/build-mingw.sh
python3 scripts/package.py --variant auto-listening
```

Windows MSVC 的 scripts/build.ps1 还运行 x86 ABI mocks。可复用模块见 [API](api.md)，底层历史核对记录见 [validation](validation.md)。

## v0.9.7 独立诊断构建

默认构建 DANGO_DIAGNOSTICS=0，在编译期移除 Steam 自指/SocketPair、LX 控制测试、接口枚举审计、相机矩阵导出和实体 JSONL 导出。生产 DLL 拒绝遗留诊断回调，忽略旧诊断配置，普通运行错误日志保留。

需要诊断时单独构建：`DANGO_DIAGNOSTICS=1 bash scripts/build-mingw.sh`，输出 build/diagnostics/L4D2DangoMusic.dll；Windows 使用 scripts/build.ps1 -Diagnostics。`python3 scripts/package.py --variant diagnostics` 打包。仅替换加载，不与生产 DLL 同时启用。网络测试也必须先主动开启一起听总开关。打包器检查独立构建标识，拒绝将诊断 DLL 包装成正式安装包。

network_isolation_test 使用计数传输验证：存在有效玩家名单、旧开关和旧队列时，关闭状态的绑定/收发/轮询/音乐写控制均为零；开启后撤销仅清理一次，随后保持零调用；来源变化或即时权限撤销也会阻断。
