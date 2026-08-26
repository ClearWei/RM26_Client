# 协议边界与真相源

## 权威顺序

协议实现出现冲突时，按以下顺序判断：

1. 经团队核验的官方 2026 通信协议版本和勘误。
2. 仓库 canonical schema：`src/network/proto/robomaster.proto`。
3. 经过测试的 C++ 解析、路由和 `GameData` 投影。
4. 模拟器、历史文档、注释和临时脚本。

官方资料是否可以随仓库再分发，与其是否属于语义真相源是两件事。公开仓库优先保存版本、
来源链接、哈希和团队独立编写的兼容说明，不默认打包官方 PDF。

## 一条消息的完整契约

每个 topic 或 UDP 命令至少记录：

| 项目 | 内容 |
| --- | --- |
| 方向 | 赛事端到客户端，或客户端到赛事端 |
| transport | MQTT、UDP 或兼容路径 |
| topic/command ID | 完整模板、阵营与机器人 ID 变量 |
| payload | Protobuf 消息名与字段 presence 语义 |
| 频率 | 上限、默认发送频率与限流位置 |
| 状态落点 | `NetworkManager` handler 与 `GameData` 属性 |
| UI/动作落点 | 读取页面或有类型 action |
| 证据 | 官方条目、golden payload、双端测试 |

只写“支持某消息”不足以作为契约。

## 生成与验证目标

- C++ 与 Python 都从同一 schema 生成，不手工维护两个等价消息定义。
- 生成文件的版本和生成命令可复现。
- descriptor 测试检查字段编号、类型、optional/repeated 和 enum 值。
- golden payload 同时经过 C++ 与 Python 解析，验证跨端兼容。
- 协议审计覆盖 `src/`、`sim/` 和公开文档，不只扫描生产 proto。

客户端和模拟器均由仓库内的 canonical schema 生成协议代码。CI 使用 descriptor 对比和
wire golden 检查字段及字节兼容性；生成与验证方法见
[Protobuf 单一 Schema 维护说明](../maintainers/protocol-convergence-plan.md)。

机器可读的兼容目标见
[protocol_manifest.json](../../src/network/proto/protocol_manifest.json)。
