# 契约、测试边界与验收

## LX 契约依据

核对 lyswhut/lx-music-desktop `ad95d5091c9ed689fa72b5e5c849df65f5a679ce`，`src/main/modules/openApi/index.ts`。

- /play、/pause、/seek 返回纯文本 OK，不是 JSON。
- /seek 的 offset 单位是秒，保留三位小数；API 检查 0 <= offset <= duration。
- 没有 /stop；不发明不存在的端点。
- 兼容只读 /status 和 /lyric-all 的第三方播放器不一定实现控制 API，须分别验收。

## Steam ABI 依据

核对 ValveSoftware/source-sdk-2013 的 public/steam 公开接口声明及 ValveSoftware/GameNetworkingSockets 的 include/steam 公共定义；项目不附带 Steam SDK、steam_api.dll、steamclient.dll 或账号凭据。

- 游戏必须已经初始化 SteamAPI。仅取得已有 steam_api.dll 的工厂和现有 user/pipe 句柄，不调用 Init/Shutdown。
- SteamClient017..020：GetISteamUser 槽 5、GetISteamGenericInterface 槽 12、GetISteamNetworking 槽 16；CreateLocalUser 槽 3、ReleaseUser 槽 4、BReleaseSteamPipe 槽 1 只用于本模块显式创建的测试句柄。
- SteamNetworking005/006 前六项：SendP2PPacket、IsP2PPacketAvailable、ReadP2PPacket、AcceptP2PSessionWithUser、CloseP2PSessionWithUser、CloseP2PChannelWithUser。本模块不调用槽 4，不关闭游戏会话，仅使用槽 5 清理专用 channel。
- SteamUser017..023 前三项固定；优先使用 flat GetSteamID 导出，老版本用显式 x86 返回 thunk 兼容 EDX:EAX 与隐藏返回存储，并恢复调用栈。不会让 GCC 隐式猜测 MSVC 的 CSteamID 类返回规则。
- SteamNetworkingSockets012：CreateSocketPair 槽 19、SendMessageToConnection 槽 11、ReceiveMessagesOnConnection 槽 14、CloseConnection 槽 5。缺失此版本就报告 UNSUPPORTED，不扫描未知槽。
- Windows x86 SteamNetworkingMessage 前缀的 connection user data 偏移 152、release 回调偏移 180，编译期检查。读取数据有大小与地址检查，按 SDK release 释放收到的消息。

## 本地自动回归

- steam_progress_test：协议、虚拟多端握手、四时间戳校时（不同 OS uptime）、漂移/暂停、空闲死区、错误歌曲、心跳/时钟过期、重复包、伪造 sender、非法/多余字段、默认关闭与配置序列化、自指内存总线。
- test_lx_control_http.py + lx_control_driver：真实 loopback HTTP fixture 驱动生产 C++ Executor，验证播放/暂停、30.125 秒跳转、暂停归零，以及“返回 OK 但播放器不变”必须超时而非成功。
- steam_abi_test.exe：Windows x86 mock 方法分别使用寄存器返回与隐藏返回指针，循环检查 SteamID 和栈平衡；跨编译只能验证构建，真正运行需 Windows。
- 既有核心、歌词、动画、菜单、实体和挂画测试仍保留。新菜单导航与操作分别注册，设置/控制不会叠加返回层。

## 必须实际运行的 Steam 测试

真实 Steam-ID 自指、CreateLocalUser、Steam CreateSocketPair、两个真实账号跨机器均需要实际 Steam 环境。实现与菜单已经提供；云端没有可用 Steam 登录会话，不能声称已实机通过。

记录实际结果时区分：PASS、失败、超时和接口 UNSUPPORTED。创建本地句柄或模拟 ID 不能证明存在第二个认证玩家；SocketPair 只能证明本地双端通道与进度包收发，不能替代跨账号 NAT/中继验收。

双玩家验收建议：先 DRY RUN，确认相同曲目和稳定 RTT；再测试房主播放、暂停、拖动、归零、正常换歌、客户端退出、网络中断、重连、配置禁用和第三个未允许玩家。换到不同歌曲时，跟随端应停止自动控制，直到用户自己选好匹配曲目。
