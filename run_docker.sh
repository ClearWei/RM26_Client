#!/bin/bash

set -euo pipefail

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

is_loopback_host() {
    case "$1" in
        localhost|127.*|::1) return 0 ;;
        *) return 1 ;;
    esac
}

validate_field_runtime_config() {
    if [ -z "${RM_SERVER_IP:-}" ] || is_loopback_host "$RM_SERVER_IP"; then
        log_error "现场模式需要显式设置非回环 RM_SERVER_IP"
        return 1
    fi
    if [ -z "${RM_MQTT_BROKER:-}" ] || is_loopback_host "$RM_MQTT_BROKER"; then
        log_error "现场模式需要显式设置非回环 RM_MQTT_BROKER"
        return 1
    fi
    if ! [[ "${RM_CLIENT_ROBOT_ID:-}" =~ ^[0-9]+$ ]] || [ "$RM_CLIENT_ROBOT_ID" -le 0 ]; then
        log_error "现场模式需要显式设置有效的 RM_CLIENT_ROBOT_ID"
        return 1
    fi
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"
ENV_EXAMPLE="$SCRIPT_DIR/.env.docker.example"
CONFIG_FILE="$SCRIPT_DIR/config.json"
SERVICES=("rm26-mqtt" "rm26-client")
NEED_REVOKE_XHOST=false
STACK_MODE="full"
BUILD_IMAGES=false
COMPOSE_FILE_ARGS=()
COMPOSE_PROJECT_NAME="rm26_qtclient"
RM26_CLIENT_BASE_IMAGE="${RM26_CLIENT_BASE_IMAGE:-ubuntu:24.04}"
RM_DOCKER_PULL_RETRIES="${RM_DOCKER_PULL_RETRIES:-3}"
RM_DOCKER_PULL_RETRY_DELAY_SEC="${RM_DOCKER_PULL_RETRY_DELAY_SEC:-5}"

. "$SCRIPT_DIR/scripts/docker-audio-proxy.sh"
. "$SCRIPT_DIR/scripts/rm26-recording.sh"

# ==============================================================================
# 从根 config.json 读取默认配置
# ==============================================================================
CONFIG_SERVER_IP=""
CONFIG_SERVER_PORT=""
CONFIG_CLIENT_PORT=""
CONFIG_MQTT_BROKER=""
CONFIG_MQTT_PORT=""
CONFIG_CLIENT_ROBOT_ID=""
CONFIG_VIDEO_PORT=""

if [ -f "$CONFIG_FILE" ] && command -v python3 >/dev/null 2>&1; then
    CONFIG_SERVER_IP=$(python3 -c "import json; data=json.load(open('$CONFIG_FILE')); print(data.get('network', {}).get('server_ip', ''))" 2>/dev/null || echo "")
    CONFIG_SERVER_PORT=$(python3 -c "import json; data=json.load(open('$CONFIG_FILE')); print(data.get('network', {}).get('server_port', ''))" 2>/dev/null || echo "")
    CONFIG_CLIENT_PORT=$(python3 -c "import json; data=json.load(open('$CONFIG_FILE')); print(data.get('network', {}).get('client_port', ''))" 2>/dev/null || echo "")
    CONFIG_MQTT_BROKER=$(python3 -c "import json; data=json.load(open('$CONFIG_FILE')); print(data.get('network', {}).get('mqtt_broker', ''))" 2>/dev/null || echo "")
    CONFIG_MQTT_PORT=$(python3 -c "import json; data=json.load(open('$CONFIG_FILE')); print(data.get('network', {}).get('mqtt_port', ''))" 2>/dev/null || echo "")
    CONFIG_CLIENT_ROBOT_ID=$(python3 -c "import json; data=json.load(open('$CONFIG_FILE')); print(data.get('network', {}).get('client_robot_id', ''))" 2>/dev/null || echo "")
    CONFIG_VIDEO_PORT=$(python3 -c "import json; data=json.load(open('$CONFIG_FILE')); print(data.get('network', {}).get('video_port', ''))" 2>/dev/null || echo "")
    log_info "已从 config.json 读取默认配置"
else
    log_warn "未找到 config.json 或 python3，使用内置默认值"
fi

cleanup() {
    rm26_recording_stop "run_docker"
    if [ "$NEED_REVOKE_XHOST" = true ] && command -v xhost >/dev/null 2>&1; then
        xhost -local:docker >/dev/null 2>&1 || true
        log_info "已恢复 X11 权限: xhost -local:docker"
    fi
}
trap cleanup EXIT INT TERM

find_compose_cmd() {
    if docker compose version >/dev/null 2>&1; then
        echo "docker compose"
        return 0
    fi

    if command -v docker-compose >/dev/null 2>&1; then
        echo "docker-compose"
        return 0
    fi

    return 1
}

validate_retry_setting() {
    local value="$1"
    local fallback="$2"

    if [[ "$value" =~ ^[0-9]+$ ]] && [ "$value" -ge 0 ]; then
        echo "$value"
        return 0
    fi

    echo "$fallback"
}

print_base_image_pull_help() {
    local image="$1"

    cat <<EOF
${YELLOW}[WARN]${NC} 无法拉取 client 基础镜像: $image
${YELLOW}[WARN]${NC} 当前报错如果停在 "load metadata for docker.io/library/ubuntu:24.04"，通常不是项目代码问题，而是 Docker 到镜像仓库的网络/代理/IPv6 连通性问题。
${YELLOW}[WARN]${NC} 可选处理方式:
${YELLOW}[WARN]${NC}   1. 先手动验证: docker pull $image
${YELLOW}[WARN]${NC}   2. 配置 Docker 镜像加速或代理后重试
${YELLOW}[WARN]${NC}   3. 若已有可用基础镜像，设置 RM26_CLIENT_BASE_IMAGE 为本地 tag 或内网镜像
${YELLOW}[WARN]${NC}      例如: export RM26_CLIENT_BASE_IMAGE=rm26-ubuntu-base:24.04
EOF
}

ensure_client_base_image() {
    local image="$RM26_CLIENT_BASE_IMAGE"
    local retries
    local delay
    local attempt

    retries="$(validate_retry_setting "$RM_DOCKER_PULL_RETRIES" 3)"
    delay="$(validate_retry_setting "$RM_DOCKER_PULL_RETRY_DELAY_SEC" 5)"

    if docker image inspect "$image" >/dev/null 2>&1; then
        log_info "已找到 client 基础镜像缓存: $image"
        return 0
    fi

    log_step "预检查 client 基础镜像: $image"

    for attempt in $(seq 1 $((retries + 1))); do
        if docker pull "$image"; then
            log_info "client 基础镜像已就绪: $image"
            return 0
        fi

        if [ "$attempt" -le "$retries" ]; then
            log_warn "拉取基础镜像失败，$delay 秒后重试 ($attempt/$retries)"
            sleep "$delay"
        fi
    done

    print_base_image_pull_help "$image"
    exit 1
}

ensure_env_file() {
    if [ -f "$ENV_FILE" ]; then
        return 0
    fi

    if [ ! -f "$ENV_EXAMPLE" ]; then
        log_error "未找到环境变量模板: $ENV_EXAMPLE"
        exit 1
    fi

    cp "$ENV_EXAMPLE" "$ENV_FILE"
    log_warn "未检测到 .env，已从 .env.docker.example 自动创建"
}

prepare_x11() {
    if [ "$(uname -s)" != "Linux" ]; then
        return 0
    fi

    if [ -z "${DISPLAY:-}" ]; then
        log_warn "未检测到 DISPLAY，GUI 容器可能无法显示"
        return 0
    fi

    if ! command -v xhost >/dev/null 2>&1; then
        log_warn "未找到 xhost，无法自动配置 X11 权限"
        return 0
    fi

    xhost +local:docker >/dev/null 2>&1 || true
    NEED_REVOKE_XHOST=true
    log_info "已授予 Docker 访问本机 X11: xhost +local:docker"
}

print_usage() {
    cat <<'EOF'
用法: ./run_docker.sh [选项]

选项:
  --build              增量重建 Docker 镜像（源码变更后使用，复用 CMake/Ninja 缓存）
  --reuse, --no-build  直接启动现有容器/镜像，不重新构建 client（默认行为）
  --field-client       现场客户端模式：只启动客户端，使用 Linux host network 接收赛事引擎 MQTT/UDP 图传
  -h, --help           显示帮助
EOF
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build)
                BUILD_IMAGES=true
                shift
                ;;
            --reuse|--no-build)
                BUILD_IMAGES=false
                shift
                ;;
            --field-client)
                STACK_MODE="field-client"
                SERVICES=("rm26-client")
                shift
                ;;
            -h|--help)
                print_usage
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                print_usage
                exit 1
                ;;
        esac
    done
}

