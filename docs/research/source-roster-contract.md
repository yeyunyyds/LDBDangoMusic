# 当前 L4D2 玩家名单调用契约

自动一起听沿用 vendor/source-binary-profile.json 中已核对的 Source 相机游戏构建；此处仅记录对用户提供的 engine.dll 的静态检查，不发布游戏二进制。

PE TimeDateStamp 1782845142，SizeOfImage 7598080，VEngineClient013。

| 方法 | 表槽 | RVA | 前 12 字节 |
| --- | --- | --- | --- |
| GetPlayerInfo | 8 | 0x5f4d0 | 55 8b ec 56 8b 75 08 4e e8 c3 37 02 |
| GetLocalPlayer | 12 | 0x5d030 | 6a ff e8 39 5c 02 00 8b 80 d8 00 00 |
| GetMaxClients | 21 | 0x5d290 | e8 0b 5a 02 00 8b 80 58 01 00 00 c3 |

GetPlayerInfo 的复制与清零路径均使用 0x90 字节。name 在 +8、friendsName 在 +0x54，均按 0x20 字节处理；GetPlayerForUserID 使用 +0x28。因此不可套用其他 SDK 中 128 字节名字的 player_info_t。

当前 144 字节输出：XUID +0（8 字节）、name +8（32）、userID +40、GUID +44（33）、friendsID +80、friendsName +84（32）、fakeplayer +116、HLTV +117、自定义文件 +120、下载计数 +136。调用前后设置各 16 字节 canary，检查边界；纯解析测试不调用游戏代码。

GetLocalPlayer 访问 split-screen 状态 +0xd8 再加一；GetMaxClients 访问 ClientState +0x158，槽位是 21，并非某些 SDK 中的 20。

src/source_roster.hpp 在已有验证相机的游戏线程采集，先验证 PE 和函数字节，再调用上述原始入口。未知构建拒绝调用，不扫描并尝试其他槽位。跨地图、离开游戏、名单变化均更换采集代次；工作线程只读取不可变快照，五秒过期。

名单只保留本机与至多三个当前真人 SteamID，过滤 fakeplayer/HLTV。XUID 与 friendsID 同时存在时必须一致；只有 XUID 无效而 friendsID 有效时按 public individual account 重建 SteamID64。Steam 传输绑定后还会验证本机身份一致。协议名单 scope、双向插件确认和独立 nonce 继续限制会话。

该契约的静态核对与交叉编译不代表新自动功能已完成游戏内多账号验收。
