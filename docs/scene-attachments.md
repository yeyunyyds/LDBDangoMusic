# 场景对象与歌词挂载接口（实验）

分支：`experiment/entity-attachments`。

可以为客户端可用的实体和场景物体建立统一的歌词挂载接口，但不能承诺读取“服务器世界的所有内容”。客户端只拥有当前已加载、已同步的对象；休眠实体的坐标可能过时，未同步的服务器实体不可直接枚举。静态地图、静态模型、动态实体、骨骼、第一人称视图模型也不是同一类数据。

本分支已经实现**不依赖游戏指针的 C++ 接口原型和模拟测试**，并对用户提供的游戏 DLL 做了静态接口调查。**尚未接入 Source 实机采集后端或歌词绘制，不能安装后直接宣称已经跟踪游戏实体。** 当前 `main` 和 `zip` 不受本实验影响。

## 挂载能力与取数路径

| 目标 | 数据路径 | 挂载方式与限制 |
| --- | --- | --- |
| 飞行中的投掷物 | 客户端实体列表、网络类、完整实体句柄、插值后的渲染变换 | 跟随实体位置/旋转；爆炸或删除后解除，不能误挂到复用同一索引的新实体 |
| 队友头顶 | 玩家实体、队伍关系、模型骨骼或命名 attachment | 开发者按模型选择头部骨骼；缺少骨骼数据时明确失败，不悄悄退回脚底原点 |
| gnome 等可携带物体 | 动态道具实体、模型路径、owner/parent 关系 | 不能仅按 `prop_physics` 判断 gnome；落地、被携带和被投掷时跟随同一有效对象或预设的关系 |
| 当前手持物品（世界模型） | 本地玩家 → 活动物品/携带物关系 → 对应对象 | 可按预设选择物体原点、局部偏移或模型挂点 |
| 第一人称手上物品 | 本地玩家 → viewmodel，视图模型骨骼/附件、专用相机 | 单独标记 `ViewModel` 渲染域，不能直接拿世界相机矩阵投影；需要验证预测更新和 viewmodel FOV |
| 静态车辆 | 静态道具管理器、模型名、模型到世界变换 | 常不是网络实体；需要独立的静态道具采集后端 |
| 动态车辆/可移动模型 | 客户端实体、模型变换、碰撞对象 | 跟随完整姿态，不能只给文字中心加一个世界坐标偏移 |
| 车窗、车厢外壁、车厢内部 | 对象表面命中，或开发者按车型提供的局部平面预设 | 同一个物体可以有多个锚点，分别指定位置与朝向；物体移动/旋转时整个文字平面一起移动/旋转 |
| 墙、地面等地图表面 | 已加载 BSP/世界碰撞或专门的表面查询 | 可以固定挂载，但碰撞结果不等于完整渲染网格 |

“车窗”“车厢内部”通常不是引擎直接提供的语义对象。碰撞网格可能简化了车窗，玻璃也可能没有独立碰撞面。对这些目标，**开发者按车型预设局部坐标/局部朝向**是可靠接口；自动识别视觉上的每块车窗则是另外一项模型/材质分析能力。内外两侧可以分别定义朝向相反的平面，不应假设任何车型都含有同名挂点。

粒子、贴花、天空、屏幕空间特效以及服务器未同步内容不能统一当成普通实体。若以后需要完整视觉三角形、材质和遮挡，应提供独立的几何采集能力；实体挂载不要求先读取整张地图的所有三角形。

## 已有的具体证据

静态报告：[scene-interface-evidence.json](research/scene-interface-evidence.json)。

用户提供的 DLL 中找到了注册入口：

- `client.dll`：`VClient016`、`VClientEntityList003`。
- `engine.dll`：`VModelInfoClient004`、`StaticPropMgrClient005`、`EngineTraceClient003`。
- 客户端数据包含 `DT_PipeBombProjectile`、`DT_MolotovProjectile`、`DT_VomitJarProjectile`、`DT_BasePlayer`、`DT_BaseViewModel` 等表名。
- 可找到 `m_hActiveWeapon`、`m_hViewModel`、`m_hOwnerEntity`、`m_vecOrigin`、`m_angRotation` 等属性名。

这些证明对应接口/属性入口存在，**不证明当前游戏构建的所有虚表槽和字段偏移已经验证**。部分接口对象在运行时初始化，文件中不能直接读到最终虚表。报告的 `runtime_dispatch_verified` 为 `false`。

公开 hl2sdk 的 l4d2 分支目前声明 `VClient015`，而用户实际构建为 `VClient016`；不能照抄 SDK 的客户端虚表下标。骨骼、附件和静态道具接口也要分别核对实际 ABI，然后才调用。

