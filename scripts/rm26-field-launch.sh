#!/usr/bin/env bash
# ==============================================================================
# RM26 客户端现场启动脚本 (Ubuntu)
#
# 用法:
#   ./scripts/rm26-field-launch.sh                # native 模式（默认，有源码变更自动编译）
#   ./scripts/rm26-field-launch.sh --build        # 强制重新编译后启动
#   ./scripts/rm26-field-launch.sh --docker       # Docker field-client 模式
#   ./scripts/rm26-field-launch.sh --stop         # 停止运行中的客户端
#
# 桌面快捷方式 / 开机自启: bash scripts/rm26-field-install.sh
# 日志: tmp/log/field_client.log
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
LOG_DIR="$PROJECT_ROOT/tmp/log"
LOG_FILE="$LOG_DIR/field_client.log"
PID_FILE="$LOG_DIR/field_client.pid"
CONFIG_FILE="$PROJECT_ROOT/config.json"
MODE="native"
FORCE_BUILD=false

. "$SCRIPT_DIR/rm26-recording.sh"

# ---- 颜色 ----
GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'

# ---- 工具函数 ----
log() {
    local msg="[$(date '+%Y-%m-%d %H:%M:%S')] $1"
    echo "$msg"
    echo "$msg" >> "$LOG_FILE"
}

is_loopback_host() {
    case "$1" in
        localhost|127.*|::1) return 0 ;;
        *) return 1 ;;
    esac
}

# ============================================================================
# 关键：Ubuntu 下从 .desktop 启动 Qt/QML 应用时，需要设置以下环境变量
# 否则输入法模块会吃掉键盘事件，导致 QML 快捷键/面板失效
# ============================================================================
setup_env() {
    # 1. 禁用 Qt 输入法模块 —— 不设这行，IBus 会拦截所有按键，QML 收不到键盘事件
    if [ -z "${QT_IM_MODULE:-}" ]; then
        # 尝试 IBus 兼容，但优先禁用以避免 QML 键盘焦点丢失
        export QT_IM_MODULE=""
    fi

    # 2. X11 MIT-SHM 扩展 —— 避免 X11 共享内存导致的渲染异常
    export QT_X11_NO_MITSHM=1

    # 3. XDG_RUNTIME_DIR —— Qt/QML 需要这个目录可写
    if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
        export XDG_RUNTIME_DIR="/tmp/runtime-$(id -u)"
        mkdir -p "$XDG_RUNTIME_DIR"
        chmod 700 "$XDG_RUNTIME_DIR"
    fi

    # 4. 强制 X11 后端（Wayland 上 Qt 可能行为不一致）
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"

    # 5. X11 显示目标
    export DISPLAY="${DISPLAY:-:0}"

    # 6. QML 渲染后端（software 最稳，需要硬件加速则改为 opengl）
    export RM_QT_QUICK_BACKEND="${RM_QT_QUICK_BACKEND:-software}"

    # 7. 避免 GTK 模块干扰 Qt 键盘处理
    export GTK_IM_MODULE="${GTK_IM_MODULE:-}"

    # 8. 避免 Qt 平台插件自动检测 IM
    export QT4_IM_MODULE=""

    # 9. 关闭 QML 磁盘缓存，避免现场加载旧缓存
    export QML_DISABLE_DISK_CACHE=1
}

# ==== 参数解析 ====
for arg in "$@"; do
    case "$arg" in
        --docker)  MODE="docker" ;;
        --native)  MODE="native" ;;
        --build)   FORCE_BUILD=true ;;
        --stop)
            log "正在停止所有客户端..."
            rm26_recording_stop "field_launch"
            rm26_recording_stop "run_docker"
            if [ -f "$PID_FILE" ]; then
                OLD_PID=$(cat "$PID_FILE" 2>/dev/null || true)
                if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
                    kill "$OLD_PID" 2>/dev/null || true
                    sleep 1
                    kill -9 "$OLD_PID" 2>/dev/null || true
                    log "已终止进程 PID=$OLD_PID"
                fi
                rm -f "$PID_FILE"
            fi
            if command -v docker >/dev/null 2>&1; then
                docker stop rm26-client-field 2>/dev/null || true
                docker rm rm26-client-field 2>/dev/null || true
                log "已停止 Docker 容器"
            fi
            exit 0
            ;;
        -h|--help)
            echo "用法: $0 [--native|--docker|--stop]"
            exit 0
            ;;
        *) log "未知参数: $arg"; exit 1 ;;
    esac
