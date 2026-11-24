# L4D2DangoMusic @VERSION@ — Native Folia / AFK 运镜实验版

**实验版，未发布正式 Release。** 只有一个 L4D2DangoMusic.dll 和配置；不需要 DangoMediaBridge.exe、.NET 或其他额外程序。需要 L4N。配套运镜另需 L4D2Visual ABI v4。

## Folia
1. Folia 桌面版「设置 → 连接与集成」启用 Stage Mode，选择 Stage API；另外启用歌词接口。
2. 保持普通歌曲播放界面，不进入外部舞台会话。DangoMusic 音乐来源选择 Folia（Stage API）。
3. 插件自动读取 %APPDATA% 下 Folia 的 stage/connection.json，只访问 127.0.0.1 的播放状态与歌词 GET 接口。Token 不写进配置或日志。
4. 保留旧 config.json 即可升级；此前的 DangoMediaBridge.exe 不再需要，可以删除。

## AFK 运镜
配套 Visual ABI v4 DLL，在 Visual 的 FreeCamera 设置允许外部接口。DangoMusic → 系统与恢复 → FreeCamera 闲置运镜：选择风格、设置首次等待秒数、启用 AFK 运镜并关闭菜单。等待期间保持原生游戏，闲置达到阈值才进入自由相机。键鼠接管时保持相机原位，等待 Visual 配置的交还时间后继续。F6/Visual 菜单关闭 FreeCamera 仅退出当前视角，重新等待首次 AFK 时间；DangoMusic 菜单才关闭 AFK 功能。

可选：混合、人物环绕、建筑歌词、场景远景、鸟瞰追踪、墙面运镜。模型定位来自经验证的本地网络实体；镜头段落从当前真实相机位置出发，按人物最新位置重新规划。静态世界避障无法保证所有动态实体、移动门或特殊几何。没有合适场景时缩短镜头或原地构图；Bot 接管后按实体 Handle、唯一角色编号或观察目标恢复跟踪；主体暂失则保持当前外部镜头。建筑歌词通过有限射线探测立面上沿和屋顶，在屋顶之上放置固定朝向文字；墙面运镜使用贴墙平面和低位镜头。无有效构图时回退普通歌词，手动实体跟随仍优先。首次闲置 1–120 秒，默认10；全局与独立挂画退场支持 0 ms。

仅离线实机验证建议；Folia 原生接口已获用户认可，归档于 archive/folia-native-stage-verified；本轮新运镜仍须验证各地图及 Bot 接管的实际效果。源码：@SOURCE_URL@

@DISCLAIMER@
