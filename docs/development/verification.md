---
title: 验证说明
status: active
owners:
  - clear
updated_on: 2026-08-25
depends_on:
  - ../../CMakePresets.json
  - ../../tools/release/packaging_policy.json
  - ../architecture/testing.md
---

# 验证说明

本页给出新贡献者和发布维护者可直接执行的验证入口。检查结果只对被测快照、平台和依赖有效，
不代表所有操作系统、网络和赛场环境都已验收。

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

## 最近一次候选快照

复核日期：2026-08-26。

本地环境：macOS 26.5.2、Apple Clang 17.0.0、CMake 3.30.2、Qt 6.9.3。
持续集成环境：GitHub Actions Ubuntu 24.04、系统 Qt 6.4。

| 检查 | 结果 |
| --- | --- |
| Release 配置与编译 | 通过 |
| CTest | 27/27 通过 |
| 发布工具单测 | 118/118 通过 |
| 模拟器 Python 测试 | 28/28 通过 |
| 模拟器 JavaScript 测试 | 3/3 通过 |
| QML 静态检查 | 63 个文件，0 个诊断 |
| 打包、运行时资源、版本与媒体元数据检查 | 通过 |
| Ubuntu 原生构建与 CTest | 通过 |

当前未覆盖 Windows、Linux 桌面实机长期运行、真实裁判系统、完整网络抖动和全部现场动作。
Ubuntu 24.04 当前覆盖 CI 构建与自动测试，不等同于现场运行验收。许可证与当前 10 组素材快照
已经完成公开登记；未来素材变更仍需重新复核，现有结果也不能外推到其他平台和现场环境。

## 提交验证证据

Issue 或 Pull Request 中至少写明：被测提交、操作系统、编译器、Qt 与 CMake 版本、实际命令、结果以及
未验证项。视频链路还应记录输入编码、分辨率、帧率和异常条件；现场问题应在获得授权后保留发送端与
接收端证据，并在公开前脱敏。
