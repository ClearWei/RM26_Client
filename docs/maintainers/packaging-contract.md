# 开源发行与打包契约

这份文档定义“能够从源码构建”和“可以作为发行物交付”之间的边界。只有满足对应模式的验收项，
才能在 README、Release 页面或采访材料中把产物称为可发布版本。

机器可读模式由 `tools/release/packaging_policy.json` 维护，并由
`tools/release/check_packaging_readiness.py` 检查。文档和策略冲突时采用更保守的结论。

## 发行范围

仓库使用 `source` 模式，公开交付物为固定提交生成的源码归档。预编译的 macOS、Windows、Linux
安装包和模拟器 wheel 不属于该模式的发行内容。本地 `build/` 中出现的应用、可执行文件或 wheel
属于开发产物，经过目标平台安装和运行验收后才能作为二进制发行物。

## 发布模式

### `undecided`

这是保留的阻断状态，用于发行方式尚未确定的情况。该模式允许继续开发和验证源码，但不允许
上传 `.app`、wheel 或本地构建目录作为正式发行物。手动 `Public Readiness` 工作流应返回阻断。

### `source`

源码模式发布源码归档，可使用 source archive、source tarball 或 source zip。发行说明应明确
使用者需要安装依赖并构建，同时避免使用“下载即用”“已提供安装包”或“通过 wheel 安装”等表述。

正式发布源码归档前至少需要：

- 公开镜像、许可证、素材权属和第三方声明通过发布门禁；
- 从公开远端全新 clone 后，按照 README 完成配置、构建和测试；
- 保存构建平台、依赖版本、提交 SHA 和测试证据；
- 为最终源码归档生成 SBOM、第三方许可证包和 SHA-256 校验和；
- 明确源码归档是否签名，并给出可复核说明；
- README 和 Release 页面明确没有经过验收的二进制安装包。

源码归档只保留客户端、模拟器、测试、构建与发布脚本、公开配置示例和稳定说明文档。
阶段性方案、任务记录、工作区配置、本地 profile 和运行日志通过 `.gitattributes`
中的 `export-ignore` 排除。不得为缩小归档而删掉 CMake、QRC、模拟器静态资源、生成协议代码或
运行时必需文件；导出后必须在新目录重新构建和测试。

源码归档的文件级 SBOM 由 `tools/release/generate_source_sbom.py` 生成，最终归档校验和由
`tools/release/generate_checksums.py` 生成。源码归档的签名说明见
[源码发行物签名策略](signing-policy.md)。

### `binary`

二进制模式按平台发布可安装或可解压运行的产物。每个声明支持的平台都必须独立验收；Linux 构建
通过不能替代 macOS 或 Windows 证据。

切换到 `binary` 前至少需要：

- CMake 提供可复核的 `install()` 安装树；
- Qt/QML 插件和启用的 FFmpeg、Paho、Protobuf、Abseil、ONNX Runtime 等运行依赖随包部署；
- 应用名、版本、图标、Bundle ID 或 Windows VERSIONINFO 等平台元数据完整；
- 从干净环境安装后，在源码树外、任意当前目录下完成启动和基本功能 smoke test；
- 产物内不包含本机绝对路径、临时配置、开发缓存或未批准素材；
- 每个平台的产物格式、系统范围、CPU 架构和最低系统版本有明确说明；
- 最终产物生成 SBOM、第三方许可证包和 SHA-256 校验和；
- 签名、公证或明确不签名的策略已形成公开证据。

## 平台与产物验收

| 平台 | 可接受格式示例 | 必须验证的重点 |
| --- | --- | --- |
| macOS | DMG、签名 `.app` 或明确说明的压缩包 | Qt 依赖部署、Bundle 元数据、架构、签名与公证 |
| Windows | ZIP、MSI 或安装器 | Qt 插件、第三方 DLL、VERSIONINFO、卸载或解压运行路径 |
| Linux | AppImage、deb 或带安装说明的 tar | RPATH、Qt 插件、`.desktop`、图标和目标发行版范围 |
| 模拟器 | wheel、源码 extra 或 OCI 镜像 | protobuf、静态资源、命令入口、隔离安装启动 |

未在策略中声明的平台属于“不提供发行物”，而不是“可能可以运行”。

## 安装树契约

二进制模式需要先定义稳定安装树，再编写打包规则。至少区分：

- 可执行文件；
- Qt/QML 插件和第三方运行库；
- 只读运行资源；
- 示例配置；
- 用户配置、日志、缓存和录制数据。

配置、日志和运行数据应写入系统用户目录，不应写入 `/usr/bin`、Program Files 或签名后的 macOS
Bundle。安装测试必须从源码树外启动，避免相对路径兜底掩盖缺失资源。

## 版本与命名契约