done

# ==== 准备工作 ====
mkdir -p "$LOG_DIR"

# 防止重复启动
if [ -f "$PID_FILE" ]; then
    OLD_PID=$(cat "$PID_FILE" 2>/dev/null || true)
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        log "客户端已在运行 (PID=$OLD_PID)，跳过启动"
        exit 0
    fi
    rm -f "$PID_FILE"
fi

echo $$ > "$PID_FILE"

# 清理函数
cleanup() {
    local exit_code=$?
    rm26_recording_stop "field_launch"
    log "客户端退出 (exit=$exit_code)"
    rm -f "$PID_FILE"
}
trap cleanup EXIT INT TERM

# 设置环境变量（必须在启动前）
setup_env

# 环境变量优先，桌面启动没有环境变量时再读取本地 config.json。
CONFIG_SERVER_IP=""
CONFIG_MQTT_BROKER=""
CONFIG_ROBOT_ID=""
if [ -f "$CONFIG_FILE" ] && command -v python3 >/dev/null 2>&1; then
    CONFIG_SERVER_IP=$(python3 -c "import json; d=json.load(open('$CONFIG_FILE')); print(d.get('network',{}).get('server_ip',''))" 2>/dev/null || true)
    CONFIG_MQTT_BROKER=$(python3 -c "import json; d=json.load(open('$CONFIG_FILE')); print(d.get('network',{}).get('mqtt_broker',''))" 2>/dev/null || true)
    CONFIG_ROBOT_ID=$(python3 -c "import json; d=json.load(open('$CONFIG_FILE')); print(d.get('network',{}).get('client_robot_id',''))" 2>/dev/null || true)
fi

SERVER_IP="${RM_SERVER_IP:-$CONFIG_SERVER_IP}"
MQTT_BROKER="${RM_MQTT_BROKER:-$CONFIG_MQTT_BROKER}"
ROBOT_ID="${RM_CLIENT_ROBOT_ID:-$CONFIG_ROBOT_ID}"

if [ -z "$SERVER_IP" ] || is_loopback_host "$SERVER_IP"; then
    log "错误: 现场启动前请通过环境变量或本地 config.json 设置非回环 server_ip"
    exit 1
fi
if [ -z "$MQTT_BROKER" ] || is_loopback_host "$MQTT_BROKER"; then
    log "错误: 现场启动前请通过环境变量或本地 config.json 设置非回环 mqtt_broker"
    exit 1
fi
if ! [[ "$ROBOT_ID" =~ ^[0-9]+$ ]] || [ "$ROBOT_ID" -le 0 ]; then
    log "错误: 现场启动前请设置有效的机器人 ID"
    exit 1
fi

export RM_SERVER_IP="$SERVER_IP"
export RM_MQTT_BROKER="$MQTT_BROKER"
export RM_CLIENT_ROBOT_ID="$ROBOT_ID"

log "===== RM26 客户端启动 ====="
log "模式: $MODE"
log "机器人 ID: $ROBOT_ID"
log "服务端: $SERVER_IP"
log "MQTT Broker: $MQTT_BROKER"
log "项目目录: $PROJECT_ROOT"
log "DISPLAY:  ${DISPLAY:-未设置}"
log "QT_QPA_PLATFORM: ${QT_QPA_PLATFORM:-未设置}"
log "QT_IM_MODULE: ${QT_IM_MODULE:-空(已禁用)}"
log "RM_QT_QUICK_BACKEND: ${RM_QT_QUICK_BACKEND:-未设置}"

cd "$PROJECT_ROOT"

