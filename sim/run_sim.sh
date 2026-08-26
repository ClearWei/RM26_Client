#!/bin/bash

# ==============================================================================
# RM26 仿真服务器启动脚本
#
# 功能：
# 1. 启动 Python 协议仿真服务器 (FastAPI)
# 2. 启动视频图传模拟器 (HEVC over UDP)
#
# 默认只启动协议服务；视频图传需要通过 --video-only 或 --video-file 显式启用。
# 使用方法: ./run_sim.sh [选项]
#   --no-video           不启动视频图传模拟
#   --video-only         只启动视频图传模拟
#   --video-file <path>  指定视频文件并启用图传
# ==============================================================================

set -e

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$SCRIPT_DIR"

# 定义颜色
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

PYTHON_CMD=""
UNAME_S="$(uname -s)"
IS_WINDOWS=false
LOCAL_VENV_PYTHON=""

# Homebrew 的 mosquitto 位于 sbin。独立运行本脚本时也要覆盖 GUI/IDE
# 常见的精简 PATH，否则会错误地进入 Docker/UDP 回退链路。
if [ "$UNAME_S" = "Darwin" ]; then
    for homebrew_sbin in /opt/homebrew/sbin /usr/local/sbin; do
        if [ -d "$homebrew_sbin" ] && [[ ":$PATH:" != *":$homebrew_sbin:"* ]]; then
            PATH="$homebrew_sbin:$PATH"
        fi
    done
    export PATH
fi

if [[ "$UNAME_S" == *"MINGW"* ]] || [[ "$UNAME_S" == *"MSYS"* ]] || [[ "$UNAME_S" == *"CYGWIN"* ]]; then
    IS_WINDOWS=true
fi

if [ "$IS_WINDOWS" = true ]; then
    if [ -x "$SCRIPT_DIR/.venv/Scripts/python.exe" ]; then
        LOCAL_VENV_PYTHON="$SCRIPT_DIR/.venv/Scripts/python.exe"
    fi
else
    if [ -x "$SCRIPT_DIR/.venv/bin/python" ]; then
        LOCAL_VENV_PYTHON="$SCRIPT_DIR/.venv/bin/python"
    fi
fi

is_windowsapps_python() {
    local path="$1"
    [[ -n "$path" && "$path" == *"WindowsApps"* ]]
}

if [ -n "$LOCAL_VENV_PYTHON" ]; then
    PYTHON_CMD="$LOCAL_VENV_PYTHON"
elif [[ -n "$VIRTUAL_ENV" ]] && command -v python >/dev/null 2>&1; then
    PYTHON_CMD="python"
elif [ "$IS_WINDOWS" = true ]; then
    if command -v python >/dev/null 2>&1; then
        PYTHON_PATH="$(command -v python)"
        if ! is_windowsapps_python "$PYTHON_PATH"; then
            PYTHON_CMD="python"
        fi
    fi

    if [ -z "$PYTHON_CMD" ] && command -v python3 >/dev/null 2>&1; then
        PYTHON3_PATH="$(command -v python3)"
        if ! is_windowsapps_python "$PYTHON3_PATH"; then
            PYTHON_CMD="python3"
        fi
    fi
else
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_CMD="python3"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_CMD="python"
    fi
fi

if [ -z "$PYTHON_CMD" ]; then
    log_error "未找到可用的 Python（python3 或 python）"
    exit 1
fi

check_python_dependencies() {
    local pyproject_file="$SCRIPT_DIR/pyproject.toml"

    if "$PYTHON_CMD" -c "import fastapi, socketio, uvicorn, multipart, cv2, numpy, paho.mqtt.client, google.protobuf" >/dev/null 2>&1; then
        return 0
    fi

    log_error "Python 依赖不完整，无法启动仿真服务"
    if [ -f "$pyproject_file" ]; then
        log_info "请先运行: cd \"$SCRIPT_DIR\" && $PYTHON_CMD -m pip install -e ."
        if [ "$UNAME_S" = "Linux" ]; then
            local python_version
            python_version="$("$PYTHON_CMD" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>/dev/null || true)"

            log_info "Ubuntu/Debian 首次安装请依次运行："
            log_info "  sudo apt update"
            if [ -n "$python_version" ]; then
                log_info "  sudo apt install -y python${python_version}-venv python3-pip python3-dev build-essential"
            else
                log_info "  sudo apt install -y python3-venv python3-pip python3-dev build-essential"
            fi
            log_info "  cd \"$SCRIPT_DIR\""
            log_info "  /usr/bin/python3 -m venv --clear .venv"
            log_info "  source .venv/bin/activate"
            log_info "  python -m pip install --upgrade pip setuptools wheel"
            log_info "  python -m pip install -e ."
            log_info "以后重新打开终端，只需运行: cd \"$SCRIPT_DIR\" && source .venv/bin/activate"
        else
            log_info "兼容旧方式: $PYTHON_CMD -m pip install -r \"$SCRIPT_DIR/requirements.txt\""
        fi
    else
        log_info "请先安装 sim 的 Python 依赖"
    fi
    exit 1
}

