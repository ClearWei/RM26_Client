#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

INSTALL_DESKTOP=false
INSTALL_AUTOSTART=false
SKIP_DOCKER=false

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
step() { echo -e "${BLUE}[STEP]${NC} $1"; }
warn() { echo -e "${RED}[WARN]${NC} $1"; }

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

apt_package_available() {
    local package_name="$1"
    apt-cache show "$package_name" >/dev/null 2>&1
}

docker_cli_available() {
    command_exists docker && docker --version >/dev/null 2>&1
}

docker_compose_available() {
    if command_exists docker && docker compose version >/dev/null 2>&1; then
        return 0
    fi

    command_exists docker-compose
}

install_docker_packages() {
    local -a docker_packages=()

    if docker_cli_available && docker_compose_available; then
        info "检测到宿主机已安装 Docker / Compose，跳过 Docker 软件包安装"
        return 0
    fi

    step "安装 Docker / Compose"

    if apt_package_available docker-ce && apt_package_available docker-ce-cli; then
        info "检测到官方 Docker APT 仓库，安装 docker-ce 套件"
        docker_packages=(docker-ce docker-ce-cli)

        if apt_package_available containerd.io; then
            docker_packages+=(containerd.io)
        fi
        if apt_package_available docker-buildx-plugin; then
            docker_packages+=(docker-buildx-plugin)
        fi
        if apt_package_available docker-compose-plugin; then
            docker_packages+=(docker-compose-plugin)
        fi
    else
        info "未检测到官方 Docker APT 仓库，安装 Ubuntu 仓库中的 docker.io / docker-compose-v2"
        docker_packages=(docker.io docker-compose-v2)
    fi

    "${SUDO_CMD[@]}" apt-get install -y "${docker_packages[@]}"
}

print_usage() {
    cat <<'EOF'
用法: bash scripts/rm26-recording-install.sh [选项]

选项:
  --with-desktop     安装桌面快捷方式
  --with-autostart   安装桌面快捷方式并开启开机自启动
  --skip-docker      跳过 Docker / Compose 安装
  -h, --help         显示帮助
EOF
}

for arg in "$@"; do
    case "$arg" in
        --with-desktop)
            INSTALL_DESKTOP=true
            ;;
        --with-autostart)
            INSTALL_DESKTOP=true
            INSTALL_AUTOSTART=true
            ;;
        --skip-docker)
            SKIP_DOCKER=true
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            warn "未知参数: $arg"
            print_usage
            exit 1
            ;;
    esac
done

if [ "$(uname -s)" != "Linux" ]; then
    warn "该安装脚本仅面向 Ubuntu / Debian Linux 宿主机"
    exit 1
fi

if ! command -v sudo >/dev/null 2>&1 && [ "$(id -u)" -ne 0 ]; then
    warn "需要 sudo 或 root 权限来安装依赖"
    exit 1
fi

SUDO_CMD=()
if [ "$(id -u)" -ne 0 ]; then
    SUDO_CMD=(sudo)
fi

step "安装 RM26 自动录屏依赖"

packages=(
    ffmpeg
    mpv
    xdotool
    x11-xserver-utils
    pulseaudio-utils
    socat
)

if [ "$SKIP_DOCKER" = false ]; then
    info "将按当前系统仓库情况自动处理 Docker / Compose 安装"
fi

"${SUDO_CMD[@]}" apt-get update
"${SUDO_CMD[@]}" apt-get install -y "${packages[@]}"

if [ "$SKIP_DOCKER" = false ]; then
    install_docker_packages

    step "启用 Docker 服务"
    "${SUDO_CMD[@]}" systemctl enable --now docker

    if ! id -nG "$USER" 2>/dev/null | tr ' ' '\n' | grep -qx docker; then
        "${SUDO_CMD[@]}" usermod -aG docker "$USER" || true
        info "已尝试把当前用户加入 docker 组"
        info "如果当前终端还不能直接运行 docker，请注销后重新登录一次"
    fi
fi

mkdir -p "$PROJECT_ROOT/recordings" "$PROJECT_ROOT/tmp/log" "$PROJECT_ROOT/tmp/recording-state"

if [ "$INSTALL_DESKTOP" = true ]; then
    step "安装桌面启动入口"
    if [ "$INSTALL_AUTOSTART" = true ]; then
        bash "$SCRIPT_DIR/rm26-field-install.sh"
    else
        bash "$SCRIPT_DIR/rm26-field-install.sh" --no-autostart
    fi
fi

echo
info "安装完成"
echo "  - 录屏输出目录: $PROJECT_ROOT/recordings"
echo "  - 本地播放器: mpv"
echo "  - 播放最新录屏: ./videoplayer.sh"
echo "  - 直接运行 Docker 客户端: ./run_docker.sh --field-client"
echo "  - 关闭自动录屏: RM_AUTO_RECORD=0 ./run_docker.sh --field-client"
echo "  - 桌面模式启动: ./scripts/rm26-field-launch.sh --docker"
