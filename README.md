# L4D2DangoMusic 0.6.0

纯 x86 L4N 歌词插件：Source 相机、LX API、Peak 风格动画、世界投影、D3D9 绘制，正常经过 LDB Client → Host → DXVK。

本版加入 L4N 设置菜单、六套预设、RGB 自定义、远处歌名/作者、旧句额外停留1秒、长句默认3秒回到当前视野，以及瞬间重复绘制的针对性修正。

## 安装与升级

退出游戏和 Host，解压发布包，在 **source 文件夹**打开 PowerShell：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\install.ps1 -GameRoot "D:\Steam\steamapps\common\Left 4 Dead 2"
```

如果已经在 source/scripts 目录，只需 `.\install.ps1 ...`。
工具会迁移旧 LDBDangoMusic 配置，并停用旧歌词 DLL，保留旧文件供回滚。

手动安装位置：`bin/neko/plugins/L4D2DangoMusic.dll`。
配置：`bin/neko/plugins/L4D2DangoMusic/config.json`；日志：`bin/neko/plugins/L4D2DangoMusic.log`。
**手动升级务必停用旧 LDBDangoMusic.dll，不能让新旧歌词插件同时加载。**

游戏中进入 L4N → L4D2DangoMusic。改动实时生效，点击“保存设置”后才在下次启动保留。
详情见 [菜单、迁移及重影说明](docs/0.6.0-menu-and-ghosting.md)。

## 构建与验证

Windows 使用 x86 MSVC 开发者 PowerShell：`./scripts/build.ps1`；独立验证：`./scripts/smoke.ps1`。
Linux 回归：`bash scripts/test.sh`。MinGW i686 POSIX 编译：`bash scripts/build-mingw.sh`，可通过 DANGO_CXX 指定编译器。
打包：`python scripts/package.py`。预设资料在 presets/。

Source 游戏构建适配与此前0.4.3相同，仅支持已核对的 DLL 资料；不匹配则保留屏幕歌词。
云端测试和交叉编译不代表 Windows L4N 菜单或重影问题已经游戏内验收。
旧包和 GitHub 仓库原名可能保留，当前软件名、代码和产物统一为 L4D2DangoMusic。

GPL-3.0；来源与 SDK 单独许可见 NOTICE.md。
