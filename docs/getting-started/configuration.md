# 配置说明

客户端从根目录的 `config.json` 读取本地运行参数；公开仓库只提供脱敏的
`config.example.json`。第一次构建前先复制一份，再按自己的本机环境修改。赛事地址、机器人
身份、账号、密钥、本地绝对路径和比赛记录都不应进入提交。

## 先生成本地配置

macOS 或 Linux：

```bash
cp config.example.json config.json
python3 tools/release/check_example_config.py config.json
```

Windows PowerShell：

```powershell
Copy-Item config.example.json config.json -Force
python tools/release/check_example_config.py config.json
```

`config.example.json` 是公开模板，可以提交；`config.json` 是本地运行文件，应由 Git 忽略，
不要提交。示例检查器会要求回环地址、仓库相对路径和中性身份，因此它适合检查“本机安全
配置”，不适合拿来验证现场配置。

建议在第一次运行 CMake 配置前完成复制。根目录没有 `config.json` 时，CMake 会用
`config.example.json` 在构建目录生成安全配置，因此干净克隆仍可构建；主动复制一份的好处是
本地修改更直观。构建流程会把最终运行配置放到可执行文件旁边。修改网络或视频参数后，最稳妥的
做法是重新构建或同步配置，并重启客户端。

### Docker 配置

Docker 构建上下文同样忽略 `config.json`，镜像只会使用公开示例完成编译，不会把本机现场参数写进
镜像层。通过 `run_docker.sh` 或 Compose 启动客户端前，应先生成本地配置；Compose 会把它只读挂载
到容器内：

```bash
cp config.example.json config.json
./run_docker.sh
```

如果直接使用 `docker compose`，也必须先准备根目录 `config.json`。不要把真实配置写进 Dockerfile、
镜像参数或可共享的 `.env` 文件；现场变量应只在受控主机上设置。

## 客户端会到哪里找配置

`ConfigManager` 默认请求 `config.json`，`ConfigLoader` 依次检查：

1. 当前工作目录下的 `config.json`；
2. 可执行文件所在目录；
3. macOS 应用包的 `Contents/Resources`；
4. 当前工作目录的上一级目录。

日志中的“找到配置文件”会给出最终路径。客户端会监视已加载文件并尝试重新读取，但 MQTT、UDP
和视频服务不会因为每个字段变化而完整重建连接；修改通信参数后请重启客户端，避免新旧连接
混用。

## 配置优先级

网络地址、端口、机器人 ID、主图传 URL 和工业相机网格按以下顺序取值：

1. 对应的非空 `RM_*` 环境变量；
2. 当前加载的 `config.json`；
3. `ConfigManager` 中的回退值。

窗口、界面文字、机器人描述和 AR 参数没有通用环境变量覆盖。环境变量端口只有在能够转换为
`0` 到 `65535` 的整数时才会被当前代码接受；实际通信请使用 `1` 到 `65535`。公开示例检查器
会执行更严格的类型、端口、回环地址、相对路径和敏感字段检查。

### 可用环境变量

| 环境变量 | 覆盖字段 | 公开示例值 |
| --- | --- | --- |
| `RM_SERVER_IP` | `network.server_ip` | `127.0.0.1` |
| `RM_SERVER_PORT` | `network.server_port` | `3333` |
| `RM_CLIENT_PORT` | `network.client_port` | `3333` |
| `RM_VIDEO_PORT` | `network.video_port` | `3334` |
| `RM_MQTT_BROKER` | `network.mqtt_broker` | `127.0.0.1` |
| `RM_MQTT_PORT` | `network.mqtt_port` | `3333` |
| `RM_CLIENT_ROBOT_ID` | `network.client_robot_id` | `1` |
| `RM_VIDEO_STREAM_URL` | `video.stream_url` | `udp://0.0.0.0:3334` |
| `RM_HERO_CAMERA_GRID` | `video.industrial_camera_grid` | 未设置，读取 JSON 中的 `false` |

`RM_CLIENT_ROBOT_ID` 与 JSON 字段使用相同范围：红方 `1`–`7`、蓝方 `101`–`107`。其他值会被忽略，
客户端回到本地配置或安全默认身份，避免界面标签与 MQTT client ID 静默错位。

`RM_HERO_CAMERA_GRID` 的 `1`、`true`、`yes`、`on`（不区分大小写）表示开启；其他非空值表示
关闭。`0.0.0.0` 只表示 UDP 接收端监听本机所有网卡，不能作为服务器、Broker 或发送目标。
当 `video.stream_url` 已显式填写时，单独设置 `RM_VIDEO_PORT` 不会重写 URL 中的端口；需要临时
切换主图传端口时，请直接设置 `RM_VIDEO_STREAM_URL`。

