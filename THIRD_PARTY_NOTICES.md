# 第三方依赖说明

项目自有源代码采用 MIT License。本文件识别项目直接使用或随仓库分发的第三方组件；第三方
组件继续适用各自许可证，不因项目采用 MIT 而改变。本清单不是法律意见。

正式发布前必须按实际版本、构建选项和发行内容逐项复核，并随源码或二进制提供所需的许可证文本、版权声明、源码获取方式和修改说明。

## C++ 客户端直接依赖

| 组件 | 用途 | 上游声明的许可证 | 发布前注意事项 |
|------|------|------------------|----------------|
| [Qt 6](https://www.qt.io/licensing/open-source-lgpl-obligations) | Widgets、QML、网络、多媒体等 | 模块不同，可能为 LGPL-3.0、GPL 或商业许可证 | 固定实际模块和版本，确认动态/静态链接方式并附带相应声明。 |
| Qt HTTP Server | 可选开发观测接口 | 开源版本为 GPL-3.0 或商业许可证 | 正式构建默认不链接，仅在 `RM26_ENABLE_DEVTOOLS=ON` 时启用；对外分发开发构建前仍需确认授权路径。 |
| [FFmpeg](https://ffmpeg.org/legal.html) | H.264/HEVC 解码与媒体处理 | 通常为 LGPL-2.1-or-later；启用特定组件后可能为 GPL-2.0-or-later | 保存准确版本、配置参数和来源，核查是否启用 GPL/nonfree 组件，并满足动态链接、源码和声明要求。 |
| [Protocol Buffers](https://github.com/protocolbuffers/protobuf/blob/main/LICENSE) | 协议序列化 | BSD-3-Clause | 保留许可证和版权声明，记录生成器及运行库版本。 |
| [Abseil](https://github.com/abseil/abseil-cpp/blob/master/LICENSE) | Protobuf 相关基础库 | Apache-2.0 | 随发行包提供许可证和 NOTICE 要求。 |
| [Eclipse Paho MQTT C](https://github.com/eclipse-paho/paho.mqtt.c/blob/master/LICENSE) | MQTT 通信 | EPL-2.0 OR EDL-1.0 | 明确采用的授权路径，并保留对应文本和声明。 |
| [ONNX Runtime](https://github.com/microsoft/onnxruntime/blob/main/LICENSE) | 可选目标检测运行时 | MIT | 仅在启用相关功能时纳入发行清单，并核对模型自身授权。 |
| [OpenCV](https://opencv.org/license/) | 图像和训练工具相关能力 | Apache-2.0，发行包还包含第三方组件 | 根据实际使用和二进制包附带的第三方清单复核。 |

## 模拟器直接依赖

模拟器依赖声明位于 `sim/pyproject.toml` 和 `sim/requirements.txt`。当前未锁定精确版本，以下许可证必须在生成锁文件后再次核对。

| 组件 | 上游常见许可证 | 用途 |
|------|----------------|------|
| FastAPI | MIT | HTTP 服务 |
| Uvicorn | BSD-3-Clause | ASGI 运行时 |
| python-socketio | MIT | 实时网页通信 |
| python-multipart | Apache-2.0 | 表单和上传解析 |
| OpenCV Python | Apache-2.0 及随包第三方声明 | 视频处理 |
| NumPy | BSD-3-Clause | 数值处理 |
| Paho MQTT Python | EPL-2.0 OR EDL-1.0 | MQTT 发布 |
| Protocol Buffers Python | BSD-3-Clause | 协议序列化 |

## 随仓库分发的前端库

| 文件 | 版本 | 声明 |
|------|------|------|
| `sim/server/static/vendor/vue.min.js` | Vue.js 2.7.14 | MIT，文件头保留上游声明；完整文本见 [`LICENSE.vue-2.7.14.txt`](sim/server/static/vendor/LICENSE.vue-2.7.14.txt)。 |
| `sim/server/static/vendor/socket.io.min.js` | Socket.IO 4.5.4 | MIT，文件头保留上游声明；完整文本见 [`LICENSE.socket.io-4.5.4.txt`](sim/server/static/vendor/LICENSE.socket.io-4.5.4.txt)。 |

两份完整 MIT 文本已与对应版本的上游仓库核对并随源码保留。正式发布前仍应确认这些版本的安全
维护状态和对应源码可获得；若改用包管理器，应同步提交锁文件。

## 其他开发依赖

- 当前仓库不捆绑第三方字体；客户端从系统已安装字体中选择可用项。未来如随发行包加入字体，必须同时登记来源、许可证文本和再分发范围。
- `tools/yolo_training/requirements.txt` 包含 Ultralytics、PyTorch、Roboflow 等可选训练依赖。使用或再分发训练工具与模型前，应分别核查对应版本的许可证；不要假定它们自动适用项目许可证。

## 不属于第三方开源依赖的内容

DJI/RoboMaster 官方手册、协议原文、官方客户端截图、界面图片和音效不应因为出现在仓库中就被视为开源依赖或项目授权内容。其公开发布边界见 [ASSET_LICENSES.md](ASSET_LICENSES.md)。

## 正式发布检查

- 固定直接依赖和构建镜像的准确版本；
- 生成并复核源码与二进制 SBOM；
- 保存 Qt、FFmpeg 等实际构建配置；
- 提供所有必须随发行包附带的许可证和版权文本；
- 在应用“关于”页面和下载页展示必要声明；
- 对静态链接、GPL 组件、模型和第三方媒体单独复核；
- 确认本清单与最终发行包内容一致。