# 解析参数
START_VIDEO=false
START_SERVER=true
VIDEO_FILE="$PROJECT_ROOT/resources/videos/testdemo.mov"
VIDEO_SENDER="$PROJECT_ROOT/tools/video_sender.py"
# 默认只向本机发送，双机或现场联调必须显式指定客户端地址。
TARGET_IP="${RM_SIM_TARGET_IP:-127.0.0.1}"
MQTT_HOST="${RM_SIM_MQTT_HOST:-127.0.0.1}"
MQTT_PORT="${RM_SIM_MQTT_PORT:-3333}"
WEB_PORT="${RM_SIM_WEB_PORT:-8000}"
SERVER_ARGS=()
PIDS=()
MQTT_DOCKER_STARTED=false
COMPOSE_CMD=()
CLEANUP_DONE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-video)
            START_VIDEO=false
            shift
            ;;
        --video-only)
            START_SERVER=false
            START_VIDEO=true
            shift
            ;;
        --video-file)
            VIDEO_FILE="$2"
            START_VIDEO=true
            shift 2
            ;;
        --target-ip)
            TARGET_IP="$2"
            shift 2
            ;;
        --mqtt-host)
            MQTT_HOST="$2"
            shift 2
            ;;
        --mqtt-port)
            MQTT_PORT="$2"
            shift 2
            ;;
        --web-port)
            WEB_PORT="$2"
            shift 2
            ;;
        --current-robot-id)
            SERVER_ARGS+=("$1" "$2")
            shift 2
            ;;
        --enable-receiver|--enable-udp-sender)
            SERVER_ARGS+=("$1")
            shift
            ;;
        *)
            log_error "未知参数: $1"
            exit 1
            ;;
    esac
done

port_is_listening() {
    local port="$1"

    if command -v lsof >/dev/null 2>&1; then
        lsof -i "tcp:${port}" -sTCP:LISTEN -n -P >/dev/null 2>&1
        return $?
    fi

    if command -v ss >/dev/null 2>&1; then
        ss -ltn "( sport = :${port} )" 2>/dev/null | grep -q ":${port}"
        return $?
    fi

    if command -v netstat >/dev/null 2>&1; then
        netstat -ano 2>/dev/null | awk -v port=":${port}" '$0 ~ port && $0 ~ /LISTEN|LISTENING/ { found=1 } END { exit found ? 0 : 1 }'
        return $?
    fi

    return 1
}

is_valid_port() {
    local port="$1"
    [[ "$port" =~ ^[0-9]+$ ]] && [ "$port" -ge 1 ] && [ "$port" -le 65535 ]
}

wait_for_port() {
    local port="$1"
    local retries="${2:-20}"
    local delay="${3:-0.5}"
    local i

    for ((i = 0; i < retries; ++i)); do
        if port_is_listening "$port"; then
            return 0
        fi
        sleep "$delay"
    done

    return 1
}

is_local_mqtt_host() {
    case "$1" in
        127.0.0.1|localhost|::1)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

if docker compose version >/dev/null 2>&1; then
    COMPOSE_CMD=(docker compose -f "$PROJECT_ROOT/docker-compose.yml")
elif command -v docker-compose >/dev/null 2>&1; then
    COMPOSE_CMD=(docker-compose -f "$PROJECT_ROOT/docker-compose.yml")
fi

