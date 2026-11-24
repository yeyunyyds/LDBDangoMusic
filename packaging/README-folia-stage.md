# L4D2DangoMusic @VERSION@ — Native Folia / AFK 运镜实验版

**实验版，未发布正式 Release。** 只有一个 L4D2DangoMusic.dll 和配置；不需要 DangoMediaBridge.exe、.NET 或其他额外程序。需要 L4N。配套运镜另需 L4D2Visual ABI v3。

## Folia
1. Folia 桌面版「设置 → 连接与集成」启用 Stage Mode，选择 Stage API；另外启用歌词接口。
2. 保持普通歌曲播放界面，不进入外部舞台会话。DangoMusic 音乐来源选择 Folia（Stage API）。
3. 插件自动读取 %APPDATA% 下 Folia 的 stage/connection.json，只访问 127.0.0.1 的播放状态与歌词 GET 接口。Token 不写进配置或日志。
4. 保留旧 config.json 即可升级；此前的 DangoMediaBridge.exe 不再需要，可以删除。

## AFK 运镜
配套 Visual ABI v3 DLL，在 Visual 的 FreeCamera 设置允许外部接口。DangoMusic → 系统与恢复 → FreeCamera 闲置运镜：选择风格、设置首次等待秒数、启用 AFK 运镜并关闭菜单。等待期间保持原生游戏，闲置达到阈值才进入自由相机。键鼠接管时保持相机原位，等待 Visual 配置的交还时间后继续。F6/菜单关闭 FreeCamera 会结束本轮运镜，需手动重新启用。

可选：混合、人物环绕、建筑歌词、场景远景、鸟瞰追踪。模型定位来自经验证的本地网络实体；镜头段落从当前真实相机位置出发，按人物最新位置重新规划。静态世界避障无法保证所有动态实体、移动门或特殊几何。没有合适场景时缩短镜头或原地构图；主体丢失时不盲目飞行。建筑歌词只在有效墙面、可见性和构图检查通过时绘制，普通歌词与已有手动跟随仍可使用。首次闲置 1–120 秒，默认10；全局与独立挂画退场支持 0 ms。

仅离线实机验证建议；尚未验证 Folia 真实播放或新运镜在各地图上的效果。源码：@SOURCE_URL@

@DISCLAIMER@
