# 验证说明

本页给出新贡献者和发布维护者可直接执行的验证入口。每次结果都应绑定被测提交、平台和依赖
版本，适用范围以记录中的环境为准。

## 普通改动

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
git diff --check
```

修改协议、网络、模拟器或发布路径时，再执行：

```bash
python3 tools/release/check_sim_protobuf_runtime.py
python3 -m unittest discover -s tools/release/tests -p 'test_*.py' -v
python3 -m unittest discover -s sim/tests -t sim -p 'test_*.py' -v
node --test sim/tests/test_map_geometry.js
```

## 公开配置与发布快照

```bash
python3 tools/release/check_example_config.py
python3 tools/release/check_docs.py
python3 tools/release/check_packaging_readiness.py
python3 tools/release/check_runtime_resources.py
python3 tools/release/check_version_contract.py
python3 tools/release/check_media_metadata.py
python3 tools/release/check_public_readiness.py
```

这些检查读取 Git 索引中的快照。发布前应先在导出目录执行 `git add --all`，再运行同一组命令，
防止未跟踪文件或本地差异干扰结论。许可证和素材快照变更后必须重新复核，不应通过删除检查规则
或沿用旧摘要绕过门禁。

## 验证范围

Quality 工作流在 Ubuntu 24.04 上执行仓库检查、QML 检查、客户端构建、CTest、发布工具测试和
模拟器测试。工作流的实际结果可在仓库 Actions 页面查看；本地运行同一命令时，应同时记录操作
系统、编译器、Qt、CMake 和其他关键依赖版本。

Windows、Linux 桌面长期运行、真实裁判系统、网络抖动和现场动作需要在对应环境中单独验证。
素材授权仅覆盖 [ASSET_LICENSES.md](../../ASSET_LICENSES.md) 登记的固定快照，素材变化后应重新复核。

## 提交验证证据

Issue 或 Pull Request 中至少写明：被测提交、操作系统、编译器、Qt 与 CMake 版本、实际命令、结果以及
未验证项。视频链路还应记录输入编码、分辨率、帧率和异常条件；现场问题应在获得授权后保留发送端与
接收端证据，并在公开前脱敏。
