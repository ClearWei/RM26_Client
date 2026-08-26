# RM26 Custom Client — RoboMaster 自定义客户端与模拟器

[简体中文](README.md) · [English](README.en.md)

面向 RoboMaster 赛场的自定义操作客户端与战场信息终端。它把赛事状态、机器人、雷达和
多路图传组织成一张可操作的战场态势，并提供独立模拟器，让没有完整场地和实车的开发者也能
调试主要数据链路。

项目由复旦大学星云 EGA 在 2026 赛季的训练和比赛中持续迭代，围绕操作手注意力、异常降级和
可验证开发形成了一套完整的桌面端系统。

> [!IMPORTANT]
> 项目源代码采用 [MIT License](LICENSE)。随本次源码发布的素材已由维护者确认可以公开，
> 具体范围见[素材来源与授权清单](ASSET_LICENSES.md)；第三方名称和标识仍归其权利人所有。

本项目由社区团队独立开发，与 DJI 或 RoboMaster 无隶属或官方合作关系。RoboMaster、DJI 及
相关标识的权利归其各自权利人所有。

## 一分钟了解项目

| 这个项目关心什么 | 对应做法 |
| --- | --- |
| 操作手能否看清全局 | 将比赛状态、敌我位置、生命值、关键事件和数据时效性集中到战术视图 |
| 重要信息能否及时到达 | 按角色和紧急程度组织界面、声音与分层提示，减少操作手主动搜索信息的次数 |
| 主视图异常后能否继续操作 | 在已接入备用数据源时提供替代视图，条件恢复后回到战术视图 |
| 没有完整赛场时能否继续开发 | 使用独立协议对端模拟比赛状态、命令和图传，客户端保持正常的数据链路 |
| 重构后功能是否仍然可信 | 用客户端测试、模拟器测试和发布检查记录行为证据，再逐步整理边界 |

## 选择你的入口

