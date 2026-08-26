# 项目文档

这里是 RM26 自定义客户端、独立模拟器和工程验证资料的稳定入口。第一次阅读不需要先理解目录
结构，按照自己的目标选择一条路径即可。

本项目并非 DJI 或 RoboMaster 官方项目。协议和规则应以官方最新资料为准；仓库文档只说明
本项目的兼容目标、实现和验证边界。深度文档目前以中文为主，英文读者可先从
[English README](../README.en.md) 开始。

## 先选择你的阅读路径

| 你的目标 | 从这里开始 | 下一步 |
| --- | --- | --- |
| 我想先看它解决什么问题 | [项目 README](../README.md) | 核心能力、系统图和已知限制 |
| 我想在本地跑起来 | [快速开始](../README.md#快速开始) | [配置说明](getting-started/configuration.md)与[模拟器说明](../sim/README.md) |
| 我想读懂代码和架构 | [学习路径](learning-path.md) | 架构、线程、协议和测试专题 |
| 我想提交一个改动 | [客户端开发指南](rm26-client-dev-guide.md) | [贡献指南](../CONTRIBUTING.md) |
| 我负责准备正式发布 | [开源发行与打包契约](maintainers/packaging-contract.md) | [公开镜像发布手册](maintainers/public-release-runbook.md) |

## 第一次读代码

推荐先完成[学习路径](learning-path.md)中的“10 分钟了解项目”，再选择一条真实数据链路：

- MQTT/Protobuf 消息如何经过 `NetworkManager` 和 `GameData` 驱动 QML；
- 客户端命令如何从界面经过类型化接口回到协议对端；
- UDP 视频如何完成接收、组帧、解码、恢复和显示；
- 独立模拟器如何在没有实车时提供可控输入，同时保持生产边界。

这比按文件夹从头阅读更容易理解项目为什么采用当前结构，也更容易找到适合贡献的模块。

## 架构与数据流

- [架构总览](architecture/overview.md)：正式客户端、模拟器和验证工具的整体边界；
- [组件职责与数据流](architecture/data-flow.md)：消息、状态、界面、上行动作和两路视频的代码落点；
- [传输接入与链路诊断](architecture/transport-integration.md)：MQTT、UDP、现场接入和双端证据；
- [战术分析](architecture/tactical-analysis.md)：当前实现、数据可信边界和持久化演进方向；
- [模拟器架构](architecture/simulator.md)：独立协议对端、组件边界和已知结构债务；
- [线程与对象生命周期](architecture/threading-model.md)：线程归属、退出和迟到回调；
- [协议边界与真相源](architecture/protocol-boundary.md)：官方资料、schema、客户端和模拟器的关系；
- [测试与质量门](architecture/testing.md)：不同改动应提供什么证据；
- [架构决策](decisions/README.md)：影响多个模块且需要长期保留的技术取舍。

理解项目时始终保持四类边界：

1. `src/` 是正式 Qt/C++ 客户端；
2. `src/devhooks/` 是显式启用的观测桥，不承载业务规则；
3. `sim/` 是独立协议对端，不与客户端共享进程内状态；
4. `tests/` 和 `tools/release/` 用于测试与发布检查，不应成为生产客户端的隐性运行依赖。

## 开发与验证

- [客户端开发指南](rm26-client-dev-guide.md)：环境、构建、模块边界、测试、联调和排错；
- [配置说明](getting-started/configuration.md)：本地 `config.json`、环境变量、两路视频和模拟器联调；
- [可选比赛音效包](getting-started/optional-audio-pack.md)：自有语音的文件名、放置方式和授权门槛；
- [贡献指南](../CONTRIBUTING.md)：Issue、PR、配置、素材和安全要求；
- [编码与提交约定](development/coding-style.md)：C++、QML、Python、中文注释和提交习惯；
- [验证说明](development/verification.md)：必跑命令、最近一次发布候选结果和未覆盖边界；
- [模拟器说明](../sim/README.md)：安装、启动、参数和开发约束；
- [公开发布预检](../tools/release/README.md)：文档、许可证、素材、敏感信息和打包声明门禁。

普通贡献只需 Git、CMake、CTest、Python 和项目依赖；发布维护者可按需运行公开发布预检。

## 协议兼容资料

- [官方资料索引与校验值](references/official-materials.md)
- [协议目标清单](../src/network/proto/protocol_manifest.json)
- [Protobuf 协议单源收敛记录](maintainers/protocol-convergence-plan.md)

官方 PDF、逐页 OCR、页面截图和内部需求稿不进入公开树。仓库只保留官方入口、版本、校验值和
项目独立编写的兼容性说明；素材与再分发边界见 [ASSET_LICENSES.md](../ASSET_LICENSES.md)。

## 社区与开源治理

- [支持说明](../SUPPORT.md)
- [安全政策](../SECURITY.md)
- [社区行为准则](../CODE_OF_CONDUCT.md)
- [路线图](../ROADMAP.md)
- [变更日志](../CHANGELOG.md)
- [第三方依赖说明](../THIRD_PARTY_NOTICES.md)
- [素材来源与授权清单](../ASSET_LICENSES.md)

维护者还应阅读：

- [开源发行与打包契约](maintainers/packaging-contract.md)
- [公开镜像发布手册](maintainers/public-release-runbook.md)
- [首个源码发行版签名策略](maintainers/signing-policy.md)
- [社区开源发布稿](communications/2026-rm26-custom-client-open-source-release.md)

发布说明只引用稳定的项目文档和机器可读清单；阶段性方案、任务记录和本地日志
不进入公开源码快照。

## 当前发布状态

首个公开版本采用 MIT License 和干净根快照的源码归档模式，当前 `release_mode` 为 `source`；
10 组素材的固定快照已经完成公开登记。开发机 `.app` 和模拟器 wheel 仍不是已验收发行物，远端
CI、Windows 与 Linux 构建结果应在后续版本继续补齐，不能由 macOS 验证结果替代。

## 文档维护规则

- 只把已经实现或已经验证的能力写成事实；计划内容明确标注状态和适用版本。
- 重要结果记录提交、平台、依赖、命令和未覆盖范围，不用截图或采访替代性能证据。
- 示例使用占位地址，不记录真实现场主机、账号、密钥路径、抓包和比赛日志。
- 相对链接应能从文件当前位置打开，不使用个人电脑绝对路径。
- 引用官方和第三方资料时保留来源与版本，不复制没有再分发许可的全文或页面素材。
- 代码、配置和行为变化影响公开说明时，在同一 PR 中更新相关文档。
