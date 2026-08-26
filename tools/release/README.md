# 公开发布检查

## QML lint 基线检查

`check_qml_lint.py` 使用 Qt 自带的 `qmllint` 扫描 `src/qml`，并按
`qml_lint_policy.json` 阻止静态质量回退：`unqualified` 上限为 0；任何
新的诊断类别、严重级别变化或数量回升都会失败。

```bash
python3 tools/release/check_qml_lint.py
```

策略要求全仓保持零诊断，不能为通过检查而上调基线。检查器会先查找 PATH，再通过 `qtpaths6`
定位发行版专用的 Qt 工具目录；仍缺少
`qmllint` 时返回退出码 `2` 并给出 Qt Declarative 工具依赖提示。Quality CI 在 Ubuntu 原生 Qt
环境中执行真实扫描，检查器自身的策略和回退判定由发布工具单元测试覆盖。

## 示例配置检查

`check_example_config.py` 检查工作区中的公开配置是否完整、只连接本机，并且没有凭据字段或个人
绝对路径。日常开发和 Quality CI 可直接运行：

```bash
python3 tools/release/check_example_config.py
```

也可以显式检查准备运行的本地文件：

```bash
python3 tools/release/check_example_config.py config.json
```

退出码为 `0` 表示示例通过，`1` 表示存在配置问题，`2` 表示文件无法读取或 JSON 无法解析。该检查
只接受回环服务端和 Broker；现场配置不应通过放宽规则进入公开仓库。

## 公开媒体元数据检查

`check_media_metadata.py` 使用 `ffprobe` 检查 Git 索引中的音视频，而不是开发者尚未暂存的
工作区文件。它会阻止未经批准的标题、作者、备注、地点、设备等高风险字段，报告只显示字段名
和内容摘要，不回显可能包含隐私的信息：

```bash
python3 tools/release/check_media_metadata.py
```

确需保留的字段必须在 `public_media_metadata.json` 中按文件、作用域、字段和精确值登记，并引用
Git 索引中存在的复核记录或 HTTPS 证据。策略中的失效登记同样会阻止发布，避免素材已清理而
旧隐私值仍留在公开清单中。该检查只证明已处理已知高风险元数据，不代表素材已经取得再分发授权；
创建时间等技术字段仍需在批准素材前人工复核。

## 打包发布就绪检查

`check_packaging_readiness.py` 读取 Git 索引中的 `packaging_policy.json`、CMake 文件和证据路径，
区分“源码可以构建”和“已有可安装发行物”：

```bash
python3 tools/release/check_packaging_readiness.py
```

`release_mode` 只允许三种状态：

- `undecided`：尚未决定源码版还是二进制版，检查必须返回阻断；
- `source`：只允许声明源码归档，不能同时声称已有 `.app`、wheel 或其他安装包；
- `binary`：必须声明平台与产物格式，并提供 `install()`、平台元数据、运行依赖部署和安装后启动
  测试等静态证据。

检查还会阻止 Git 索引跟踪 `*.egg-info`、`build/`、`dist/`、`output/`、wheel 和典型编译产物。
未跟踪的本地构建目录不属于发布快照，也不会让检查失败。策略中的证据必须使用仓库相对路径；
错误输出不会回显非法的本机绝对路径。

仓库策略为 `source`：发行内容为源码归档，并要求源码构建证据、SPDX SBOM、SHA-256 校验和
生成入口和“不签名”说明都能从 Git 索引复核。该检查覆盖发行方式与供应链入口；许可证、素材
权属、版本和运行资源由各自门禁检查。完整标准见
[开源发行与打包契约](../../docs/maintainers/packaging-contract.md)。

## 源码归档供应链

在最终无历史公开镜像的固定提交上生成文件级 SPDX 2.3 SBOM：