get_service_image() {
    local service="$1"
    local config_json

    config_json="$($COMPOSE_CMD "${COMPOSE_FILE_ARGS[@]}" config --format json 2>/dev/null)" || return 1
    python3 -c \
        'import json, sys; print(json.load(sys.stdin).get("services", {}).get(sys.argv[1], {}).get("image", ""))' \
        "$service" <<<"$config_json"
}

compose_has_service() {
    local service="$1"
    $COMPOSE_CMD "${COMPOSE_FILE_ARGS[@]}" config --services 2>/dev/null | grep -qx "$service"
}

ensure_reusable_images() {
    local client_image

    client_image="$(get_service_image "rm26-client")"

    if [ -z "$client_image" ] || ! docker image inspect "$client_image" >/dev/null 2>&1; then
        log_warn "未找到 client 镜像: ${client_image:-<none>}，自动切换到构建模式"
        BUILD_IMAGES=true
        return 0
    fi

    if [ "$STACK_MODE" = "full" ] && compose_has_service "rm26-sim"; then
        local sim_image
        sim_image="$(get_service_image "rm26-sim")"
        if [ -z "$sim_image" ] || ! docker image inspect "$sim_image" >/dev/null 2>&1; then
            log_warn "未找到 sim 镜像: ${sim_image:-<none>}，自动切换到构建模式"
            BUILD_IMAGES=true
            return 0
        fi
    fi

    if ! docker run --rm --entrypoint /bin/sh "$client_image" -lc \
        "test -x /opt/rmclient/build/RoboMasterClient2025" \
        >/dev/null 2>&1; then
        log_warn "当前 client 镜像缺少已编译程序，自动切换到构建模式"
        BUILD_IMAGES=true
        return 0
    fi
}

