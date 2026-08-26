# 可选比赛音效包

公开源码默认不包含历史比赛 BGM、倒计时、结算和击杀语音。缺少这些文件时客户端保持静默，比赛
状态、操作、动画和文字播报不受影响。这样可以避免把来源未确认的音频或伪造占位文件带入项目。

## 文件名

| 场景 | 文件 |
|---|---|
| 准备/自检背景音乐 | `min3bgm.mp3` |
| 比赛背景音乐 | `gameBg.mp3` |
| 比赛开始倒计时 | `3-1比赛开始.mp3` |
| 剩余时间提示 | `2自检.mp3` |
| 正常或异常结算 | `20game_finish.mp3`、`game_finish.mp3` |
| 普通击杀、第一滴血 | `dead.wav`、`firstblood.wav` |
| 二至五连杀 | `2kill.wav`、`3kill.wav`、`4kill.wav`、`5kill.wav` |

## 本地使用

使用团队自制或已获授权的音频，并保持上述文件名。运行源码构建时，把文件放到
`resources/sounds/`；也可以放到可执行文件旁的 `resources/sounds/`。击杀语音还会检查当前工作
目录和 macOS Bundle 的 `Contents/Resources/sounds/`。

不要提交来源不明的旧音频。准备把语音随公开源码或安装包发行时，需要同时完成：

1. 在 `ASSET_LICENSES.md` 和素材策略中登记来源、作者、许可证与证据；
2. 运行 `python3 tools/release/check_runtime_resources.py`，补齐检查要求的 QRC alias；
3. 在支持的平台上验证格式、音量、重复播放、阶段切换和静音开关；
4. 重新生成源码 SBOM 和发行校验和。

这套可选资源契约的原因和边界见 [ADR 0004](../decisions/0004-optional-audio-pack.md)。