| 你现在想做什么 | 推荐入口 | 看完能得到什么 |
| --- | --- | --- |
| 先判断项目是否适合自己 | 继续阅读[核心能力](#核心能力)和[适用范围](#发布内容与适用范围) | 功能范围和使用条件 |
| 构建并运行客户端 | [快速开始](#快速开始) | 可执行程序和最基本的本地运行环境 |
| 单独体验赛事数据模拟 | [启动模拟器](#3-启动模拟器) | 一个可从浏览器控制的独立协议对端 |
| 读懂架构和关键数据链路 | [学习路径](docs/learning-path.md) | 从消息接入到 QML 展示的源码阅读顺序 |
| 准备提交改动 | [贡献指南](CONTRIBUTING.md) | 开发、测试、安全和提交要求 |

## 为什么做这个项目

比赛现场数据很多，操作手能分配给界面的注意力却很有限。传统界面往往要求操作手在多个数据源
之间主动寻找答案；RM26 Custom Client 尝试把信息整理成统一战场态势，再按角色和紧急程度
进行展示、提示与切换：

- **看得全**：融合敌我位置、生命值、比赛状态、关键事件和数据时效性。
- **知道快**：通过战术地图、声音和分层提示，把重要信息主动交给操作手。
- **不断档**：当已经接入可用的备用数据源时，在主视图异常后提供替代视图；实际可用性仍取决于现场链路和配置。

这套系统已经在 2026 赛季的训练和比赛环境中投入使用，接受过真实工作流的检验。延迟、稳定性
和成绩相关结论以[验证说明](docs/development/verification.md)中的可复核记录为准。

## 核心能力

| 能力 | 说明 |
| --- | --- |
| 战术态势 | 全屏战术地图、红蓝方视角、位置与生命值融合、数据过期提示 |
| 操作界面 | Qt Widgets 与 QML 混合界面，支持角色化布局和多分辨率适配 |
| 赛事通信 | MQTT、UDP 与 Protobuf 数据链路，覆盖状态接收和赛事命令发送 |
| 图传链路 | UDP 分片接收、H.264/H.265 解码、面向低延迟的显示链路与异常恢复 |
| 事件提醒 | 基地、前哨、飞镖、能量机关等关键状态的视觉与声音提示 |
| 独立模拟器 | FastAPI、Socket.IO 驱动的 Web 控制台，可模拟比赛状态、命令和图传 |
| 工程验证 | C++/Qt 测试、模拟器测试，以及配置、文档和发布完整性检查 |

## 客户端、模拟器与工具的边界

- `src/` 是正式 Qt/C++ 客户端，负责网络接入、统一比赛状态、视频处理和操作界面。
- `sim/` 是可以独立启动的协议对端，用于在非赛场环境提供可控输入；真实裁判系统联调仍需单独进行。
- `tests/` 和 `tools/release/` 属于开发与发布验证层，正式客户端运行不依赖这些目录。
- 真实比赛命令和远端现场动作必须由操作者明确授权，自动化工具不会自行执行。

## 系统概览

```mermaid
flowchart LR
    Engine["赛事引擎 / 雷达 / 机器人"] --> Transport["MQTT / UDP / Protobuf"]
    Transport --> Network["NetworkManager<br/>接入与协议路由"]
    Network --> State["GameData<br/>统一比赛状态"]
    State --> UI["Widgets + QML<br/>战术与操作界面"]
    Video["相机 / 视频源"] --> Pipeline["UDP 接收 / 解码 / 恢复"]
    Pipeline --> UI
    Simulator["独立模拟器"] -. "开发与回归测试" .-> Transport
    Tools["测试 / 发布检查"] -. "质量验证" .-> UI
```

组件职责、线程归属和数据流见[架构概览](docs/architecture/overview.md)；协议版本和官方资料来源见
[协议边界](docs/architecture/protocol-boundary.md)。

## 快速开始

下面给出从全新克隆到本地联调的最短路径。完成客户端构建后，让客户端与模拟器连接同一个
MQTT Broker，即可复现主要数据链路。公开示例均使用回环地址，不需要裁判系统或比赛网络。

### 1. 准备依赖

最低要求：

- CMake 3.21 或更高版本（使用 Presets）；显式配置方式最低支持 3.16
- 支持 C++17 的编译器
- Qt 6：Core、Concurrent、Widgets、Multimedia、Network、SerialPort、Svg、Quick、QuickWidgets、QuickControls2；构建测试还需要 Test
- Protobuf 与 Abseil
- Paho MQTT C（缺失时 MQTT 功能不启用）
- FFmpeg（可选，缺失时 H.264/H.265 解码功能受限）

macOS 可使用 Homebrew：

```bash
brew install cmake qt@6 protobuf abseil libpaho-mqtt ffmpeg
```

Ubuntu 24.04 的完整依赖列表可参考
[docker/client.Dockerfile](docker/client.Dockerfile)。

### 2. 构建客户端

全新克隆先从公开示例生成本地配置。已有现场配置的团队工作区不要直接覆盖：

```bash
# macOS / Linux
cp config.example.json config.json
python3 tools/release/check_example_config.py config.json

# Windows PowerShell
Copy-Item config.example.json config.json -Force
python tools/release/check_example_config.py config.json
```

示例只连接 `127.0.0.1`，不包含模型、视频或现场身份。`config.json` 是被 Git 忽略的本地文件，
只提交 `config.example.json`。字段、代码回退值、环境变量和配置加载位置见
[配置说明](docs/getting-started/configuration.md)。根目录没有 `config.json` 时，CMake 会从公开示例
生成构建目录中的安全运行配置；需要联调时仍建议先复制并显式核对参数。
Docker 构建也会排除本地配置；使用 Compose 启动前必须先准备根目录 `config.json`，运行时以只读
方式挂载，避免现场参数写进镜像层。

本地联调需要对齐三组值：客户端与模拟器使用同一 MQTT Broker 和机器人 ID；主图传监听
`video.stream_url` 中的 UDP 端口；英雄工业相机复用 MQTT `CustomByteBlock`，不需要第二个 UDP
端口。

使用仓库预设：

```bash
cmake --preset release
cmake --build --preset release
```

也可以显式指定构建目录和选项：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DRM26_ENABLE_DEVTOOLS=OFF
cmake --build build --parallel
```

可执行目标沿用兼容名称 `RoboMasterClient2025`。1.x 系列保持该名称，以兼容已有脚本和打包
流程；规范名称迁移将在主版本升级时处理。

运行方式：

```bash
# 使用 release preset 时（Linux）
./build/release/RoboMasterClient2025

# 使用显式 build 目录时（Linux）
./build/RoboMasterClient2025

# 使用 release preset 时（macOS）
./build/release/RoboMasterClient2025.app/Contents/MacOS/RoboMasterClient2025

# 使用显式 build 目录时（macOS）
./build/RoboMasterClient2025.app/Contents/MacOS/RoboMasterClient2025

# Windows + Visual Studio 多配置生成器
.\build\release\Release\RoboMasterClient2025.exe

# Windows + Ninja 单配置生成器
.\build\release\RoboMasterClient2025.exe
```

首次运行前请再次检查实际加载的 `config.json`。它只属于本地运行环境；真实 IP、账号、密钥、
抓包或比赛运行记录不得进入提交。

构建成功后应能在对应目录找到 `RoboMasterClient2025` 可执行目标。没有连接数据源时，客户端会
正常显示空状态；启动模拟器或连接测试 Broker 后，界面才会收到比赛数据。

### 3. 启动模拟器

模拟器要求 Python 3.11 或更高版本。下面显式使用公开示例中的 MQTT `127.0.0.1:3333` 和机器人
ID `1`；启动脚本会复用现有 Broker，或尝试启动本机 Mosquitto。

```bash
python3 -m venv sim/.venv
source sim/.venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -e sim
./sim/run_sim.sh \
  --no-video \
  --mqtt-host 127.0.0.1 \
  --mqtt-port 3333 \
  --current-robot-id 1
```

启动后访问 `http://127.0.0.1:8000`。参数、端口和独立测试方法见
[模拟器说明](sim/README.md)。模拟主图传时使用自己有权使用的视频执行
`--video-file <path>`，它会向客户端 UDP `3334` 发送 HEVC；第二路工业相机在 Web 控制台中选择
视频并启动，经同一 Broker 的 `CustomByteBlock` 发送 H.264。完整对照见
[两路视频配置](docs/getting-started/configuration.md#两路视频怎样配置)。

## 验证

```bash
# C++ / Qt 测试
cmake --build --preset release
ctest --preset release

# 公开配置与发布工具
python3 tools/release/check_example_config.py
python3 -m unittest discover -s tools/release/tests -p 'test_*.py' -v

# 模拟器独立测试
python3 -m unittest discover -s sim/tests -t sim -p 'test_*.py' -v
node --test sim/tests/test_map_geometry.js

# 文档、资源与发布快照检查
python3 tools/release/check_docs.py
python3 tools/release/check_runtime_resources.py
python3 tools/release/check_public_readiness.py
```

各类改动的必跑项、验证基线和适用范围见
[验证说明](docs/development/verification.md)。测试失败时请保留完整命令、平台、构建选项和日志，
不要只记录“本机不通过”。

自动化测试覆盖模拟器赛事推进、地图坐标转换和协议兼容性，并随 Quality CI 运行。涉及 Broker、
视频流和客户端显示的改动，还应补充隔离集成或端到端验证。

### 验证范围

Quality CI 在 Ubuntu 24.04 上执行仓库检查、QML 检查、原生构建、CTest、发布工具和模拟器测试。
Windows、Linux 桌面长期运行、真实裁判系统、网络抖动和完整赛场动作需要在目标环境中另外验证。
各项检查的命令、适用环境和参考记录见[验证说明](docs/development/verification.md)。

## 可以从项目中学习什么

仓库同时保留界面实现、关键数据链路、测试和架构说明，适合按以下主题阅读：

- **端到端状态链路**：一条 MQTT/Protobuf 消息如何进入网络层、写入 `GameData`，再驱动
  Widgets 与 QML 更新。
- **Qt 混合桌面架构**：主窗口、原生控件和 QML 战术视图如何分工，以及线程和生命周期怎样收口。
- **低延迟视频链路**：UDP 分片、组帧、FFmpeg 解码、异常恢复和界面显示之间如何协作。
- **可控的无实车开发**：独立模拟器、自动化测试、协议兼容性检查和人工授权边界如何降低回归风险。

按目标阅读的源码入口、概念文档和练习建议见[学习路径](docs/learning-path.md)。

## 仓库结构

```text
.
├── src/                  # 正式 Qt/C++ 客户端
│   ├── core/             # 比赛状态与领域数据
│   ├── network/          # MQTT、UDP、协议与视频链路
│   ├── ui/               # 主窗口和应用编排
│   ├── widgets/          # 原生 Qt 控件
│   ├── qml/              # 战术地图和交互面板
│   └── devhooks/         # 可选开发观测接口
├── sim/                  # 独立 Python/Web 模拟器
├── tests/                # C++/Qt 测试
├── tools/release/        # 配置、资源、文档与发布检查
├── docs/                 # 用户、架构、协议和维护者文档
├── resources/            # 客户端运行资源，公开范围见 ASSET_LICENSES.md
└── CMakeLists.txt
```

文档导航见 [docs/README.md](docs/README.md)。

## 贡献

欢迎提交可复现的问题、测试和小步改进。涉及协议、网络、图传或比赛命令的修改，必须同时
提供行为证据和回归测试；目录迁移、接口改写与功能变化应拆成不同提交。

开始前请阅读：

- [贡献指南](CONTRIBUTING.md)
- [使用与问题支持](SUPPORT.md)
- [安全策略](SECURITY.md)
- [行为准则](CODE_OF_CONDUCT.md)
- [路线图](ROADMAP.md)
- [架构决策](docs/decisions/README.md)

项目采用 MIT License。提交贡献即表示你有权提供相关代码、文档或素材，并同意维护者按本项目
许可证发布；第三方内容仍需保留其原始许可证和署名要求。

## 发布内容与适用范围

本项目以源码形式发布。使用者需要按[快速开始](#快速开始)安装依赖并完成构建；仓库不提供预编译的
macOS、Windows、Linux 安装包或模拟器 wheel。

| 事项 | 说明 | 使用提示 |
| --- | --- | --- |
| 项目许可证 | MIT | 源代码可按 [LICENSE](LICENSE) 使用、修改和再分发 |
| 发行方式 | 仅源码归档 | 不提供下载即用的客户端安装包或模拟器 wheel |
| 素材权属 | 10 组素材快照列入公开台账 | 授权范围以 [ASSET_LICENSES.md](ASSET_LICENSES.md) 登记的固定快照为准 |
| 仓库历史 | 公开仓库从审阅后的源码快照开始 | 内部研发记录、任务文件和本地日志不属于开源内容 |
| 模拟器测试 | 提供 Python 与 JavaScript 测试 | Broker、视频和客户端显示需要另外完成端到端验证 |
| 验证范围 | macOS 本机构建与 Ubuntu 24.04 CI | 其他系统、网络和比赛环境应按实际部署条件复核 |

发布模式和验收条件见[开源发行与打包契约](docs/maintainers/packaging-contract.md)，素材状态见
[ASSET_LICENSES.md](ASSET_LICENSES.md)。构建运行可从[快速开始](#快速开始)继续，理解设计与代码可从
[文档首页](docs/README.md)进入；准备提交改动前，请阅读贡献与安全说明。
