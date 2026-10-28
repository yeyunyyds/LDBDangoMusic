# LDBDangoMusic 0.2.0 — SwapChain demo 修正版

Windows x64 LDB Host 插件，D3D9 屏幕空间歌词；支持离线演示和 BLF/LX 本机开放 API。

## 直接测试 demo

使用 LDB 分支 `codex/steam-reshade-plugin-entry-test`，提交 **db5cb944314bb50c2e61835d5f0e8855c089e2e6** 或确认兼容的后续版本。该版本增加真实 SwapChain 及其生命周期回调，旧 Host 和本插件不可混用。

1. 退出游戏和 Host。
2. 将 `releases/LDBDangoMusic-0.2.0-demo.zip` 中的 `bin/` 合并到游戏根目录。
3. 确认 `server.plugins = True`，完整重启。
4. 左上应显示 `LDBDangoMusic 0.2.0 DEMO - SwapChain/Device`，下方循环中英文歌词。

安装位置：

```text
游戏根目录/bin/.l4d2bridge/plugins/
├─ LDBDangoMusic.dll
└─ LDBDangoMusic/config.json
```

包内 demo=true。已有配置若不覆盖，请手动设置 demo=true；配置修改后重启 Host。不存在配置时也默认演示模式。

如果仍不显示，查 Host 日志中的 `LDBDangoMusic` 和 `LDB_HOST_PLUGIN`，具体诊断见 [修复与验收记录](docs/0.2.0-validation.md)。本次修复移除了旧版跳过 SwapChain Present 的逻辑，更新了 SDK 回调布局。

## 使用播放器

将实际 `config.json` 的 `demo` 改为 false，在 BLF/LX 开启本机开放 API（默认 23330），完整重启 Host。
仅从 `127.0.0.1` 读取 `/status` 和 `/lyric-all`，不会播放音乐或登录。
支持 LRC、多时间标签、时间偏移、LX 逐词时间、翻译、左右定位、淡入上浮、暂停保持、进度/倍速及断线重试。

`offsetMs` 正值使歌词提前；`alternate=false` 居中；`verticalPosition` 为屏幕高度比例。
尚未实现世界空间歌词、光晕、设置 UI、网易云官方客户端及普通 LRC 的估算逐字动画。

## 构建与测试

Windows 安装 MSVC C++ 桌面工具和 Windows SDK，从 **x64 开发者 PowerShell** 执行：

```powershell
./scripts/build.ps1
./scripts/smoke.ps1
```

MSVC /MD 产物需要 Visual C++ x64 运行库。ZIP 中 MinGW 静态运行库产物仅依赖 Windows 系统 DLL。
独立 smoke 程序在临时目录配置 demo，以实际 D3D9 像素验证主/附加 SwapChain、状态恢复和 Reset；不能替代游戏验证。

Linux：

```sh
bash scripts/test.sh
# 安装 MinGW x64 POSIX 后：
bash scripts/build-mingw.sh
# 或 DANGO_CXX=/path/to/x86_64-w64-mingw32-g++-posix bash scripts/build-mingw.sh
```

`python scripts/mock_player.py` 提供本机模拟播放器，覆盖暂停/跳转/换歌，勿与真实播放器占用同一端口。
`python scripts/package.py` 打包当前已构建 DLL、Windows 测试程序和完整对应源码。

## 验证边界

已运行 Linux 核心、Present 目标选择和模拟 HTTP→C++ 解析回归；已交叉编译 x64 DLL 及 Windows 验证程序。
**未运行 Windows D3D9、真实播放器或 L4D2/DXVK/ReShade 验收。** 首次文字会创建 GDI/D3D 资源并缓存，性能待实测。

源码 GPL-3.0，来源与独立依赖许可见 NOTICE.md。