main() {
    parse_args "$@"

    log_step "检查 Docker 环境..."

    if ! command -v docker >/dev/null 2>&1; then
        log_error "未找到 docker，请先安装 Docker"
        exit 1
    fi

    if ! docker info >/dev/null 2>&1; then
        log_error "Docker daemon 未启动，请先启动 Docker"
        exit 1
    fi

    COMPOSE_CMD="$(find_compose_cmd)" || {
        log_error "未找到 docker compose / docker-compose"
        exit 1
    }

    ensure_env_file
    rm26_ensure_docker_audio
    prepare_x11

    if [ "$STACK_MODE" = "field-client" ]; then
        if [ "$(uname -s)" != "Linux" ]; then
            log_error "--field-client 依赖 Docker host network，只支持 Linux 现场机"
            exit 1
        fi
        COMPOSE_PROJECT_NAME="rm26_qtclient_field"
        COMPOSE_FILE_ARGS=(--project-name "$COMPOSE_PROJECT_NAME" -f "$SCRIPT_DIR/docker-compose.field.yml")
        # 优先级：环境变量 > config.json > 本机安全默认值
        export RM_SERVER_IP="${RM_SERVER_IP:-${CONFIG_SERVER_IP:-127.0.0.1}}"
        export RM_SERVER_PORT="${RM_SERVER_PORT:-${CONFIG_SERVER_PORT:-3333}}"
        export RM_MQTT_BROKER="${RM_MQTT_BROKER:-${CONFIG_MQTT_BROKER:-127.0.0.1}}"
        export RM_MQTT_PORT="${RM_MQTT_PORT:-${CONFIG_MQTT_PORT:-3333}}"
        export RM_CLIENT_ROBOT_ID="${RM_CLIENT_ROBOT_ID:-${CONFIG_CLIENT_ROBOT_ID:-1}}"
        export RM_ENEMY_TEAM_NAME="${RM_ENEMY_TEAM_NAME:-示例对手}"
        export RM_CLIENT_PORT="${RM_CLIENT_PORT:-${CONFIG_CLIENT_PORT:-3333}}"
        export RM_VIDEO_PORT="${RM_VIDEO_PORT:-${CONFIG_VIDEO_PORT:-3334}}"
        export RM_VIDEO_UDP_RCVBUF_BYTES="${RM_VIDEO_UDP_RCVBUF_BYTES:-16777216}"
        export RM_VIDEO_RESET_ON_LOSS_THRESHOLD="${RM_VIDEO_RESET_ON_LOSS_THRESHOLD:-1}"
        export RM_RUNTIME_LOG_PATH="${RM_RUNTIME_LOG_PATH:-/opt/rmclient/tmp/log/runtime.log}"
        validate_field_runtime_config
    fi

    if [ "$STACK_MODE" = "full" ]; then
        COMPOSE_PROJECT_NAME="rm26_qtclient"
        COMPOSE_FILE_ARGS=(--project-name "$COMPOSE_PROJECT_NAME" -f "$SCRIPT_DIR/docker-compose.yml")
        # 优先级：环境变量 > config.json > 本机安全默认值
        export RM_SERVER_IP="${RM_SERVER_IP:-${CONFIG_SERVER_IP:-127.0.0.1}}"
        export RM_SERVER_PORT="${RM_SERVER_PORT:-${CONFIG_SERVER_PORT:-3333}}"
        export RM_CLIENT_PORT="${RM_CLIENT_PORT:-${CONFIG_CLIENT_PORT:-3333}}"
        export RM_CLIENT_HOST_PORT="${RM_CLIENT_HOST_PORT:-3334}"
        export RM_VIDEO_HOST_PORT="${RM_VIDEO_HOST_PORT:-3334}"
        export RM_MQTT_HOST_PORT="${RM_MQTT_HOST_PORT:-3333}"
        export RM_MQTT_BROKER="${RM_MQTT_BROKER:-${CONFIG_MQTT_BROKER:-127.0.0.1}}"
        export RM_MQTT_PORT="${RM_MQTT_PORT:-${CONFIG_MQTT_PORT:-$RM_MQTT_HOST_PORT}}"
        export RM_CLIENT_ROBOT_ID="${RM_CLIENT_ROBOT_ID:-${CONFIG_CLIENT_ROBOT_ID:-1}}"
        export RM_SIM_TARGET_IP="${RM_SIM_TARGET_IP:-rm26-client}"
        export RM_SIM_MQTT_HOST="${RM_SIM_MQTT_HOST:-rm26-mqtt}"
        export RM_SIM_MQTT_PORT="${RM_SIM_MQTT_PORT:-1883}"
        export RM_SIM_WEB_PORT="${RM_SIM_WEB_PORT:-8000}"
        export RM_VIDEO_UDP_RCVBUF_BYTES="${RM_VIDEO_UDP_RCVBUF_BYTES:-16777216}"
        export RM_VIDEO_RESET_ON_LOSS_THRESHOLD="${RM_VIDEO_RESET_ON_LOSS_THRESHOLD:-1}"
        export RM_ENEMY_TEAM_NAME="${RM_ENEMY_TEAM_NAME:-示例对手}"
        export RM_RUNTIME_LOG_PATH="${RM_RUNTIME_LOG_PATH:-/opt/rmclient/tmp/log/runtime.log}"
    fi
    mkdir -p "$SCRIPT_DIR/tmp/log"
    mkdir -p "$SCRIPT_DIR/recordings"

    if [ "$BUILD_IMAGES" = true ]; then
        export DOCKER_BUILDKIT="${DOCKER_BUILDKIT:-1}"
        export COMPOSE_DOCKER_CLI_BUILD="${COMPOSE_DOCKER_CLI_BUILD:-1}"
        export RM_SIM_INSTALL_DEPS="${RM_SIM_INSTALL_DEPS:-1}"
        ensure_client_base_image
    else
        export RM_SIM_INSTALL_DEPS=0
        ensure_reusable_images
    fi

    log_step "启动 Docker 联调环境..."
    log_info "使用环境文件: $ENV_FILE"
    log_info "赛事网络配置: $CONFIG_FILE（修改后仅需重启，无需 --build）"
    log_info "Compose project: $COMPOSE_PROJECT_NAME"
    if [ "$STACK_MODE" = "field-client" ]; then
        log_info "Compose 文件: docker-compose.field.yml"
        log_info "网络模式: host network (绕过 docker bridge/veth，降低 UDP 图传丢包和延迟)"
    fi
    log_info "启动服务: ${SERVICES[*]}"
    if [ "$BUILD_IMAGES" = true ]; then
        log_info "增量构建模式: 启动前重建镜像，复用 BuildKit CMake/Ninja 与 ccache 缓存"
    else
        log_info "复用模式: 跳过镜像构建，直接使用已有镜像启动"
    fi
    log_info "客户端 -> 服务端 UDP: ${RM_SERVER_IP:-rm26-sim}:${RM_SERVER_PORT:-3333}"
    log_info "客户端 -> MQTT: ${RM_MQTT_BROKER:-rm26-mqtt}:${RM_MQTT_PORT:-3333}"
    log_info "客户端 MQTT ClientID: ${RM_CLIENT_ROBOT_ID:-<config.json>}"
    if [ "$STACK_MODE" = "full" ] && compose_has_service "rm26-sim"; then
        log_info "Web 控制台: http://localhost:${RM_SIM_WEB_PORT:-8000}"
    fi
    log_info "录屏输出目录: ${RM_RECORD_OUTPUT_DIR:-$SCRIPT_DIR/recordings}"

    cd "$SCRIPT_DIR"
    COMPOSE_ARGS=(up)
    if [ "$BUILD_IMAGES" = true ]; then
        COMPOSE_ARGS+=(--build)
    fi

    rm26_recording_start "run_docker"

    $COMPOSE_CMD "${COMPOSE_FILE_ARGS[@]}" "${COMPOSE_ARGS[@]}" "${SERVICES[@]}"
}

main "$@"