机器可读状态由 `tools/release/version_policy.json` 维护，
`tools/release/check_version_contract.py` 只读取 Git 索引中的真实来源。应用版本应有唯一真相源，
并同步到 CMake、公开示例配置、Qt 运行时元数据、配置兜底、Docker 镜像元数据、macOS 应用包和
Release tag。

应用版本为 `1.0.0`，模拟器采用独立版本 `0.1.0`。规范名称登记为 `RM26CustomClient`，CMake、
Docker 和 macOS 入口在 1.x 期间继续使用兼容名称 `RoboMasterClient2025`。名称迁移应随主版本
升级处理，并同步各平台入口。

协议清单中的 `2.0.0` 对应官方 V2.0.0 兼容目标，始终作为独立维度检查，不等同于应用版本，
也不参与客户端或模拟器的版本比较。

后续发布应先同步所有真实来源，再调整机器策略。只修改 `declared_version` 或策略状态，不能视为
版本已经收敛；涉及 Bundle ID 或历史可执行名的变更应按破坏性迁移处理。

## 资源契约

资源必须同时满足“运行需要、文件存在、路径正确、体积可接受、允许再分发”。
`tools/release/check_runtime_resources.py` 按 `tools/release/runtime_resources.json` 核对 Git 索引中的
代码引用、精确 QRC alias 和 `install()` 规则。12 个历史比赛音效按
[ADR 0004](../decisions/0004-optional-audio-pack.md) 作为可选资源包保留文件名接口，默认源码归档
不携带。可选文件进入 Git 索引后，也要通过素材授权和 QRC alias 检查。

270 帧结算动画已通过 `install()` 保留外部目录结构。约 552 MiB 的原图不整体嵌入 QRC，避免
重复包体和超大生成源码；该取舍见
[ADR 0003](../decisions/0003-external-result-animation-assets.md)。

另外，打包设计还需处理：

- 结算帧原图约 552 MiB，授权和平台安装包体积仍需独立验收；
- 构建后整目录复制可能把 `.DS_Store` 等本地文件带入产物；
- 当前 `config.json` 的复制方式不应成为公开安装包的默认配置策略。

资源清单应采用显式 manifest，并在安装 smoke test 中逐项验证。素材授权仍以
`ASSET_LICENSES.md` 和 `tools/release/public_assets.json` 为准；打包成功不能替代授权批准。
`public_assets.json` schema v2 对每组 stage 0 索引项按“路径、NUL、blob OID、换行”生成精确
SHA-256，并记录文件数。它能发现索引中的增删和内容替换，但只固定复核对象，不证明权属或视觉
等价；合法变更也必须重新复核并更新快照。

## 依赖与许可证契约

每个发行物应保存实际启用的功能和精确依赖版本。自动探测到依赖后静默改变功能集，不属于可复现
发行。二进制发布还需要：

- 锁定或记录 Qt、Protobuf、Abseil、FFmpeg、Paho、ONNX Runtime 的来源和版本；
- 记录静态或动态链接方式及随包文件；
- 使用与项目许可证兼容的 FFmpeg 构建选项，单独复核 GPL 组件；
- 收集随包第三方许可证文本；
- 对 Python 依赖使用可复现约束，并从隔离环境验证模拟器安装。

## SBOM、校验和与签名

SBOM、校验和和签名只能在最终归档生成后执行，不能复用构建目录中的临时结果。

- SBOM 使用 SPDX 或 CycloneDX，并覆盖应用、Python 包和随包动态库；
- 每个公开文件写入 `SHA256SUMS`，发布后重新下载并复核；
- 第三方许可证包与 SBOM 中的组件对应；
- macOS、Windows 和 OCI 签名策略由项目方决定并留下证据；
- 不签名也是需要明确记录的决定，不能省略说明；
- 任何重打包都会使校验和与签名失效，必须重新生成。

## 公开历史边界

首次建立公开仓库时，从通过门禁的提交导出快照，并在新目录建立没有内部历史的公开根提交。
详细操作见
[公开镜像发布手册](public-release-runbook.md)。

## 最终验收

发布负责人只有在以下结果全部可复核时才能调整 `release_mode` 并发布：

1. `python3 tools/release/check_public_readiness.py` 通过；
2. `python3 tools/release/check_media_metadata.py` 通过；
3. `python3 tools/release/check_packaging_readiness.py` 通过；
4. `python3 tools/release/check_runtime_resources.py` 通过；
5. `python3 tools/release/check_version_contract.py` 通过；
6. 对应模式的干净环境构建、安装和启动证据齐全；
7. 最终产物的 SBOM、许可证包、校验和与签名策略齐全；
8. 从公开远端重新 clone，并在全新目录复验。

不得通过忽略退出码、删除失败检查或把 `release_mode` 改成与事实不符的值来获得绿色结果。
