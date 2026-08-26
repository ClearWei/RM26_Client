#!/usr/bin/env bash
# ==============================================================================
# RM26 客户端桌面集成安装脚本 (Ubuntu)
#
# 功能:
#   1. 生成带正确绝对路径的 .desktop 文件
#   2. 安装到 ~/Desktop/ (桌面快捷方式)
#   3. 安装到 ~/.config/autostart/ (开机自启动)
#   4. 安装到 ~/.local/share/applications/ (应用菜单)
#
# 用法:
#   bash scripts/rm26-field-install.sh              # 完整安装
#   bash scripts/rm26-field-install.sh --no-autostart  # 仅桌面快捷方式,不开机自启
#   bash scripts/rm26-field-install.sh --uninstall     # 卸载
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

LAUNCH_SCRIPT="$SCRIPT_DIR/rm26-field-launch.sh"
ICON_PATH="$PROJECT_ROOT/resources/images/app_icon.png"

INSTALL_AUTOSTART=true
ACTION="install"

# 颜色
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'
info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${RED}[WARN]${NC} $1"; }

# 解析参数
for arg in "$@"; do
    case "$arg" in
        --no-autostart) INSTALL_AUTOSTART=false ;;
        --uninstall)    ACTION="uninstall" ;;
        -h|--help)
            echo "用法: $0 [--no-autostart] [--uninstall]"
            exit 0
            ;;
        *) warn "未知参数: $arg"; exit 1 ;;
    esac
done

# ==== 卸载 ====
if [ "$ACTION" = "uninstall" ]; then
    info "正在卸载..."
    rm -f "$HOME/Desktop/rm26-field.desktop"
    rm -f "$HOME/.config/autostart/rm26-field.desktop"
    rm -f "$HOME/.local/share/applications/rm26-field.desktop"
    update-desktop-database "$HOME/.local/share/applications/" 2>/dev/null || true
    info "卸载完成"
    exit 0
fi

# ==== 安装 ====
info "项目目录: $PROJECT_ROOT"
info "启动脚本: $LAUNCH_SCRIPT"
info "图标路径: $ICON_PATH"

# 校验
if [ ! -f "$LAUNCH_SCRIPT" ]; then
    warn "未找到启动脚本: $LAUNCH_SCRIPT"; exit 1
fi
if [ ! -f "$ICON_PATH" ]; then
    warn "未找到图标: $ICON_PATH，将跳过图标设置"
    ICON_PATH=""  # 留空则表示无图标
fi

# 确保启动脚本有执行权限
chmod +x "$LAUNCH_SCRIPT"

# 生成 .desktop 文件 (使用绝对路径)
DESKTOP_CONTENT="[Desktop Entry]
Version=1.0
Name=RM26 客户端
Name[zh_CN]=RM26 自定义客户端
Comment=FDU EGA RoboMaster 2026 Custom Client
Comment[zh_CN]=复旦大学 EGA RoboMaster 2026 自定义客户端
Exec=$LAUNCH_SCRIPT
Icon=$ICON_PATH
Type=Application
Terminal=false
Categories=Game;
StartupNotify=false
X-GNOME-Autostart-enabled=true"

# 1) 桌面快捷方式
mkdir -p "$HOME/Desktop"
echo "$DESKTOP_CONTENT" > "$HOME/Desktop/rm26-field.desktop"
chmod +x "$HOME/Desktop/rm26-field.desktop"
info "桌面快捷方式已创建: ~/Desktop/rm26-field.desktop"

# 2) 应用菜单
mkdir -p "$HOME/.local/share/applications"
echo "$DESKTOP_CONTENT" > "$HOME/.local/share/applications/rm26-field.desktop"
update-desktop-database "$HOME/.local/share/applications/" 2>/dev/null || true
info "应用菜单已注册"

# 3) 开机自启动
if [ "$INSTALL_AUTOSTART" = true ]; then
    mkdir -p "$HOME/.config/autostart"
    cp "$HOME/.local/share/applications/rm26-field.desktop" \
       "$HOME/.config/autostart/rm26-field.desktop"
    info "开机自启动已启用: ~/.config/autostart/rm26-field.desktop"
else
    info "开机自启动已跳过（使用 --no-autostart）"
fi

echo ""
info "===== 安装完成 ====="
echo "  - 桌面快捷方式: 双击桌面上的 \"RM26 客户端\" 图标"
echo "  - 应用菜单: 搜索 \"RM26\""
if [ "$INSTALL_AUTOSTART" = true ]; then
    echo "  - 开机自启动: 下次重启自动运行"
fi
echo "  - 日志文件: tmp/log/field_client.log"
echo "  - 录屏目录: recordings/"
echo ""
echo "如需先安装录屏 / Docker 依赖: bash $SCRIPT_DIR/rm26-recording-install.sh"
echo "如需 Docker 模式: 编辑 $LAUNCH_SCRIPT 将 MODE 改为 docker"
echo "或运行: bash $LAUNCH_SCRIPT --docker"