## 字段对照

下表中的“代码回退值”来自当前 `ConfigManager`，与公开示例值是两个概念。只要完整复制示例，
运行时通常不会触发这些回退值。

### `app_settings`、`window` 与 `ui_text`

| 字段 | 公开示例值 | 当前用途 |
| --- | --- | --- |
| `app_settings.client_name` | `RM26 Custom Client` | 兼容元数据访问器；当前 Qt 应用元数据仍在入口代码中设置 |
| `app_settings.version` | `1.0.0` | 配置版本访问器与版本一致性检查，代码回退同为 `1.0.0` |
| `window.min_width` / `min_height` | `1280` / `720` | 主窗口最小尺寸，代码回退分别为 `1280`、`720` |
| `window.default_width` / `default_height` | `1920` / `1080` | 保留的兼容字段；当前启动尺寸按屏幕计算 |
| `window.fullscreen` | `true` | 保留的兼容字段；当前程序入口固定全屏启动 |
| `ui_text.window_title` | `RM26 Custom Client` | 当前主窗口标题 |
| `ui_text` 其余文本 | 见示例文件 | 兼容界面文案；当前主要界面尚未全部通过这些键取值 |

JSON 中的 `_comment` 只是给使用者看的说明，客户端不会把它当作配置项处理。

### `network`

| 字段 | 公开示例值 | 代码回退值 | 当前用途 |
| --- | --- | --- | --- |
| `server_ip` | `127.0.0.1` | `127.0.0.1` | UDP 兼容链路和命令目标地址 |
| `server_port` | `3333` | `20000` | 服务端 UDP 目标端口 |
| `client_port` | `3333` | `10000` | 未启用 MQTT 时的客户端 UDP 监听端口 |
| `video_port` | `3334` | `3334` | `stream_url` 缺失时生成主图传 UDP URL 的端口；公开检查要求两者一致 |
| `mqtt_broker` | `127.0.0.1` | `127.0.0.1` | MQTT Broker 地址 |
| `mqtt_port` | `3333` | `1883` | MQTT Broker 端口 |
| `client_robot_id` | `1` | `1` | 客户端机器人身份；只接受红方 `1`–`7` 或蓝方 `101`–`107` |

构建时如果没有找到 Paho MQTT C，客户端不会启用完整 MQTT 链路。此时界面仍可启动，但 MQTT
比赛状态和工业相机图传不会工作；请先确认 CMake 输出中已找到 Paho，再排查配置。

### `video`

| 字段 | 公开示例值 | 当前用途 |
| --- | --- | --- |
| `default_path` | 空字符串 | 兼容视频源字段，字段缺失时代码回退为 `resources/videos/demo.mp4`；当前接收控件不会直接解码本地文件 |
| `stream_url` | `udp://0.0.0.0:3334` | 主图传 UDP 监听 URL；显式 URL 中的端口优先 |
| `resolution` | `1280x720` | 配置与开发观测元数据，代码回退同为 `1280x720` |
| `fps` | `60` | 公开配置兼容字段；当前视频刷新率由界面选择和控件初始值控制 |
| `industrial_camera_grid` | `false` | 英雄工业相机校准网格，可由 `RM_HERO_CAMERA_GRID` 覆盖 |

公开模板不附带视频。当前本地视频应交给模拟器转成协议图传，不要把 `default_path` 当作可用的
桌面播放器入口。使用自己的素材时，仓库内配置只写相对路径；本机临时绝对路径只能留在被忽略
的 `config.json` 中。

### `robots`

每个机器人条目包含：

- `description`：角色说明；
- `ui_layout`：布局标识；
- `modules`：模块名称数组；
- `key_bindings`：动作名到按键名的映射。

这些字段属于兼容配置和公开示例结构。当前角色选择、操作面板和全部快捷键并非都由这里动态
生成，修改后应对照实际界面验证，不能仅凭 JSON 判断已经生效。

### `ar_overlay`

| 字段 | 公开示例值 | 代码回退值 |
| --- | --- | --- |
| `enabled` | `false` | `false` |
| `model_path` | 空字符串 | 空字符串 |
| `confidence_threshold` | `0.5` | `0.5` |
| `nms_threshold` | `0.4` | `0.4` |
| `smoothing_factor` | `0.3` | `0.3` |
| `max_missed_frames` | `10` | `10` |
| `detection_interval_ms` | `33` | `33` |

AR 只有在构建时找到 ONNX Runtime、生成 `RM_HAS_AR_OVERLAY` 能力并提供有效模型后才会启用。
公开仓库不附带模型；请使用已获授权的模型和本地配置，不要提交模型绝对路径。

## 两路视频怎样配置

