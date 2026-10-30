# 实体挂载实验归档

本实验按用户决定整理保存，停止推进，不计划合入正式版。保留分支 `experiment/entity-attachments`，不删除源码、测试、静态调查证据或实验安装包。正式版、zip 分支和 Release 不因本次归档变化。

## 保存位置

- [最终 build 6 安装包](https://github.com/yeyunyyds/LDBDangoMusic/raw/62cb38c42e58967b72f75fb295ca6eb61cdac92c/artifacts/L4D2DangoMusic-entity-diagnostics.zip)：仅 README 和 bin；升级保留原配置。包内二进制不因文档归档重新构建。
- 对应源码提交：[`3a8730b`](https://github.com/yeyunyyds/LDBDangoMusic/tree/3a8730b)。安装包保存提交：`62cb38c`。
- 安装包 SHA-256：`945259b44a82ab3e9656091be0996bc7a62d8f4fd0ab50af0310fc268833e9a1`。
- [诊断及历史验证步骤](entity-inventory-test.md)、[统一场景接口设计](scene-attachments.md)。

## 实际完成与验证边界

| 内容 | 保存时状态 |
| --- | --- |
| 网络实体采集 | 真实日志已读出类名、完整句柄、网络属性及关系；实体表槽位修正有实际 DLL 离线重放证据 |
| 原生 HUD、实体跟随 Demo | 用户确认能够跟随部分 NPC/玩家模型、掉落武器及物品 |
| build 5 感染者/车辆筛选 | 已补齐普通感染者、特感和专用车辆实体类；离线回归及 Windows x86 交叉编译通过，未收到对应实机验收 |
| build 6 武器/物品补齐 | 精确匹配 40 类拾取物/刷新实体；补读武器归属并处理拾取后隐藏；离线回归、内存检查与交叉编译通过，未收到对应实机验收 |
| 统一场景接口 | 值类型接口和模拟测试原型，尚未连接真实渲染姿态生产者 |

跟随 Demo 默认关闭，仅锁定一个目标，设置与目标只在当前会话有效。文字按实体网络姿态与局部偏移定位，面向相机；完整句柄和地图 epoch 防止删除、索引复用或切图后串到其他对象。目标失效、休眠、快照过期或无法解析父级时隐藏。定时诊断默认关闭，主日志和 JSONL 位于同一插件数据目录。

## 开发者调用入口

当前只有同一插件内的 C++ 内部调用，没有可供其他插件加载的公共 SDK 或跨 DLL 实体接口。

1. 已有游戏线程回调调用 `source_entities::onFrame()`，发布不可变值快照；其他线程不直接访问游戏对象。
2. 选择阶段通过 `browseRequested` 开启全量候选采集，等待快照；读取 `source_entities::snapshot`，结合 `follow::candidates()` 或自定义策略选定完整句柄与 epoch。结束选择后关闭全量候选采集。
3. 在 `plugin.cpp` 内持有 `settingsMutex`，复制 `followSettings`，设 `enabled=true`、`sample=false`、`target={true, epoch, fullHandle}` 和局部 `offset`，通过 `setFollow()` 提交。该函数更新目标请求和 revision，现有绘制会清理旧目标内容。
4. 采集器约每 33ms 更新选中目标及父链；绘制读取 `followSnapshot`，调用 `follow::resolve()`，成功后用 `follow::anchor()` 得到世界位置，交给现有歌词投影和动画。
5. 关闭时通过同一路径提交 `enabled=false`；不要从其他 DLL 包含这些头文件后假定能共享本插件的内部状态。

具体实现：[`source_entities.hpp`](../src/source_entities.hpp)、[`entity_follow.hpp`](../src/entity_follow.hpp)、[`plugin.cpp`](../src/plugin.cpp)。`scene_attachment.hpp` 中的骨骼/附件/表面接口属于设计原型，不能作为已经可用的真实游戏后端调用。

## 未完成项

- 静态地图道具和静态车辆枚举。
- 精确头骨、手部附件、第一人称 viewmodel 挂载及其投影。
- 插值后的渲染姿态、动态物体遮挡、车窗/车厢局部平面采集。
- 模型名查询在接口被其他模块改写时不可用；继续保留校验，不绕过。
- 多目标挂载管理及带版本号的跨 DLL 公共 ABI。

这些内容作为研究记录保留，不列入正式版计划。本次归档仅更新说明，不改功能、不创建新版本或 Release。
