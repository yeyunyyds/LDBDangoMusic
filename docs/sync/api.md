# 可复用模块 API

## 分层

- `src/sync/protocol.hpp`：消息数据结构、严格编码/校验、曲目匹配、NTP 式 Clock、纯函数 plan、测试内存总线。
- `src/sync/session.hpp`：房主/跟随者状态机，握手、会话 nonce、单调序号、允许列表、过期处理。依赖抽象 Transport，不依赖 Windows、Steam 或播放器。
- `src/sync/steam_transport.hpp`：Windows x86 Steam 适配，复用游戏已初始化的 Steam user/pipe；专用通道默认 45631，可通过构造函数选择其他 >=1024 的通道。只关闭自己使用的通道/本地连接，不关闭游戏 P2P 会话、不释放借用的游戏 user/pipe。
- `src/sync/lx_control.hpp`：纯 C++ 控制命令/请求路径、回读状态机，HTTP 和时钟由调用者注入。
- `src/sync/runtime.hpp`：线程安全请求队列、Steam 轮询、进度计划到 LX 操作的组合；由插件现有后台线程驱动。
- `src/sync/settings.hpp`：参与、发现、搜索与进度应用设置；旧角色字段仅作兼容迁移。
- `src/sync/roster.hpp` / `src/source_roster.hpp`：纯名单契约及已核对游戏构建的游戏线程采集。
- `src/sync/discovery.hpp`：双向插件发现、参与资格、逻辑任期选主，依赖抽象 Transport。
- `src/sync/song_switch.hpp`：固定 LX Scheme 构造、选曲验证、退避重试。

## 任意底层传输

```cpp
class YourTransport : public dango::sync::Transport {
public:
  dango::sync::Peer self() const override;
  bool send(dango::sync::Peer peer, const std::string& payload) override;
  bool receive(dango::sync::Packet& packet) override;
};
```

receive 必须提供传输层确认的真实来源，不能直接信任 JSON 的 sender。Steam 适配使用 ReadP2PPacket 的 SteamID；编解码器要求它与包内 sender 一致。内存总线仅用于测试，不具备 Steam 身份认证。

Steam 传输的正常收发本身不要求使用歌词协议，其他模块可选独立 channel 实现自己的数据格式；本模块的控制协议保留独立 app/version 命名空间。每个 channel 只交给一个消费者，避免多个模块争抢接收队列。

## 歌曲进度会话

```cpp
// transport 已连接，hostId / allowed 来自同局发现及选主。
dango::sync::Session follower(
    transport, dango::sync::Role::Follower, allowed, hostId, randomNonce, roster.cohort());
follower.tick(localPlayerState, monotonicMilliseconds);
auto correction = follower.correction(localPlayerState, monotonicMilliseconds);
// 只有 correction.valid 后才考虑 seek/play/pause。
```

房主使用 Role::Host，authority 参数为 0。Session 不执行播放器控制，发送/收到的也只有元数据；调用者自行决定是否采纳 plan。一起听默认关闭，菜单开启时同时开启进度应用；可单独关闭应用以观察 DRY RUN。

收到新状态、关闭同步、房主改变、曲目改变时，执行前再次检查最新计划和本机歌曲，取消不再适用的未发送自动命令。网络模块没有运行任意函数、访问 URL 或执行收到的播放器端点的功能。

## LX 回调测试

```cpp
dango::lx::Command cmd;
cmd.id = requestId;
cmd.action = dango::lx::Action::Seek;
cmd.position = 30125;                    // 毫秒，转换为 /seek?offset=30.125
cmd.expectedTrack = freshLocalState.key;
auto ack = executor.begin(cmd, freshLocalState, now, httpRequest, monotonicClock);
// 后续每次拿到新的播放器状态：
auto result = executor.observe(freshLocalState, now);
```

ack.accepted 表示 HTTP 请求应答成功，result.verified 才表示状态已验证。不把远端 ACK 或 HTTP 200 当成音频已经对齐。停止由固定的 Pause+Seek(0) 构成；每步前重新读本机状态，不接受远端字符串作为 HTTP 路径。命令执行限制在 127.0.0.1 的配置端口。

UI 回调通过 Requests.push 提交，返回值只表示队列有容量，不等于音乐已受控。实际执行结果由 Status.control 与日志给出；这些请求/回读在后台线程运行，不在 D3D9 渲染或 L4N 菜单回调中执行。


## 当前来源与排队请求

Runtime.tick 接收 SourceContext（provider、revision、epoch）、Roster 快照及可选固定 Scheme 调度回调；默认上下文不给予 LX 控制权限。集成层必须按实际使用的播放器填入，不能仅根据“本机安装了 LX”判断。Requests 的本机控制命令也要携带 sourceRevision/sourceEpoch。来源切换、自动回退、断线或旧命令代次不符时，丢弃命令、取消回读并重建同步会话。

仅 SourceContext::LX 可以启用 Session 与执行控制，网易云/Windows/示例上下文不初始化进度会话或访问 LX 写 API；来源无关的同局插件发现仍可运行，只公告不具备 LX 参与资格。实际集成在每次 HTTP 操作前再次比较不可变配置的 playbackSourceRevision，避免切换来源与后台执行之间的竞争。此运行时字段不写入用户配置文件。

## 自动房间与切歌

`Discovery(transport, roster, nonce, now)` 使用专用发现传输；每轮 `tick(enabled, actualLX, now)`，`leader()` 提供 id/term/boot，`participants()` 仅返回当前已确认且具备 LX 资格的对端。描述符、名单或来源代次变化时重建 Session。`claim(now)` 和 `rescan(now)` 对应用户菜单操作。

`Session::remote(now)` 只返回新鲜且已校时的房主状态；`SongSwitch::tick(track, playing, local, now, dispatch)` 对不同曲目构造受限 URI，通过注入调度器交给 LX。只有后续本机歌曲实际匹配时，Session::correction 才会提供有效进度计划。调用者必须对调度器再次执行来源检查，不能让远端指定任意 URI。