SDK 参考与本次所读文件的哈希记录在 [scene-sdk-sources.json](research/scene-sdk-sources.json)。它们作为接口语义参考，没有将游戏 DLL 或第三方 SDK 整包纳入本分支。

静态复核命令（不执行 DLL）：

```bash
python scripts/discover_scene_interfaces.py --client /path/to/client.dll --engine /path/to/engine.dll
```

## 当前 C++ 接口原型

实现：[src/scene_attachment.hpp](../src/scene_attachment.hpp)。目前是供同一插件内使用的 C++ 接口，不是已经导出的跨 DLL ABI。

```text
经过 ABI 验证的 Source 游戏线程采集器（待实现）
  ├─ 客户端实体与关系
  ├─ 静态道具
  ├─ 请求目标的骨骼/附件
  └─ 表面命中 → 所属物体 ID
            ↓
       scene::Exchange（不可变快照）
            ↓
       scene::View
  ├─ enumerate(Filter)
  ├─ related(ObjectId, Relation)
  ├─ resolve(Anchor)
  └─ bindSurface(owner, hitPoint, hitNormal)
            ↓
      挂载预设 → 歌词动画/绘制（待接入）
```

接口原型提供：

- `ObjectId`：地图 epoch、对象种类、完整网络句柄或静态实例标识。网络句柄保留序列号，不把 entity index 当成永久 ID。客户端临时对象需要生产者提供生命周期代号。
- `Frame`：对象列表、模型/类名、队伍、关系、对象变换，以及支持的能力位。它不是“全世界完整快照”的保证。
- `Filter`：按对象类别、网络类名、模型路径片段、队伍筛选，供开发者预设选择。
- `Relation`：Owner、Parent、ActiveWeapon、ViewModel、CarriedObject；采集后端不能确定的关系不应伪造。
- `Anchor`：原点、骨骼、命名附件、局部表面四种挂载方式。预设指定局部变换，决定文字相对位置和朝向。
- `bindSurface`：将命中点和法线构成的世界平面转换为所属物体的局部平面。之后每帧通过对象的新变换还原，支持移动、旋转和缩放。
- `resolve`：返回 Ready、Stale、WrongMap、TargetGone、Dormant、Unsupported、MissingSocket 或 InvalidTransform。缺能力/目标丢失时不会把文字留在过期位置。
- `Domain::World` / `Domain::ViewModel`：明确区分投影域，绘制层必须选择匹配的相机。

例如，对一个已经识别出来的车辆对象：

```cpp
scene::View view(exchange.snapshot(), nowMs);
auto windowAnchor = view.bindSurface(carId, hitPoint, hitNormal);
// 保存 windowAnchor；以后使用新快照解析同一个锚点，而不是每帧重投射。
if (windowAnchor) {
    auto pose = nextView.resolve(*windowAnchor);
    // Ready 时使用 pose.world 的完整平面变换绘制；其他状态隐藏/解除。
}
```

同一辆车可分别创建车窗、车厢内部和外壁的锚点。已知车型的预设也可直接构造 `Anchor.local`，不依赖车窗碰撞命中。文字是否朝向相机、是否继承物体旋转、何时选择目标，属于歌词预设/绘制层策略，而不是强制写死在采集层。

## 接入顺序

1. **只读场景诊断**：核对实体列表与网络类/RecvTable ABI，在已有游戏线程回调中枚举完整句柄、模型、渲染变换、休眠状态和关系；导出诊断数据验证目标出现/消失。
2. **实体原点/局部平面**：先验证 gnome 和投掷物，处理拾取、掉落、销毁、序列号复用和切图。
3. **队友骨骼/附件**：核对实际 renderable ABI，仅对选中目标采集已更新的骨骼/附件，不对全场所有物体强制 SetupBones。
4. **静态道具与车辆**：核对 StaticPropMgr 的枚举/碰撞归属路径；建立模型名 → 局部表面预设。扩展现有只测世界几何的射线后端，不能假设现有 `TRACE_WORLD_ONLY` 已经支持道具。
5. **第一人称手持物品**：单独验证 viewmodel 的更新时序、挂点与投影，之后再接歌词预设。

所有游戏对象访问都应位于经验证的游戏线程采集阶段。绘制/网络线程只能读值快照。对象变换优先使用插值后的渲染姿态，而不是未经插值的网络 origin，避免跟随快速投掷物时抖动。

## 已验证与未验证

`tests/scene_attachment_test.cpp` 已用模拟快照验证：投掷物移动、队友头骨偏移、手持关系、gnome 模型筛选、车辆局部平面随移动/旋转、不可变快照、删除/句柄序列号复用/休眠/切图/过期处理和能力缺失。

这些测试验证接口和坐标运算，**不代表真实 L4D2 实体采集、骨骼 ABI、静态道具枚举或 viewmodel 投影已经跑通**。本分支不更新 `zip` 安装包。
