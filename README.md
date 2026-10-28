# L4N LyricsPlugin 0.4.0 — 纯 x86 L4N 架构

Source Engine 相机、LX API、动画、world-to-screen 和 D3D9 绘制均在单个 L4N 插件中。
绘制经 LDB Client 正常转发至 Host → DXVK，不需要歌词 Host DLL 或共享内存。

下载包：`releases/LDBDangoMusic-0.4.0-l4n.zip`。

**旧安装目录已改变。** 退出游戏，将旧 Host 歌词 DLL 改名停用，新 DLL 放入 `bin/neko/plugins/`。
配置在 `bin/neko/plugins/LDBDangoMusic/config.json`。包内默认 LX 模式、提前 500ms、实验世界歌词开启；离线测试用 config.world-demo.json。
也可在 ZIP 的 source 目录执行：

```powershell
./scripts/install.ps1 -GameRoot 'D:\SteamLibrary\steamapps\common\Left 4 Dead 2'
```

该脚本备份并迁移原配置，只停用旧歌词 Host DLL，保留其他插件。
接口不匹配或相机无效时回退屏幕歌词，worldLyrics=false 可主动关闭世界模式。
详见 [架构、安装、限制及实机验证](docs/l4n-world-lyrics.md)。

## 构建

Windows 使用 **x86 MSVC** 开发者 PowerShell：`./scripts/build.ps1`。
独立 Windows D3D9/L4N 模拟宿主检查：`./scripts/smoke.ps1`。
Linux 回归：`bash scripts/test.sh`。
MinGW 32 位 POSIX：`bash scripts/build-mingw.sh`，可用 DANGO_CXX 指定 i686 编译器。
打包：`python scripts/package.py`。

当前 Windows/游戏运行均未验收；交叉编译和数学测试不能证明 Steam 游戏接口兼容。
已验证的 0.3.x Host 版本仍保留在 releases，可按文档回滚。
源码 GPL-3.0，SDK 头等单独许可/来源见 NOTICE.md。
