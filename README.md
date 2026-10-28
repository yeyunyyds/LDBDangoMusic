# L4N LyricsPlugin 0.4.3 — 纯 x86 L4N 架构

Source Engine 相机、LX API、动画、world-to-screen 和 D3D9 绘制均在单个 L4N 插件中。
绘制经 LDB Client 正常转发至 Host → DXVK，不需要歌词 Host DLL 或共享内存。

下载包：`releases/LDBDangoMusic-0.4.3-l4n.zip`。

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

## 0.4.2 实机接口修正

已静态检查用户提供的游戏 DLL：Client 实际为 VClient016；View_Render 为槽 25，Engine 矩阵为槽 37/38。
本版同时修正接口和布局，并校验受支持构建及方法 RVA，不将旧 SDK 布局套用到未知版本。
替换 DLL 后保留原配置并完整重启。实际游戏效果待验证。
详见 [二进制证据与验收](docs/0.4.2-source-build.md)。

## 0.4.3 校验明细与回调链

实机已找到所需接口，但 0.4.2 的合并校验未通过，具体失败项尚未从旧日志确定。
本版打印各项构建值和方法入口，并在已核对的游戏构建上支持现有 L4N/其他插件回调串接。
构建不匹配或不可执行的入口继续拒绝启用相机。
升级只需退出后替换 L4N DLL，保留配置。详见 [校验与回调链](docs/0.4.3-callback-chain.md)。
