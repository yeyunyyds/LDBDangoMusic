# 0.4.0 纯 L4N 实验版

单个 x86 L4D2DangoMusic.dll，导出 GetL4NPluginInstance，进入游戏进程。
没有 Host 插件入口、外部相机进程或共享内存。历史 Host SDK 头/测试仅为旧版本资料，不参与新版本编译。

## 流程

L4D2DangoMusic → Source 相机 / LX HTTP / 动画 / 世界投影 / D3D9 绘制 → LDB Client → Host → DXVK。

Source 采样：核对 VClient016 / VEngineClient013 接口后，在 View_Render 原函数返回时读取引擎维护的 world-to-view / world-to-screen 矩阵。
不再使用 D3D9 GetTransform 推断相机。游戏侧快照在进程内发布；渲染回调只读快照。
未找到准确接口或检测到其他来源的函数指针时，拒绝安装相机钩子，继续屏幕歌词。
这两个接口与 Steam 实机的兼容性尚待验证，不能仅根据 SDK 编译成功认定兼容。

绘制：L4N 通知 Device 创建后，在游戏侧设备上安装 Device/SwapChain Present、PresentEx、Reset/ResetEx 的虚表回调。
先提交歌词再调用原始呈现函数，递归保护避免同一次嵌套呈现重复绘制。
不替换 Present 结果、不加载另一套 D3D9，不直接使用 Host 的真实 Device。
原虚表入口保留并串接，恢复时仅替换仍指向自己的入口。运行期间卸载/替换 DLL 不支持，必须完整退出游戏。
多设备同时交替渲染尚不支持；本原型针对 L4D2 单游戏设备。

## 世界锚点

- 相机矩阵需可逆且符合透视/仿射关系；快照超 500ms、菜单/加载、切图或设备 Reset 后回退屏幕。
- 首个有效视图固定 WORLD ANCHOR + 测试标记。
- 每句开始时在镜头前方左右交替放置锚点，之后位置固定。
- 按每帧矩阵投影，移动产生视差、距离影响字号；背后或视野外的锚点隐藏。
- 换歌/地图代次变化/明显传送时重建锚点。
- 当前是世界点投影后画屏幕朝向的文字，不做墙壁遮挡、场景深度或地面碰撞。
- 相机采样和 LDB 提交帧尚未精确编号配对，渲染队列导致的相机延迟/抖动需要实机检查。

## 安装与回滚

1. 完全退出游戏和 Host。使用已有 L4N SDK v2 加载器。
2. 将旧 `bin/.l4d2bridge/plugins/L4D2DangoMusic.dll` 改名为 .disabled，避免双重歌词。
3. 新 DLL 放在 `bin/neko/plugins/L4D2DangoMusic.dll`。
4. 配置放在 `bin/neko/plugins/L4D2DangoMusic/config.json`，可迁移旧 LX 配置。
5. worldLyrics=true 开启实验世界模式；worldLyrics=false 只用屏幕歌词。
6. config.world-demo.json 是开启演示、世界锚点及诊断的测试配置。
7. 回滚：退出后移走新 L4N DLL，恢复旧 Host DLL 和旧配置。其他 Host 插件无需停用。

ZIP source/scripts/install.ps1 可自动备份、迁移配置并禁用旧 Host DLL。
日志为 bin/neko/plugins/L4D2DangoMusic.log。相机诊断 JSONL 在对应 L4D2DangoMusic 配置目录；新增 source_camera 字段记录游戏矩阵。

## 实机检查

- 先确认屏幕歌词仍能显示；无字时先查 D3D9 callbacks installed / first textured draw submitted 及错误 HRESULT。
- 再检查 Source camera installed；若提示 exact interface unavailable，保留日志，不修改寄存器/虚表索引猜测。
- 有效时左上显示 GAME CAMERA: ACTIVE，固定锚点随转头移出视野、走动产生视差。
- 测试进图、转头、移动、观战、切图、暂停、全屏切换、退出。
- 若需要诊断设置 cameraDiagnostics=true，提交 JSONL 和 L4D2DangoMusic.log。

## 验证边界

已在 Linux 执行相机逆矩阵、世界投影/视差、距离缩放、超时回退及原有歌词/API/动画回归。
已交叉编译 x86 DLL 和独立 Windows L4N 模拟宿主。
尚未运行 Windows 模拟宿主、真实 L4N/游戏、LDB 转发、Source 接口、画面效果与性能验收。
纯 L4N 的字体纹理、状态保存及绘制会占用 x86 进程资源，存在额外 IPC/同步成本，尚未量化。

## 编译器边界

MSVC 构建直接使用原始 L4N SDK 虚接口。MinGW 构建通过显式的 MSVC x86 九项虚表适配（单个 scalar-deleting destructor 槽）导出接口，避免 GNU 双析构槽造成回调错位。Windows smoke 也按 MSVC 槽位调用导出实例。实际 L4N 加载器兼容仍需运行验证。

0.4.2 以用户 DLL 的静态布局为准，见 0.4.2-source-build.md；旧 SDK 的方法索引不适用。
