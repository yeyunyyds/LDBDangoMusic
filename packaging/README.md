# L4D2DangoMusic @VERSION@

《求生之路 2》原生 L4N 歌词插件。支持 LX / BLF 本地 API、翻译、逐字动画、固定世界歌词和场景挂画歌词。

## 安装

1. 退出游戏，确认已安装并能正常使用 L4N。
2. 将压缩包内的 `bin` 文件夹合并到游戏根目录，例如：
   `D:\Steam\steamapps\common\Left 4 Dead 2`。
3. 首次安装复制全部文件。升级时替换 `bin/neko/plugins/L4D2DangoMusic.dll`，保留自己的 `bin/neko/plugins/L4D2DangoMusic/config.json`，避免覆盖设置。
4. 将旧的 `LDBDangoMusic.dll`、本插件的改名副本及旧 Host 歌词插件移出插件加载目录，再完整重启游戏。只保留一份歌词插件。

安装后的文件位置：

```text
游戏根目录/
└─ bin/neko/plugins/
   ├─ L4D2DangoMusic.dll
   └─ L4D2DangoMusic/config.json
```

本插件运行于 32 位游戏进程，不依赖 LDB、DXVK、Unity 或额外下载的游戏 DLL。L4N 和音乐播放器需要自行安装。

## 使用

- 开启兼容 LX / BLF 的本地音乐 API，默认地址 `127.0.0.1:23330`，需要支持 `/status` 和 `/lyric-all`。
- 在 L4N 菜单进入 **L4D2DangoMusic**，调整翻译、颜色、动画、歌词位置和同步偏移。
- 设置立即生效，操作结果用短提示显示；用 L4N 原生 `-` 返回，重新进入页面刷新数值。点击主菜单“保存设置”可保留下次启动设置。
- 开启“场景挂画歌词”后，可自动寻找前方左右墙面，或按 F8 手动放置。当前句贴墙成功后保持原位，旧句按停留与淡出设置自然消失。
- 场景挂画默认关闭。无合适墙面的新句可回退为固定世界歌词。
- 暂停、停止或播放器断开时隐藏歌词。自动切歌会重新复核歌词数据。

## 排查

- 没有歌词：检查播放器 API、端口和歌曲是否含同步歌词。
- 双份歌词：移出所有旧副本后重启游戏。
- 同步偏差：调整“同步偏移”，正数提前、负数延后。
- 挂画失败：尝试更平整、更大的实墙或较小挂画尺寸。场景接口依赖受支持的游戏构建；射线遮挡针对世界几何，不保证动态门、角色或道具遮挡。
- 日志：`bin/neko/plugins/L4D2DangoMusic.log`。

## 源码与许可

对应源码：@SOURCE_URL@

项目主页：https://github.com/yeyunyyds/LDBDangoMusic/tree/main

本安装包仅包含运行所需的 DLL、默认配置及本文档。源码、构建脚本和测试位于 main 分支。第三方项目参考、许可声明与 GPL 许可全文附于本文档末尾。
