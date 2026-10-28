# LDBDangoMusic — 第一阶段原型

独立 C++17 / x64 Windows DLL，使用 LDB Host ABI v1。在 Present 前绘制屏幕空间歌词，后台读取 BLF / LX 本机 API。不会播放音乐或登录账号。

## 宿主要求

使用 Bridge `codex/steam-reshade-plugin-entry-test` 的提交 `90083c0a0f9685638fddd51ff8e6b098287f5648` 或经过兼容验证的后续版本。主分支 L4N SDK v2 不能替代这个 Host API。

仅支持 x64 Host。`server.plugins = True` 后完整重启 Host，插件不支持热加载。

## 构建

Windows 安装 Visual Studio C++ 桌面开发工具及 Windows SDK，从 **x64** 开发者 PowerShell 执行：

```powershell
./scripts/build.ps1
```

生成 `build/LDBDangoMusic.dll`、核心测试及独立 D3D9 验证程序。MSVC /MD 产物需要匹配的 Visual C++ x64 运行库。

Linux 核心测试：`bash scripts/test.sh`。不需要安装游戏。

## 安装与两条验证路径

将 DLL 放入 `bin/.l4d2bridge/plugins/`，将 `config.example.json` 复制为 `bin/.l4d2bridge/plugins/LDBDangoMusic/config.json`。

1. **离线**：设置 `demo: true`，启动指定版本的 Bridge 和游戏，观察中文、英文、左右换句、逐字出现及循环时间轴。
2. **播放器**：设置 `demo: false`，在 BLF/LX 开启本机开放 API（默认 23330），播放有同步歌词的歌曲。依次验证暂停、恢复、拖动进度、换歌、关闭和重启播放器；切歌后旧歌词不得继续显示。

配置变更需重启 Host。`offsetMs` 正值让歌词提前；`verticalPosition` 为屏幕高度比例；`alternate: false` 改为居中。

### 独立 Windows D3D9 验证

在仓库根目录执行 `./build/host_smoke.exe`。它加载 build 目录 DLL，在隐藏窗口创建真实 D3D9 Device，验证 ABI 拒绝、实际非黑色像素、混合状态恢复、设备 Reset 和退出。可不配置播放器，此时检查连接提示文字。要测离线歌词，在仓库根下创建 `plugins/LDBDangoMusic/config.json` 并设置 demo。该程序不替代 DXVK/ReShade 和游戏验收。

## 当前实现

- 生命周期、WinHTTP 本机读取、有限响应大小/超时、断连重试。
- LRC offset/多时间标签、LX 逐词时间、按时间戳匹配翻译。
- 进度单调时钟插值、暂停保持、倍速、过期状态停止推算。
- 左右屏幕定位、逐词淡入上浮、普通 LRC 整句淡入、中文 GDI 字体纹理。
- D3D9 状态保护、默认池纹理在 Reset 前释放、按需重建。

尚未实现：普通 LRC 的估算逐字动画、换句交叉淡出、描边光晕、设置 UI/热键、完整字素/字体回退、世界空间歌词、网易云官方客户端。

ABI v1 没有提供 SwapChain 指针，当前跳过 SwapChain Present，避免绘制到错误 backbuffer；若实际宿主仅走该路径，需要扩展接口后继续验收。首次文字出现会在渲染线程创建 GDI/D3D 资源，缓存后复用，性能尚未实测。

## 验证记录与限制

Linux 已运行核心测试，并使用 MinGW GCC 14 交叉编译 x64 DLL。Windows 独立验证程序、实际播放器、L4D2、DXVK、ReShade、全屏切换和多小时稳定性尚未运行，不能据此宣称游戏内通过。

Windows 验收应记录游戏/Bridge/播放器版本、Host 日志、连接状态、跳转/暂停效果、Reset/窗口切换结果，并对比关闭插件时的帧时间。测试完成后再将原型视为可发布版本。

来源及许可见 NOTICE.md；GPL-3.0。

可用 `./scripts/smoke.ps1` 在临时目录自动配置离线模式并执行独立 D3D9 验证，不覆盖现有用户配置。用 `python scripts/mock_player.py` 可启动模拟播放器，测试暂停/恢复、进度跳转、换歌；它不等同于真实 BLF/LX 验收，使用前请关闭占用 23330 端口的播放器 API。

可复现交叉编译命令：安装 MinGW x64 POSIX 工具链后执行 `bash scripts/build-mingw.sh`，或通过 `DANGO_CXX` 指定编译器路径。该脚本构建 DLL 和 Windows 验证程序，不能在 Linux 上执行 Windows 验证。

云端已验证模拟 HTTP 服务返回的数据可被 C++ 核心正确解析，覆盖播放、暂停、跳转和换歌。此测试使用 Python HTTP 客户端，不代表 Windows WinHTTP 已运行通过。
