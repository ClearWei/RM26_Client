# RM26 客户端学习路径

这份学习路径面向第一次接触 RM26 Custom Client 的开发者，可以在普通本地环境完成。你将先
理解项目目标，再沿真实数据链路阅读客户端和模拟器，最后完成一次边界清晰、能够复核的贡献。

文中的命令默认从仓库根目录执行。除 CMake 构建目录外，这些命令不会连接或操作真实赛事
环境。协议和现场能力的边界以当前代码、测试以及团队核验的官方资料版本为准。

## 先选择适合你的路径

| 你的目标 | 建议阅读 |
| --- | --- |
| 先判断项目是否值得继续了解 | [10 分钟了解项目](#10-分钟了解项目) |
| 想在没有硬件时理解整个系统 | [30 分钟理解模拟器与客户端边界](#30-分钟理解模拟器与客户端边界) |
| 准备修改协议、状态、界面或视频代码 | [按数据链路读代码](#按数据链路读代码) |
| 想学习 Qt、网络、视频或测试方法 | [按专题深入](#按专题深入) |
| 准备提交 Issue 或 Pull Request | [准备贡献](#准备贡献) |

## 10 分钟了解项目

### 学习目标

完成这一节后，你应该能回答三个问题：

1. 客户端解决了哪些信息展示之外的操作问题；
2. `src/`、`sim/`、`tools/` 分别承担什么职责；
3. 仓库提供哪些验证证据，各项证据适用于哪些环境。

### 阅读顺序

1. 从 [项目 README](../README.md) 了解赛场问题、核心能力和构建入口。
2. 阅读 [架构总览](architecture/overview.md) 的“运行架构”和“模块职责”。
3. 沿[组件职责与数据流](architecture/data-flow.md)选择一条真实消息或视频链路。
4. 阅读 [测试与质量门](architecture/testing.md)，区分可复核结果与尚未覆盖的平台、现场环境。
5. 需要核对协议版本时，查看[官方资料索引](references/official-materials.md)和
   [协议目标清单](../src/network/proto/protocol_manifest.json)。

先记住这条边界：`src/` 提供正式 Qt/C++ 客户端，`sim/` 作为独立协议对端，测试和发布工具
用于离线验证。

### 可以复核的命令

```bash
# 查看仓库提供的开发、发布和观测构建预设
cmake --list-presets

# 检查运行时资源和公开目录是否完整
python3 tools/release/check_runtime_resources.py

# 检查协议清单、schema 和已登记的兼容契约
python3 tools/release/check_sim_protobuf_runtime.py
```

这一步不要求先启动客户端。两项检查用于核对资源清单和模拟器协议契约；真实赛事环境的联调
结果需另行记录。

## 30 分钟理解模拟器与客户端边界

### 学习目标

完成这一节后，你应该能画出模拟器到客户端的 MQTT、UDP 和视频链路，并说明模拟器如何通过
协议与客户端隔离，以及生产协议真相源如何保持唯一。

### 先看组件关系

1. [模拟器说明](../sim/README.md)：环境、启动入口和开发约束。
2. [模拟器架构](architecture/simulator.md)：组件职责、安全边界与演进约束。
3. [系统架构总览](architecture/overview.md)：模拟器在完整系统中的位置。

### 对照源码

| 角色 | 入口 | 阅读重点 |
| --- | --- | --- |
| 模拟器组合入口 | [`sim/server/main.py`](../sim/server/main.py) | FastAPI、Socket.IO 与各组件如何装配 |
| 模拟比赛状态 | [`sim/server/state_manager.py`](../sim/server/state_manager.py) | 状态保存、阶段推进和事件队列 |
| MQTT 协议对端 | [`sim/server/mqtt_publisher.py`](../sim/server/mqtt_publisher.py) | 周期消息、事件消息和上行命令接收 |
| UDP 发送 | [`sim/server/udp_sender.py`](../sim/server/udp_sender.py) | 兼容帧和目标地址 |
| 视频输入 | [`sim/server/video_streamer.py`](../sim/server/video_streamer.py) | 文件或相机数据如何发送 |
| 客户端组合入口 | [`src/ui/MainWindow.cpp`](../src/ui/MainWindow.cpp) | `GameData`、网络、视频和 QML 的组装 |
| 客户端接入层 | [`src/network/NetworkManager.cpp`](../src/network/NetworkManager.cpp) | topic 路由、解析和上行命令 |

修改模拟器时，同时运行 Python、JavaScript 和协议兼容性检查，并把手工启动结果作为补充证据。

### 可以复核的命令

```bash
# 查看模拟器入口实际装配了哪些组件
git grep -n "StateManager\|MQTTPublisher\|UDPSender\|VideoStreamer" -- sim/server/main.py

# 查看客户端组合根创建的领域、网络和视频对象
git grep -n "GameData\|NetworkManager\|VideoReceiver" -- src/ui/MainWindow.cpp

# 确认模拟器生成代码与协议运行时仍兼容
python3 tools/release/check_sim_protobuf_runtime.py
```

若要实际启动模拟器，请严格按 [模拟器说明](../sim/README.md)准备 Python、MQTT Broker 和
本地地址。学习阶段不要把目标地址改成赛事网段，也不要使用真实账号、抓包或现场配置。

## 按数据链路读代码

选择一条真实消息，按“产生—传输—解析—状态—界面”的顺序追踪源码。

### 链路一：比赛状态进入界面

**学习目标：**理解一条 `GameStatus` 消息如何从模拟器或赛事端进入客户端，并最终触发 QML
界面更新。

按下面的顺序阅读：

1. [`robomaster.proto`](../src/network/proto/robomaster.proto) 中的 `GameStatus` 定义；
2. [`mqtt_publisher.py`](../sim/server/mqtt_publisher.py) 的 `_publish_game_status`；
3. [`MqttManager.cpp`](../src/network/MqttManager.cpp) 的 MQTT 回调；
4. [`NetworkManager.cpp`](../src/network/NetworkManager.cpp) 的 `processMqttMessage`；
5. [`GameData.cpp`](../src/core/GameData.cpp) 的 `updateGameStatus`；
6. [`MainWindow.cpp`](../src/ui/MainWindow.cpp) 设置的 QML context；
7. [`TopInfoBar.qml`](../src/qml/TopInfoBar.qml) 或
   [`TacticalCommandPage.qml`](../src/qml/Tactical/TacticalCommandPage.qml) 对状态的读取。

```bash
git grep -n "GameStatus\|processMqttMessage\|updateGameStatus" -- \
  src/network/proto/robomaster.proto \
  sim/server/mqtt_publisher.py \
  src/network/NetworkManager.cpp \
  src/core/GameData.cpp

ctest --preset release -R "TestProtobuf|TestProtocol|TestGameData" --output-on-failure
```

### 链路二：界面命令发回协议对端

**学习目标：**理解 QML 动作如何经过有类型的 C++ 接口、频率限制和 MQTT 发布，使页面与协议
细节保持解耦。

可以从 `CommonCommand` 或 `AssemblyCommand` 选择一条路径：

1. [`ExchangePanel.qml`](../src/qml/ExchangePanel.qml)、
   [`ExchangePanelEngineer.qml`](../src/qml/ExchangePanelEngineer.qml) 或
   [`RobotRespawn.qml`](../src/qml/RobotRespawn.qml) 发起动作；
2. [`NetworkManager.cpp`](../src/network/NetworkManager.cpp) 序列化并执行频率限制；
3. [`MqttManager.cpp`](../src/network/MqttManager.cpp) 发布消息；
4. [`mqtt_publisher.py`](../sim/server/mqtt_publisher.py) 在本地模拟环境接收并解释命令。

```bash
git grep -n "sendCommonCommand\|sendAssemblyCommand\|CommonCommand\|AssemblyCommand" -- \
  src/qml src/network sim/server/mqtt_publisher.py

ctest --preset release -R "TestExchangeCommandPolicy|TestProtobuf|TestMqttManager" \
  --output-on-failure
```

### 链路三：UDP 视频到 Qt 界面

**学习目标：**理解接收、组帧、解码和展示为什么需要分开，以及停止和重连为什么也是视频
功能的一部分。

按下面的顺序阅读：

1. [`VideoReceiver.cpp`](../src/network/VideoReceiver.cpp)：UDP 接收、分片与帧输出；
2. [`H264Decoder.cpp`](../src/network/H264Decoder.cpp) 与
   [`HevcDecoder.cpp`](../src/network/HevcDecoder.cpp)：解码和错误恢复；
3. [`VideoBackgroundWidget.cpp`](../src/widgets/VideoBackgroundWidget.cpp)：原生 Qt 视频承载；
4. [`MainWindow.cpp`](../src/ui/MainWindow.cpp)：视频信号如何进入 Widget 或 `GameData`；
5. [线程与对象生命周期](architecture/threading-model.md)：退出、迟到回调和会话隔离约束。

收包、组帧、解码和绘制阶段的观测方式见
[传输接入与链路诊断](architecture/transport-integration.md)。

```bash
git grep -n "VideoReceiver\|frameDecoded\|imageReceivedH264\|hevcDataReady" -- \
  src/network src/widgets src/ui/MainWindow.cpp

ctest --preset release -R "TestVideoDatagramLogger" --output-on-failure
```

### 链路四：协议事实如何被约束

**学习目标：**区分官方协议事实、仓库 schema、生产解析行为和模拟器兼容实现。

先读[协议边界与真相源](architecture/protocol-boundary.md)，再查看
[`protocol_manifest.json`](../src/network/proto/protocol_manifest.json)和
[`robomaster.proto`](../src/network/proto/robomaster.proto)。客户端与模拟器共用该 schema，
生成方式、兼容修正和字段审计边界记录在
[Protobuf 单一 Schema 维护说明](maintainers/protocol-convergence-plan.md)。

```bash
python3 tools/release/check_sim_protobuf_runtime.py
ctest --preset release -R "TestProtobuf|TestProtocol" --output-on-failure
```

## 按专题深入

### Qt Widgets 与 QML 混合界面

**你会学到：**如何用 Widgets 管理桌面窗口和原生视频，用 QML 快速组织战术地图与状态面板，
并通过 `GameData` 提供统一展示状态。

入口：

- [`MainWindow.cpp`](../src/ui/MainWindow.cpp)
- [`GameData.h`](../src/core/GameData.h)
- [`src/qml/`](../src/qml)
- [架构总览](architecture/overview.md)

```bash
git grep -n "setContextProperty" -- src/ui/MainWindow.cpp
ctest --preset release -R "TestTacticalCommandPage|TestMainWindow" --output-on-failure
```

### 并发、重连与确定性退出

**你会学到：**如何区分网络连通与对象生命周期，并检查后台回调、对象所有权和快速
启动—停止—再启动。

入口：

- [线程与对象生命周期](architecture/threading-model.md)
- [`MqttManager.cpp`](../src/network/MqttManager.cpp)
- [`VideoReceiver.cpp`](../src/network/VideoReceiver.cpp)
- [`test_mqtt_manager.cpp`](../tests/unit/test_mqtt_manager.cpp)

```bash
ctest --preset release -R "TestMqttManager" --output-on-failure
```

### 战术状态与事件展示

**你会学到：**如何从统一比赛状态派生战术信息，以及如何让界面状态具备可测试边界。

入口：

- [`TacticalAnalyzer.cpp`](../src/core/TacticalAnalyzer.cpp)
- [`TacticalCommandPage.qml`](../src/qml/Tactical/TacticalCommandPage.qml)
- [`test_tactical_analyzer.cpp`](../tests/unit/test_tactical_analyzer.cpp)
- [`test_tactical_command_page.cpp`](../tests/unit/test_tactical_command_page.cpp)
- [战术分析的数据边界与演进方向](architecture/tactical-analysis.md)

```bash
ctest --preset release -R "TestTacticalAnalyzer|TestTacticalCommandPage" \
  --output-on-failure
```

### 测试和发布证据

**你会学到：**如何记录绑定提交、平台和依赖版本的可复核运行证据。

入口：

- [测试与质量门](architecture/testing.md)
- [公开发布预检说明](../tools/release/README.md)

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
python3 -m unittest discover -s tools/release/tests -p 'test_*.py' -v
```

公开发布预检会核对根许可证、素材批准状态和固定快照。新增或替换素材后应更新授权台账和摘要，
不要通过删除规则或忽略退出码把发布阻断项伪装成通过。

## 准备贡献

### 学习目标

准备提交改动前，你应该能说明改动属于哪个模块、是否改变可观察行为、需要运行哪些测试，
以及哪些现场条件没有验证。

### 必读入口

1. [贡献指南](../CONTRIBUTING.md)：Issue、PR、安全和素材要求；
2. [编码与提交约定](development/coding-style.md)：中文注释、Qt/C++、Python、QML 和提交习惯；
3. [测试与质量门](architecture/testing.md)：按修改范围选择验证；
4. [架构总览](architecture/overview.md)：确认依赖方向和模块边界。

普通贡献流程只需要 Git、CMake、CTest、Python 和项目依赖。建议先从文档修正、独立纯
逻辑、现有测试补充或边界清晰的小缺陷开始。

### 提交前最小检查

```bash
git diff --check
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
python3 tools/release/check_runtime_resources.py
python3 tools/release/check_sim_protobuf_runtime.py
```

修改文档时，还应在准备提交的 Git 索引上运行：

```bash
python3 tools/release/check_docs.py
```

提交说明应记录动机、行为变化、实际执行的验证和未验证项。涉及协议、网络、视频、输入或
比赛命令时，优先补回归测试；未经明确授权，不要连接、扫描或操作赛事网络和他人设备。

## 学完之后应该具备的地图

学完后，可以用下面五层概括项目：

```text
外部赛事系统或本地模拟器
        ↓ MQTT / UDP / Protobuf / 视频
NetworkManager、MqttManager、VideoReceiver
        ↓ 解析、路由、解码
GameData 与领域逻辑
        ↓ Qt 属性和信号
Widgets / QML 操作界面
        ↓
CTest、模拟器测试和发布检查提供回归证据
```

遇到不确定的实现时，先回到对应链路和测试确认事实，再决定是否修改目录、接口或协议语义。
