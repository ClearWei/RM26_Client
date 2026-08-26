# 贡献指南

感谢你愿意帮助改进 RM26 自定义客户端和配套模拟器。

> 项目源代码采用 [MIT License](LICENSE)。提交贡献即表示你有权提供相关内容，并同意维护者按
> 项目许可证发布；第三方代码、素材和标识仍需遵守各自的授权与署名要求。

本项目由社区团队独立开发，并非 DJI 或 RoboMaster 官方项目，也不代表其立场或承诺。

## 开始之前

- 先搜索已有 issue，避免重复工作。
- 修复明确缺陷可以直接提交小型 PR；新增功能、协议语义调整、目录重组和大范围重构，请先通过 issue 说明目标、边界和验收方式。
- 安全问题不要提交公开 issue，请按照 [SECURITY.md](SECURITY.md) 私下报告。
- 只在你有权使用的模拟或测试环境中验证。未经授权，不要扫描、干扰或操作赛事网络和他人设备。

## 项目边界

请保持以下职责清晰：

- `src/core`：领域状态和面向界面的统一数据投影。
- `src/network`：协议解析、序列化和传输接入。
- `src/ui`、`src/qml`、`src/widgets`：界面容器、展示和交互。
- `src/devhooks`：可选的开发观测接口，不承载业务逻辑。
- `sim`：独立模拟器，用于无赛事引擎时的开发和测试。
- `tests`、`tools/release`：测试和发布检查，不应成为正式客户端的隐性依赖。

协议语义以仓库当前声明的 RoboMaster 2026 通信协议 V2.0.0 兼容目标为准。现场适配不得悄悄改变协议含义，差异应记录在文档和测试中。

## 开发流程

1. 从最新维护分支创建范围明确的分支。
2. 保持提交小而聚焦，不混入构建产物、个人配置、日志和无关格式化。
3. 修改 C++ 行为时补充或更新 QtTest；修改模拟器行为时补充 Python 或 JavaScript 测试。
4. 更新受影响的说明、配置示例和变更记录。
5. 提交 PR，并说明动机、行为变化、验证结果和未验证项。

常用的本地验证命令如下。具体依赖和平台差异以 [README.md](README.md) 及 [docs/README.md](docs/README.md) 为准。
编码、中文注释和提交说明约定见 [docs/development/coding-style.md](docs/development/coding-style.md)。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

模拟器改动当前至少运行协议兼容性与独立测试：

```bash
python3 tools/release/check_sim_protobuf_runtime.py
python3 -m unittest discover -s sim/tests -t sim -p 'test_*.py' -v
node --test sim/tests/test_map_geometry.js
```

模拟器独立测试仍是公开发布门槛。新增或修改比赛规则时，请把对应 Python 或 JavaScript
测试连同 fixture 一起提交，并在 PR 中写出实际命令。如果改动只适用于特定平台或现场
环境，请明确写出没有覆盖的验证范围。

## 文档、配置和素材

- 文档应描述仓库中真实存在的行为，避免把计划写成已实现功能。
- 示例配置必须使用占位地址，不得提交真实 IP、主机名、SSH 用户、密钥路径、账号或现场日志。
- 不要提交官方 PDF、官方客户端截图、音效、字体、视频、队徽或其他来源不明素材。
- 新增素材时，请同时在 [ASSET_LICENSES.md](ASSET_LICENSES.md) 登记来源、作者、授权和修改情况。
- 引入第三方代码或依赖时，请更新 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，并保留上游许可证和版权声明。

## PR 最低要求

- 关联对应 issue，或解释为何不需要 issue。
- 清楚说明是否改变现有功能、协议、配置或界面。
- 给出实际执行过的测试命令和结果。
- 不包含真实现场信息、密钥、个人绝对路径或运行工件。
- 不包含权属不明的第三方代码和素材。
- 引入的第三方内容必须提供可核验的许可证、授权依据和必要署名。

维护者会优先考虑边界清晰、可验证、不会破坏现有赛场功能的改动。
