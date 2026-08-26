# 源码发行物签名策略

`source` 模式提供固定提交生成的源码归档，不包含经过平台验收的 macOS、Windows、Linux 安装包
或模拟器 wheel。源码归档不使用项目签名密钥，也不声明代码签名或平台公证。

每个源码归档必须同时提供：

- 由 `tools/release/generate_source_sbom.py` 生成的 SPDX 2.3 JSON；
- 由 `tools/release/generate_checksums.py` 生成的 `SHA256SUMS`；
- 发布提交 SHA、CI 记录、许可证和第三方声明；
- 从公开远端重新下载后完成的 SHA-256 复核结果。

该策略只适用于源码归档。若提供 `.app`、MSI、AppImage、容器镜像或其他可执行产物，需为对应
平台确定密钥保管、签名、公证、撤销和产物证明策略，并更新机器可读发布策略。