```bash
SOURCE_DATE_EPOCH="$(git show -s --format=%ct HEAD)" \
python3 tools/release/generate_source_sbom.py \
  --root . \
  --output dist/source.spdx.json \
  --license '<已确认的 SPDX 标识>'
```

本项目使用 MIT License，生成公开 SBOM 时传入 `MIT`。其他项目若尚未确认许可证，应保留
`NOASSERTION`；SBOM 元数据本身不会改变代码授权。最终源码归档生成后再计算校验和：

```bash
python3 tools/release/generate_checksums.py \
  dist/RM26CustomClient-<version>.tar.gz \
  dist/source.spdx.json \
  --output dist/SHA256SUMS
```

SBOM 只覆盖 Git 索引中的源码文件；未来二进制发行还必须补充动态库、系统包和平台运行时组件。
首个源码归档暂不签名，边界见
[源码发行物签名策略](../../docs/maintainers/signing-policy.md)。

## 运行时资源检查

`check_runtime_resources.py` 只读取 Git 索引，并按 `runtime_resources.json` 核对代码引用、QRC
映射和安装树规则：

```bash
python3 tools/release/check_runtime_resources.py
```

检查器会从已跟踪的 C++ 字符串常量中识别音效引用，并补充核对策略里声明的动态资源模板。带
`qrc:/sounds/` 的音效必须存在精确 alias；运行时拼接文件名的结算动画通过 `install()` 保留完整
目录结构。源码目录或当前工作目录中的相对路径兜底，只能帮助开发机构建运行，不能证明发行物
包含这些资源。

12 个历史比赛音效按 [ADR 0004](../../docs/decisions/0004-optional-audio-pack.md) 登记为可选资源包，
默认源码归档不携带。检查器会显示可选缺失数量，但不会把静默降级当作核心功能故障；登记与源码引用不一致、证据缺失，
或者可选文件进入索引后缺少 QRC alias，仍会阻断发布。

270 帧结算动画已经加入安装树，但不会整体嵌入 QRC。原因和重新评估条件见
[ADR 0003](../../docs/decisions/0003-external-result-animation-assets.md)。

补充可选语音包时仍需单独完成素材授权、文件有效性、体积和实际播放验证；静态检查通过不替代
这些验收。

## 版本与可执行名称检查

`check_version_contract.py` 读取 Git 索引中的版本来源，并按 `version_policy.json` 核对应用版本、
模拟器版本和可执行名称迁移策略：

```bash
python3 tools/release/check_version_contract.py
```

应用公开版本为 `1.0.0`，模拟器采用独立版本 `0.1.0`。规范名称登记为
`RM26CustomClient`；为避免破坏既有比赛脚本和启动路径，1.x 期间继续使用历史可执行名
`RoboMasterClient2025`，2.0.0 前应完成规范名称迁移并同步各平台入口。

官方协议兼容版本是独立维度。检查器会读取协议清单中的 `2.0.0`，但不会拿它与应用或模拟器
版本比较。调整版本时必须同步修改策略和所有真实来源；不能只改策略文件来获得绿色结果。

## 模拟器 Protobuf 单源检查

`src/network/proto/robomaster.proto` 是客户端与模拟器唯一人工维护的 RoboMaster schema。协议
变化后重新生成 Python 代码：

```bash
python3 tools/release/generate_sim_protobuf.py
```

`check_sim_protobuf_runtime.py` 会拒绝第二份模拟器 schema，临时从 canonical schema 生成
descriptor，并与提交的 `sim/robomaster_pb2.py` 做语义比较；不同 `protoc` 机械补充的
`json_name` 不参与差异判断。检查还覆盖生成器最低 runtime、两份依赖声明和真实 pb2 导入：

```bash
python3 tools/release/check_sim_protobuf_runtime.py
```

检查通过证明双端生成来源一致，不代表全部官方消息已经完成字段审计，也不替代真实赛事联调。
四个已校正消息和 wire golden 见
[Protobuf 单一 Schema 维护说明](../../docs/maintainers/protocol-convergence-plan.md)。

