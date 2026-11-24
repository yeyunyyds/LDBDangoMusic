# Folia Native Stage — 已认可基线

归档日期：2026-11-23

用户实机反馈：「folia 功能认可 可归档」。

## 基线
- 测试源码：`f8e26f8bb766c851b1608a3027f20e10870f6720`
- 完整构建：[CI 45328195743](https://github.com/yeyunyyds/LDBDangoMusic/actions/runs/45328195743)（通过）
- [测试安装包](https://github.com/yeyunyyds/LDBDangoMusic/actions/runs/45328195743/artifacts/14890617913)

## 认可范围
- Folia 原生 Stage API 与 Lyric API 接入，音乐来源编号 8。
- DLL 内直接读取本机连接文件、播放状态和歌词，不启动额外 EXE，不需要 .NET MediaBridge。
- 保留现有 Folia 使用配置和原生歌词数据管线。
- 核心实现：src/folia_native.hpp、src/folia_protocol.hpp；src/plugin.cpp 的 Folia 数据源分发；tests/folia_protocol_test.cpp；原生安装包构建脚本。

归档分支保存实际测试的完整源码快照，而非重构后的新组合；其中同时存在 AFK 运镜等实验代码，但用户本次认可仅限 Folia 功能，不代表其他功能已经验收。此归档不合并 main，不发布正式 Release，不更改当前实验分支。后续提取 Folia 功能时以此提交为依据。