两路视频共用客户端中的接收与解码组件，但传输方式不同：

| 链路 | 编码与传输 | 配置入口 | 模拟器入口 |
| --- | --- | --- | --- |
| 主图传 | HEVC/H.265，经 UDP 分片发送 | `video.stream_url`，端口与 `network.video_port` 对齐 | `run_sim.sh --video-file ...` 向 UDP `3334` 发送 |
| 英雄工业相机 | H.264 Annex-B，经 MQTT `CustomByteBlock` 分片发送 | 复用 `mqtt_broker`、`mqtt_port` 和机器人身份，不设第二个 UDP 端口 | Web 控制台“工业相机”区域选择视频并开始推流 |

因此，不要为工业相机自行增加 `3335` 之类的端口。主图传看不到画面时检查 UDP `3334`；工业
相机看不到画面时检查 Paho MQTT 是否启用、Broker 是否连通、`CustomByteBlock` 是否到达，以及
客户端和模拟器是否使用同一机器人视角。

## 本地模拟器联调

下面是一套与公开示例一致的本机参数：MQTT `127.0.0.1:3333`、机器人 ID `1`、主图传 UDP
`3334`、Web 控制台 `8000`。

1. 先按本文生成 `config.json`，再构建并启动客户端。
2. 安装模拟器依赖：

   ```bash
   python3 -m venv sim/.venv
   source sim/.venv/bin/activate
   python -m pip install --upgrade pip setuptools wheel
   python -m pip install -e sim
   ```

3. 启动协议服务：

   ```bash
   ./sim/run_sim.sh \
     --no-video \
     --mqtt-host 127.0.0.1 \
     --mqtt-port 3333 \
     --current-robot-id 1
   ```

   脚本会优先复用已经监听该端口的 Broker，并尝试启动本机 Mosquitto；若回退到 Docker Broker，
   终端会打印实际映射端口。此时必须把客户端的 `RM_MQTT_PORT` 或 `network.mqtt_port` 改成同一个
   端口后重启。

4. 打开 `http://127.0.0.1:8000`，从 Web 控制台发送比赛状态，确认客户端收到数据。

需要同时模拟主图传时，先停止上面的模拟器，再用自己有权使用的视频重新启动：

```bash
./sim/run_sim.sh \
  --video-file /path/to/authorized-demo.mp4 \
  --target-ip 127.0.0.1 \
  --mqtt-host 127.0.0.1 \
  --mqtt-port 3333 \
  --current-robot-id 1
```

`--video-file` 只控制主图传 UDP 发送器。测试工业相机时，在 Web 控制台上传或选择自己的视频，
再在“工业相机”区域开始推流；该链路通过同一个 MQTT Broker 发送。

模拟器连接 Broker 失败时会保留兼容 UDP 回退，但这不能证明 MQTT 或第二路视频已经联通。联调
时应先让客户端和模拟器日志都确认同一 Broker、端口和机器人 ID，再分别加入主图传与工业相机。

## 敏感信息与提交检查

提交前至少确认：

```bash
git status --short --ignored
python3 tools/release/check_example_config.py
python3 tools/release/check_public_readiness.py
```

- 提交 `config.example.json`，不要提交 `config.json`；
- 不提交真实赛事 IP、机器人身份、账号、令牌、SSH 配置、抓包、日志和比赛记录；
- 不提交个人电脑绝对路径或没有再分发权的视频、模型和音频；
- 现场参数优先通过受控环境变量或本地配置注入，用完后清理终端环境。

公开预检审计的是 Git 发布快照，不能替代 Git 历史、第三方依赖和素材授权检查。

## 常见问题

### 修改配置后程序仍使用旧值

先从启动日志确认实际加载路径，再检查可执行文件旁边是否留有旧 `config.json`，以及当前终端是否
设置了 `RM_*` 环境变量。通信参数修改后重启客户端。

### 客户端能打开但没有比赛状态

界面启动不等于通信链路已连通。依次确认构建时已启用 MQTT、Broker 正在监听、两端端口一致、
机器人 ID 一致，再查看模拟器和客户端日志。不要一次同时修改 MQTT、UDP 和两路视频参数。

### 主图传正常，工业相机没有画面

主图传走 UDP，不能证明 MQTT 视频链路正常。确认 `CustomByteBlock` topic 已订阅、模拟器 Web 控制
台中的工业相机已选中视频并开始推流，并等待 H.264 IDR 关键帧恢复解码。

### 为什么现场配置不能通过示例检查

这是预期行为。示例检查器只允许回环地址和中性内容，用来防止现场信息进入公开提交；经授权的
现场联调应在受控工作区完成，不能通过放宽公开检查规则来获得绿色结果。
