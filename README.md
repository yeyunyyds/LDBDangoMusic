# L4D2DangoMusic 0.7.2

纯 x86 L4N 歌词插件：Source 相机、LX API、Peak 风格动画、世界投影、D3D9 绘制，正常经过 LDB Client → Host → DXVK。

0.7.2补充分组高度/距离调节、恢复可见弹跳和抖动、提供动画预览；默认歌词位于屏幕中心稍上方（距顶部44%），长句回屏默认关闭。保留0.7.1的暂停/重连清理和重复副本防护。

带固定预览的位置菜单：歌名、作者及1～9个歌词轮换位置可分别调整高度、左右、距离和大小，并可保存或重置。保留六套视觉预设、旧句额外停留1秒和长句回屏默认关闭，可在菜单开启。

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

## 0.6.1 旧句残留修正

取消基于场景计数跳过呈现，改为保存干净底图 → 绘制/呈现 → 恢复底图，避免文字留在游戏 backbuffer。
当前句/退场句使用独立生命周期，不复活旧实例。默认预设改名“暖白浮光”。
修正依据与未验收范围见 [旧句残留说明](docs/0.6.1-stale-lyrics.md)。

## 0.7.0 位置调节与预览

L4N → L4D2DangoMusic → **位置与数量（固定预览）**。
歌名、作者、歌词1～9可分别调整左右、高度、距离和大小，支持单项/全部位置重置。
预览期间暂时隐藏实时歌词，播放器继续播放；各级页面均可“保存设置并结束预览”，恢复当前歌词。
默认2个轮换位置位于屏幕中心略下方；歌名在上边缘附近，距离更远。
详情和旧配置说明见 [位置编辑说明](docs/0.7.0-layout-editor.md)。

## 0.7.1 暂停、重连与旧副本清理

暂停/停止/断开后清理歌词与回屏状态，恢复播放或重开软件时重新核对进度并获取歌词。
菜单标明0.7.1，新增实际DLL路径、其他已加载模块和播放器状态诊断。
**有旧版本时，请退出游戏和Host，使用安装脚本升级。** 脚本会将本项目旧副本、改名副本及DLL备份移出插件目录，保留配置。
更多说明见 [播放器会话与重复副本](docs/0.7.1-player-session.md)。

## 0.7.2 分组位置与动画

位置页可分别调整歌名、作者；歌词增加“全部歌词高度 / 远近”，保留逐个编号设置。
歌词默认高度改为44%，长句回屏默认关闭。弹跳从实际显示起算，抖动不再随远近缩得过小；提供示例动画预览和恢复默认动画入口。
旧默认值会迁移一次，自定义位置保留。详情见 [位置、动画与默认值迁移](docs/0.7.2-motion-and-placement.md)。
