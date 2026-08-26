# Protobuf 单一 Schema 维护说明

生产客户端与 Python 模拟器使用同一份 canonical schema。协议语义以团队核验的 RoboMaster
2026 通信协议 V2.0.0 为第一真相源；版本、官方入口和校验值见
[官方资料索引](../references/official-materials.md)。字段审计和真实赛事环境联调按各自证据单独记录。

## 代码结构

| 角色 | 文件 | 维护方式 |
| --- | --- | --- |
| canonical schema | `src/network/proto/robomaster.proto` | 唯一人工维护的 RoboMaster schema |
| C++ 生成代码 | 构建目录中的 `robomaster.pb.cc/.h` | CMake 在构建时从 canonical schema 生成 |
| Python 生成代码 | `sim/robomaster_pb2.py` | 由仓库生成脚本从 canonical schema 生成并提交 |

模拟器不维护单独的协议子集。新增或修正消息时，先修改 canonical schema，再同时验证 C++ 与
Python 生成代码。

## Wire 兼容说明

核验依据是 V2.0.0 的 2.2.3、2.2.14、2.2.21 和 2.2.30 节：

- `GameStatus #1-#10` 按官方定义使用 explicit optional，并补齐 `game_result #9`、
  `end_reason #10`；非结算阶段发送 `255/255`，结算阶段读取 `StateManager` 的最终结果。
- `Buff #1-#5` 使用 explicit optional；删除非官方的 `msg_params #6`，同时 reservation 字段号
  和名称，避免后续误复用旧线格式。
- `AssemblyCommand #1/#2` 保持 explicit optional，`operation=0` 的 wire payload 固定为
  `08001001`。
- `SentryStatusSync #1-#3` 使用 explicit optional，补齐并发布 `is_powered #3`。

这些修改既校正了字段集合，也修复了两个运行语义问题：模拟器不再根据比赛中实时比分猜测胜负，
哨兵强化状态不再只停留在 Web 和 `StateManager` 中。

## 生成与防漂移

在仓库根目录重新生成 Python 代码：

```bash
python3 tools/release/generate_sim_protobuf.py
```

提交的 Python 生成代码基于 `protoc 33.2`，要求 `protobuf>=6.33.2,<7`。Quality CI 运行：

```bash
python3 tools/release/check_sim_protobuf_runtime.py
```

检查器会完成以下复核：

1. 拒绝重新出现 `sim/robomaster.proto`；
2. 从提交的 pb2 提取文件 descriptor；
3. 临时调用 `protoc` 从 canonical schema 生成 descriptor；
4. 忽略不同生成器机械补充的 `json_name` 后比较其余 schema 语义；
5. 核对生成器最低 runtime、两份 Python 依赖声明和当前实际 runtime；
6. 真实导入 pb2，让 Protobuf 自带的兼容检查生效。

## 兼容性检查

| 层级 | 检查内容 |
| --- | --- |
| Descriptor | Python/C++ 检查四个消息的字段、编号和 presence |
| Wire golden | `GameStatus`、`Buff`、`AssemblyCommand`、`SentryStatusSync` 双端字节测试 |
| 模拟器行为 | fake sink 检查未结算哨兵值、最终结果和 `is_powered` |
| Python | 模拟器赛事推进与消息测试 |
| C++ | 客户端生成、编译及协议相关 CTest |

装配命令的 explicit-zero 兼容性由 C++/Python wire golden 检查。真实赛事动作必须在操作者明确
授权后验证。

## 维护规则

- 继续对 V2.0.0 全部 MQTT message 做字段类型和 optional presence 审计；任何线格式变化都补
  descriptor 与 golden payload。
- Windows、Linux 和真实赛事引擎仍按平台与现场验证矩阵补证据。
- 官方协议或勘误升级时，先更新协议清单和兼容测试，再重新生成双端代码。

相关职责边界见[协议边界与真相源](../architecture/protocol-boundary.md)。