# ==== 启动 ====
if [ "$MODE" = "docker" ]; then
    log "使用 Docker field-client 模式..."
    RUN_DOCKER_ARGS=(--field-client)
    if [ "$FORCE_BUILD" = true ]; then
        RUN_DOCKER_ARGS=(--build --field-client)
    fi
    log "启动 run_docker.sh ${RUN_DOCKER_ARGS[*]}"
    bash "$PROJECT_ROOT/run_docker.sh" "${RUN_DOCKER_ARGS[@]}" >> "$LOG_FILE" 2>&1

elif [ "$MODE" = "native" ]; then
    log "使用 Native 模式..."

    BUILD_DIR="$PROJECT_ROOT/build"

    # ==== 编译决策 ====
    NEED_BUILD=false
    APP_PATH=""
    if [ -f "$BUILD_DIR/RoboMasterClient2025" ]; then
        APP_PATH="$BUILD_DIR/RoboMasterClient2025"
    elif [ -f "$BUILD_DIR/RoboMasterClient2025.app/Contents/MacOS/RoboMasterClient2025" ]; then
        APP_PATH="$BUILD_DIR/RoboMasterClient2025.app/Contents/MacOS/RoboMasterClient2025"
    fi

    if [ "$FORCE_BUILD" = true ]; then
        NEED_BUILD=true
        log "强制重新编译 (--build)"
    elif [ -z "$APP_PATH" ] || [ ! -f "$APP_PATH" ]; then
        NEED_BUILD=true
        log "未找到二进制，自动编译..."
    elif [ -f "$CONFIG_FILE" ] && [ "$APP_PATH" -ot "$CONFIG_FILE" ]; then
        NEED_BUILD=true
        log "config.json 已更新，自动重新编译..."
    elif [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        NEED_BUILD=true
        log "未找到 CMakeCache.txt，自动配置并编译..."
    else
        # 检查是否有源码比二进制新
        NEWEST_SRC=$(find "$PROJECT_ROOT/src" \
            -name '*.cpp' -o -name '*.h' -o -name '*.qml' -o -name '*.qrc' \
            -newer "$APP_PATH" 2>/dev/null | head -1)
        if [ -n "$NEWEST_SRC" ]; then
            NEED_BUILD=true
            log "检测到源码变更: $(basename "$NEWEST_SRC")，自动重新编译..."
        fi
    fi

    # ==== 编译 ====
    if [ "$NEED_BUILD" = true ]; then
        log "开始编译..."

        if [ ! -d "$BUILD_DIR" ]; then
            mkdir -p "$BUILD_DIR"
        fi

        log "CMake 配置..."
        if ! cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" >> "$LOG_FILE" 2>&1; then
            log "错误: CMake 配置失败，查看日志: $LOG_FILE"
            tail -30 "$LOG_FILE"
            exit 1
        fi

        log "编译中 (并行 4 线程)..."
        if ! cmake --build "$BUILD_DIR" --parallel 4 >> "$LOG_FILE" 2>&1; then
            log "错误: 编译失败，查看日志: $LOG_FILE"
            tail -30 "$LOG_FILE"
            exit 1
        fi

        log "编译成功"

        # 重新定位二进制
        APP_PATH=""
        if [ -f "$BUILD_DIR/RoboMasterClient2025" ]; then
            APP_PATH="$BUILD_DIR/RoboMasterClient2025"
        elif [ -f "$BUILD_DIR/RoboMasterClient2025.app/Contents/MacOS/RoboMasterClient2025" ]; then
            APP_PATH="$BUILD_DIR/RoboMasterClient2025.app/Contents/MacOS/RoboMasterClient2025"
        fi
    fi

    if [ -z "$APP_PATH" ] || [ ! -f "$APP_PATH" ]; then
        log "错误: 编译后仍未找到可执行文件"
        exit 1
    fi

    chmod +x "$APP_PATH"
    log "启动可执行文件: $APP_PATH"
    rm26_recording_start "field_launch"
    "$APP_PATH" >> "$LOG_FILE" 2>&1
fi
