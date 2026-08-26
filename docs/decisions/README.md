# 架构决策记录

架构决策记录（ADR）用于保存会长期影响代码边界、兼容性或发布方式的选择。它不替代代码
评审和测试，但能说明当时为什么这样做，以及什么条件下应重新评估。

## 状态

- `proposed`：正在讨论，不能作为既定约束。
- `accepted`：当前采用，后续修改应遵守。
- `superseded`：已被新的 ADR 替代，历史原因仍保留。
- `deprecated`：不再推荐，但尚未完成迁移。

## 列表

| ADR | 状态 | 决策 |
| --- | --- | --- |
| [0001](0001-incremental-hardening.md) | accepted | 采用行为优先的渐进式开源重构 |
| [0002](0002-versioned-example-config.md) | accepted | 只跟踪公开示例配置，构建时生成运行副本 |
| [0003](0003-external-result-animation-assets.md) | accepted | 结算动画使用外部资源目录交付，不整体嵌入 QRC |
| [0004](0004-optional-audio-pack.md) | accepted | 历史比赛音效作为可选资源包，不恢复来源不明文件 |

## 新增 ADR

文件名使用 `NNNN-short-name.md`，正文至少包含背景、决策、结果、备选方案和重新评估条件。
只记录有实际取舍的决定，不把普通实现说明包装成 ADR。
