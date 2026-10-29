# 实体采集诊断试用（实验分支）

本轮目标是确认实际游戏里的实体句柄、网络类、模型、网络坐标和关系是否能够正确获取。**不会自动把歌词挂到队友、投掷物或车辆上**，也不替换已经验证的普通歌词/墙面歌词逻辑。诊断默认关闭，不保存开启状态。

## 安装与开启

1. 退出游戏，保留现有 `config.json`，将当前稳定 DLL 备份到插件加载目录之外。
2. 下载本实验分支的 [诊断安装包](https://github.com/yeyunyyds/LDBDangoMusic/raw/refs/heads/experiment/entity-attachments/artifacts/L4D2DangoMusic-entity-diagnostics.zip)。只含 `bin` 和 README，安装方法同普通最小安装包。
3. 替换游戏 `bin/neko/plugins/L4D2DangoMusic.dll`，升级不要覆盖自己的配置。确保没有其他旧版/改名副本同时加载。
4. 进入地图，打开 L4N → L4D2DangoMusic → **实验：实体接口只读诊断**。
5. 开启“定时采集”，然后关闭菜单正常游戏。每 200ms 采集，后台约每秒输出一帧。
6. 测试约 20～30 秒后关闭采集。需要时也可点击“立即导出当前快照”。这个动作只导出已有快照，不在菜单线程访问游戏对象。

输出：

```text
bin/neko/plugins/L4D2DangoMusic/entities.jsonl
bin/neko/plugins/L4D2DangoMusic.log
```

JSONL 是每行一帧的诊断数据，连续开启会不断追加；测试结束请关闭采集。恢复稳定版时退出游戏后换回备份 DLL 即可。

## 建议验证动作

- 观察队友走动、转头，对比玩家实体的类名、模型和坐标。
- 投出土制炸弹、燃烧瓶、胆汁，观察相应 `Projectile` 类出现、移动和销毁。
- 对 gnome 拾取、放下、投掷，查看模型路径和 `owner_handle` 等字段是否变化。
- 切换手上的武器/物品，观察玩家 `active_weapon_handle`、viewmodel 对象和模型变化。
- 结束后提供 `entities.jsonl` 和主日志，以便核对实际构建中尚未支持的 getter/数据表形式。

静态车辆不会保证出现在这一轮的输出中：当前只枚举网络实体，不含静态道具管理器的对象。车辆/车窗局部平面的接口原型仍在，但其采集接入是后续阶段。

## 字段含义

- `handle`：完整 32 位句柄，不只是 entity index。当前构建的静态证据为 12 位索引、20 位序列号。
- `index` / `serial`：诊断拆分值；同一 index 在销毁/重建后必须检查 serial。
- `class` / `class_id`：从经校验的只读 getter 和 ClientClass 元数据解析。
- `model_index` / `model`：数据表中模型索引，以及已验证模型接口返回的模型名。
- `origin` / `angles`：**网络属性原始值**，可能包含休眠对象的旧值，也可能是相对父实体的局部坐标；不能直接当作世界坐标。不是插值后的渲染姿态，不可据此声称快速投掷物或头骨挂载已经准确。
- `owner_handle` / `active_weapon_handle` / `viewmodel_handle`：仅在当前类的数据表中找到对应字段时输出。
- `dormant_candidate`：仅在 getter 可被解释为简单字节读取时给出，语义还需实机对比，不作为正式挂载的有效性保证。
- 未输出的字段表示无法可靠获取，不会使用零值伪造。
- `unknown_class` / `unknown_base` / `unknown_tables`：被保守跳过的数据数量；`getter_hints` 提供未支持的 getter 指令特征，供适配使用。

## 调用边界

- 在现有 Source `View_Render` 原函数返回后的游戏线程回调里采集。
- 实体列表采用当前构建已核对的内存布局和完整句柄；不调用未知实体虚函数。
- 类/数据基址/休眠 getter 只解释受支持的简单指令模式，不认识就跳过。
- RecvTable/RecvProp 用固定 x86 wire 布局做有界读取、类型检查和循环检测，只遍历嵌入的 `baseclass` 链，不盲目展开自定义代理 datatable。
- 模型名查询只调用已核对对象、虚表条目和指令特征的两个 engine model-info 方法。接口/构建不符时停止采集；关闭再开启可重试。
- 使用 ReadProcessMemory 读取本进程字段；日志写入和 JSON 编码位于现有后台线程。
- 切图和重新开启时清空类/模型缓存。仅读取当前游戏进程。云端静态核对不会加载或执行上传的 DLL。

已完成 Linux 合约/回归测试、内存检查和 Windows x86 交叉编译。另附真实采集器的 x86 模拟内存测试源码，Windows 构建脚本会运行它；云端仅编译，没有执行该 Windows 测试；云端无法运行真实 L4D2，本诊断采集仍需要实机验证。
