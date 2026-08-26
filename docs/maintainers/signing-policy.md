---
title: 首个源码发行版签名策略
status: active
owners:
  - clear
updated_on: 2026-08-23
depends_on:
  - packaging-contract.md
  - public-release-runbook.md
  - ../../tools/release/packaging_policy.json
---

# 首个源码发行版签名策略

首个公开版本只提供来自无历史公开镜像固定提交的源码归档，不提供已经验收的 macOS、Windows、
Linux 安装包或模拟器 wheel。源码归档暂不使用项目签名密钥，也不声明代码签名或平台公证。

每个源码归档必须同时提供：

- 由 `tools/release/generate_source_sbom.py` 生成的 SPDX 2.3 JSON；
- 由 `tools/release/generate_checksums.py` 生成的 `SHA256SUMS`；
- 发布提交 SHA、CI 记录、许可证和第三方声明；
- 从公开远端重新下载后完成的 SHA-256 复核结果。

“暂不签名”只适用于首个源码归档，不适用于未来二进制发行。后续若提供 `.app`、MSI、AppImage、
容器镜像或其他可执行产物，必须重新确定密钥保管、签名、公证、撤销和产物证明策略，并更新机器
可读发布策略。
