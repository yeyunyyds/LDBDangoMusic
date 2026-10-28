# 0.3.1 固定功能相机矩阵诊断

不修改 LDB、不 Hook、不猜 shader 寄存器、不依赖 Remix RtCamera。
本版保留 0.3.0 屏幕歌词，增加可选的原始矩阵记录。它不是世界歌词实现。

## 启用与操作

1. 退出游戏和 Host，替换 DLL。在实际配置中添加 `"cameraDiagnostics": true`。
   保留你的 LX 模式和同步设置即可；也可用包内 config.camera-demo.json 覆盖实际配置来做离线演示诊断。
2. 重启后，在菜单停留 5 秒，进图，分别站立、前后左右移动、水平转头、上下看各约 5 秒。
3. 再打开/关闭菜单，切图，切到观战模式。记录操作顺序及大致秒数；最好录一段对应画面。
4. 完全退出游戏，取得 `bin/.l4d2bridge/plugins/L4D2DangoMusic/camera-进程号-时间.jsonl`。
   Host 日志会打印完整文件路径，写入失败也会提示。
5. 可执行 `python source/scripts/analyze_camera.py 路径.jsonl` 查看候选位姿和分组统计。
6. 测完关闭 cameraDiagnostics，重启恢复普通模式。

## 记录契约

- 只在 on_before_present、插件改变任何设备状态之前读取 GetTransform(VIEW/PROJECTION) 和 GetViewport。
- 包含 HRESULT、行优先矩阵数组、viewport [x,y,width,height,minZ,maxZ]、SwapChain ID、设备生命周期代次及单调时钟。
- 全局最多 4Hz，多个 SwapChain 竞争采样机会；不能保证所有链都出现。分析时按链与代次分组。
- Host 渲染线程为单生产者，固定容量 SPSC 队列交给后台音乐线程；不在采样中分配对象或写文件。
- 后台网络请求可能延后落盘，但每条记录保留实际采样 tick；队列满时丢弃并统计 dropped_total。
- 每次运行创建独立文件，最多 2400 条或 2MiB。旧日志不自动删除。
- 身份矩阵、成功返回、可求逆都不足以证明游戏相机正确。

## 如何决定下一步

分析工具只对非单位、近似刚性仿射 VIEW 求逆，报告候选位置及三根基向量。
符合常见透视形式时估算候选垂直 FOV/宽高比。始终标记 confirmed=false。
必须进一步与移动/转头/缩放、菜单/切图/观战实测相关联，确认坐标约定。
即使数据变化，也可能是 HUD 或其他变换。

如果场景内始终为单位阵、保持不变、或只在菜单变化，Present 阶段固定功能矩阵不足。
届时应增加游戏侧相机数据通道，或设计场景 Draw 阶段捕获方案；不能靠猜一个矩阵继续画世界歌词。
深度遮挡另行验证，本诊断不读取深度或 shader 常量。

## 验证范围

Linux 已验证队列容量/顺序、非有限/单位阵判断、候选矩阵逆和分链分析。
已交叉编译 Windows x64 DLL；真实 L4D2 矩阵尚未采集，当前无法判定固定功能矩阵是否可用。