## 公开发布预检

`check_public_readiness.py` 审计 Git 索引中的待提交快照，用于在创建公开仓库或发布版本前发现明确的阻断项。未暂存改动和未跟踪文件不属于索引快照。

```bash
python3 tools/release/check_public_readiness.py
```

退出码为 `0` 表示当前规则通过，`1` 表示仍有发布阻断项，`2` 表示工具自身无法完成检查。报告会给出文件位置和建议处理方式。

GitHub 上的 `Public Readiness` 手动工作流会安装 `ffprobe`，执行媒体元数据、打包契约、运行时
资源、版本契约、公开树、示例配置和文档引用检查。除第一项外，后续步骤使用 `if: always()`，
因此一次手动运行可以收集全部阻断，而不会在首个预期失败处提前结束。准备公开仓库或打版本时，
应保存该工作流的通过记录。打包、运行时资源和版本契约由日常 Quality CI 检查；许可证、素材
权属和完整公开树需要负责人授权，由手动工作流检查。

检查内容包括：

- 根目录许可证是否缺失；
- 本地开发工具配置、临时会话、内部计划、任务记录和运行证据是否误入公开树；
- 内部需求、官方原始资料、手册截图或 OCR 转写是否仍在公开树中；
- 顶层路径是否落在公开树白名单中；
- 个人绝对路径是否被提交；
- 疑似真实现场私网地址和 SSH 元信息是否被提交。
- 素材是否由 `public_assets.json` schema v2 完整覆盖、与登记的 Git 索引快照一致，并获得批准和
  可复核证据。
- 跟踪运行配置时是否同时提供安全示例，以及两份公开配置是否满足本机地址、端口、路径和敏感
  字段规则。

`public_paths.json` 是顶层公开树白名单。新增根文件或顶层目录时必须在同一个 PR 中说明用途并更新清单；白名单不会替代对 `docs/` 等目录内敏感内容的专门规则。

文档改动还应运行：

```bash
python3 tools/release/check_docs.py
```

该检查会验证 Git 索引中 Markdown 的本地链接和 `depends_on` 元数据，外部网址和页内锚点不在检查范围内。

通用文本扫描会忽略回环地址、RFC 5737 文档地址，以及协议知识库中明确标注的赛事固定地址；
可执行配置和脚本仍必须使用安全默认值。运行 `config.json` 和 `config.example.json` 还会由专用
配置规则检查结构、端口、路径和敏感字段。预检不读取 Git 历史，也不会检查未暂存或未跟踪文件；
正式公开前仍需单独执行历史敏感信息扫描。

素材清单的 schema v2 使用 `sha256:path-nul-blob-oid-lf:v1` 算法：把每组路径下全部 stage 0
索引项按路径的 UTF-8 字节排序，依次写入“路径、NUL、blob OID、换行”，再记录文件数和
SHA-256。索引中的新增、删除或内容替换都会使快照失配；未暂存改动和未跟踪文件不参与计算。
组路径不得重叠或为空，索引存在未解决冲突时也不会继续生成可能误导的快照。

这份摘要只固定复核对象，不证明权属，也不会判断两个不同文件是否视觉等价。合法变更仍要先逐项
复核，再更新快照；`approved` 也不是自我声明，必须同时填写合同、作者确认、许可证文件或来源
记录等可复核证据。快照正确但状态仍为 `pending` 时，公开发布预检照样失败。

## 检查器单元测试

修改发布检查器或策略结构后运行：

```bash
python3 -m unittest discover -s tools/release/tests -p 'test_*.py' -v
```

这些测试会建立临时 Git 仓库，覆盖索引优先、未跟踪文件隔离、素材增删替换、运行时资源交付、
版本策略、源码/二进制模式、生成物阻断和路径隐私。测试通过只证明检查器行为正确，不代表当前
仓库已经满足发布条件。