ensure_mqtt_broker() {
    local requested_host="$1"
    local requested_port="$2"

    if ! is_local_mqtt_host "$requested_host"; then
        log_info "MQTT Broker 配置为远程地址 ${requested_host}:${requested_port}"
        return 0
    fi

    if port_is_listening "$requested_port"; then
        log_info "检测到本地 MQTT Broker 已在 ${requested_host}:${requested_port} 监听。"
        return 0
    fi

    if command -v mosquitto >/dev/null 2>&1; then
        log_info "未检测到本地 MQTT Broker，尝试使用 mosquitto 启动 ${requested_host}:${requested_port}..."
        mosquitto -c "$PROJECT_ROOT/docker/mosquitto-local.conf" > "$PROJECT_ROOT/mqtt_broker.log" 2>&1 &
        local mqtt_pid=$!
        PIDS+=("$mqtt_pid")
        if wait_for_port "$requested_port"; then
            log_info "本地 mosquitto 已启动 (PID: $mqtt_pid)"
            return 0
        fi
        log_warn "mosquitto 已尝试启动，但 ${requested_port} 仍未监听。"
    fi

    if [ "${#COMPOSE_CMD[@]}" -gt 0 ]; then
        local host_port="${RM_MQTT_HOST_PORT:-11883}"
        log_info "未检测到本地 MQTT Broker，尝试通过 Docker 启动 rm26-mqtt (host port ${host_port})..."
        if "${COMPOSE_CMD[@]}" up -d rm26-mqtt >/dev/null 2>&1; then
            MQTT_DOCKER_STARTED=true
            if wait_for_port "$host_port"; then
                MQTT_HOST="127.0.0.1"
                MQTT_PORT="$host_port"
                log_info "Docker MQTT Broker 已启动于 ${MQTT_HOST}:${MQTT_PORT}"
                return 0
            fi
            log_warn "Docker MQTT Broker 已尝试启动，但 host port ${host_port} 仍未监听。"
        else
            log_warn "Docker MQTT Broker 启动失败。"
        fi
    fi

    return 1
}

# 端口属于调用者环境，启动脚本不能代替用户终止占用者。
if [ "$START_SERVER" = true ]; then
    if ! is_valid_port "$WEB_PORT"; then
        log_error "Web 端口无效: $WEB_PORT（应为 1-65535）"
        exit 1
    fi
    if port_is_listening "$WEB_PORT"; then
        log_error "Web 端口 $WEB_PORT 已被其他进程占用，请先停止占用者或使用 --web-port 更换端口。"
        exit 1
    fi
fi

if [ "$START_SERVER" = true ] || [ "$START_VIDEO" = true ]; then
    check_python_dependencies
fi

# 清理函数 - 确保所有子进程都被终止
cleanup() {
    if [ "$CLEANUP_DONE" = true ]; then
        return 0
    fi
    CLEANUP_DONE=true

    log_info "正在停止所有服务..."

    # 只按反向顺序回收本脚本实际启动并记录的子进程。
    local index
    local pid
    for ((index = ${#PIDS[@]} - 1; index >= 0; --index)); do
        pid="${PIDS[$index]}"
        if kill -0 "$pid" 2>/dev/null; then
            log_info "终止进程 PID: $pid"
            kill -TERM "$pid" 2>/dev/null || true
            # 等待进程结束
            sleep 0.5
            # 如果还活着，强制杀死
            if kill -0 "$pid" 2>/dev/null; then
                kill -9 "$pid" 2>/dev/null || true
            fi
        fi
    done

    if [ "$MQTT_DOCKER_STARTED" = true ] && [ "${#COMPOSE_CMD[@]}" -gt 0 ]; then
        "${COMPOSE_CMD[@]}" stop rm26-mqtt >/dev/null 2>&1 || true
    fi

    log_info "服务已停止"
}
trap cleanup EXIT INT TERM HUP

if [ "$START_SERVER" = true ]; then
    if ! ensure_mqtt_broker "$MQTT_HOST" "$MQTT_PORT"; then
        log_warn "MQTT Broker 不可用，模拟器将按现有逻辑自动回退到 UDP sender。"
    fi
fi

# ==============================================================================
# 1. 启动协议仿真服务器
# ==============================================================================
if [ "$START_SERVER" = true ]; then
    log_step "正在启动协议仿真服务器..."

    # Windows/MinGW 下转换 PYTHONPATH
    PYTHON_PATH_DIR="$SCRIPT_DIR"
    if [[ "$(uname)" == *"MINGW"* ]] || [[ "$(uname)" == *"MSYS"* ]]; then
        if command -v cygpath &> /dev/null; then
            PYTHON_PATH_DIR=$(cygpath -m "$SCRIPT_DIR")
        else
            PYTHON_PATH_DIR=$(cd "$SCRIPT_DIR" && pwd -W 2>/dev/null || echo "$SCRIPT_DIR")
        fi
    fi
    export PYTHONPATH="$PYTHON_PATH_DIR:$PYTHONPATH"

    if [ -f "server/main.py" ]; then
        # 以模块方式运行，确保包导入路径正确
        $PYTHON_CMD -m server.main --target-ip "$TARGET_IP" --mqtt-host "$MQTT_HOST" --mqtt-port "$MQTT_PORT" --web-port "$WEB_PORT" "${SERVER_ARGS[@]}" &
        SERVER_PID=$!
        PIDS+=("$SERVER_PID")
        log_info "协议仿真服务器已启动 (PID: $SERVER_PID, target: $TARGET_IP, mqtt: ${MQTT_HOST}:${MQTT_PORT})"
        sleep 1
    else
        log_warn "未找到 server/main.py，跳过协议仿真服务器"
    fi
fi

# ==============================================================================
# 2. 启动视频图传模拟器
# ==============================================================================
if [ "$START_VIDEO" = true ]; then
    log_step "正在启动视频图传模拟器..."

    # Windows 环境路径转换
    if [[ "$(uname)" == *"MINGW"* ]] || [[ "$(uname)" == *"MSYS"* ]] || [[ "$(uname)" == *"CYGWIN"* ]]; then
        if command -v cygpath &> /dev/null; then
            # 使用 cygpath -m 转换为统一格式路径，避免反斜杠转义问题
            VIDEO_SENDER=$(cygpath -m "$VIDEO_SENDER")
            VIDEO_FILE=$(cygpath -m "$VIDEO_FILE")
        fi
    fi

    # 检查 ffmpeg 是否存在
    if ! command -v ffmpeg &> /dev/null; then
         log_error "未找到 ffmpeg 命令！视频发送器需要 ffmpeg。"
         log_info "Windows 用户请下载 ffmpeg 并添加到 PATH 环境变量，或通过 vcpkg 安装。"
         cleanup
         exit 1
    fi

    if [ ! -f "$VIDEO_SENDER" ] && [ ! -f "$(echo "$VIDEO_SENDER" | sed 's/\\/\//g')" ]; then
        log_error "未找到视频发送器: $VIDEO_SENDER"
        log_info "请确保 tools/video_sender.py 存在"
    elif [ ! -f "$VIDEO_FILE" ] && [ ! -f "$(echo "$VIDEO_FILE" | sed 's/\\/\//g')" ]; then
        log_warn "未找到视频文件: $VIDEO_FILE"
        log_info "请使用 --video-file 指定视频文件"
    else
        # 启动视频发送 (重定向输出到日志文件)
        VIDEO_LOG="/tmp/video_sender_$$.log"
        # 确保 tmp 目录存在
        mkdir -p /tmp

        log_info "正在启动视频发送器，日志文件: $VIDEO_LOG"
        echo "Starting Video Sender..." > "$VIDEO_LOG"
        echo "Command: $PYTHON_CMD -u \"$VIDEO_SENDER\" \"$VIDEO_FILE\" \"$TARGET_IP\" 3334" >> "$VIDEO_LOG"

        # 使用 -u 参数禁用 Python 输出缓冲
        $PYTHON_CMD -u "$VIDEO_SENDER" "$VIDEO_FILE" "$TARGET_IP" 3334 >> "$VIDEO_LOG" 2>&1 &
        VIDEO_PID=$!
        PIDS+=("$VIDEO_PID")
        log_info "视频图传模拟器已启动 (PID: $VIDEO_PID, target: $TARGET_IP)"

        # 检查进程是否立即退出
        sleep 1
        if ! kill -0 "$VIDEO_PID" 2>/dev/null; then
            log_error "视频图传模拟器启动失败！"
            log_error "查看日志内容:"
            log_error "---------------------------------------------------"
            cat "$VIDEO_LOG"
            log_error "---------------------------------------------------"
            cleanup
            exit 1
        fi
    fi
fi

# ==============================================================================
# 等待
# ==============================================================================
if [ ${#PIDS[@]} -eq 0 ]; then
    log_error "没有服务启动"
    exit 1
fi

log_info "所有服务已启动，按 Ctrl+C 停止"
log_info "-------------------------------------------"

# 等待所有子进程
# 注意：在 MinGW/Git Bash 中，wait 可能会立即返回，因此改为循环检查
while true; do
    sleep 1

    for pid in "${PIDS[@]}"; do
        if ! kill -0 "$pid" 2>/dev/null; then
            log_error "检测到进程 PID $pid 已退出，脚本即将停止..."

            # 显示视频发送器的日志（如果存在且是该进程退出）
            if [ -n "$VIDEO_LOG" ] && [ -f "$VIDEO_LOG" ]; then
                 log_info "正在读取视频模拟器日志 ($VIDEO_LOG):"
                 echo "---------------------------------------------------"
                 cat "$VIDEO_LOG"
                 echo "---------------------------------------------------"
            fi

            break 2
        fi
    done
done

log_info "服务已退出"
