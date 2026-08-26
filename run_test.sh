#!/bin/bash
# ==============================================================================
# run_test.sh — 模拟器测试专用启动脚本
#
# 启动顺序：
#   1. Docker MQTT 服务（Mosquitto）
#   2. 协议仿真模拟器（FastAPI :8000 + MQTT Publisher）
#   3. Docker RM26 QtClient（图传 + 战术指挥屏）
#
# 退出时自动清理所有进程。
#
# 用法: ./run_test.sh [--build] [--field-client]
#       浏览器打开 http://localhost:8000 进入模拟器控制台
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"

# ----------------------------------------------------------------------
# 解析 MQTT Host Port（与 docker-compose.yml 的 RM_MQTT_HOST_PORT 一致）
# ----------------------------------------------------------------------
MQTT_PORT="${RM_MQTT_HOST_PORT:-11883}"
if [ -f "$ENV_FILE" ]; then
  while IFS='=' read -r key value; do
    case "${key%%#*}" in
      RM_MQTT_HOST_PORT) MQTT_PORT="${value:-11883}" ;;
    esac
  done < "$ENV_FILE"
fi

# ----------------------------------------------------------------------
# 测试环境网络配置：全部指向本地
# 必须在调用 run_docker.sh 之前 export，否则 run_docker.sh 的硬编码
# 默认值（3333）会覆盖 .env 中配置的 MQTT 端口（11883）。
# ----------------------------------------------------------------------
export RM_SERVER_IP="${RM_SERVER_IP:-127.0.0.1}"
export RM_MQTT_BROKER="${RM_MQTT_BROKER:-127.0.0.1}"
export RM_MQTT_HOST_PORT="$MQTT_PORT"
export RM_MQTT_PORT="$MQTT_PORT"

# ----------------------------------------------------------------------
# 清理函数
# ----------------------------------------------------------------------
SIM_PID=""
cleanup() {
  if [ -n "${SIM_PID:-}" ] && kill -0 "$SIM_PID" 2>/dev/null; then
    echo ""
    echo "[run_test] 正在关闭模拟器 (PID: $SIM_PID)..."
    kill "$SIM_PID" 2>/dev/null || true
    wait "$SIM_PID" 2>/dev/null || true
    echo "[run_test] 模拟器已关闭"
  fi
}
trap cleanup EXIT INT TERM

# ----------------------------------------------------------------------
# 判断是否为 field-client 模式（field compose 不带 MQTT Broker）
# ----------------------------------------------------------------------
FIELD_MODE=false
for arg in "$@"; do
  if [ "$arg" = "--field-client" ]; then
    FIELD_MODE=true
    break
  fi
done

# ----------------------------------------------------------------------
# 步骤 1：启动 MQTT Broker（必须在模拟器之前就绪）
# ----------------------------------------------------------------------
if [ "$FIELD_MODE" = false ]; then
  echo "[run_test] Step 1/3: 启动 MQTT Broker (端口 ${MQTT_PORT})..."
  docker compose -f "$SCRIPT_DIR/docker-compose.yml" up rm26-mqtt -d

  echo -n "[run_test] 等待 MQTT Broker 就绪"
  for i in $(seq 1 15); do
    if python3 -c "import socket; s=socket.socket(); s.settimeout(1); s.connect(('127.0.0.1',${MQTT_PORT})); s.close()" 2>/dev/null; then
      echo " ✓"
      break
    fi
    echo -n "."
    sleep 1
  done
else
  echo "[run_test] Step 1/3: field-client 模式，跳过本地 MQTT Broker"
fi

# ----------------------------------------------------------------------
# 步骤 2：启动协议仿真模拟器
# ----------------------------------------------------------------------
echo "[run_test] Step 2/3: 启动模拟器 (MQTT: 127.0.0.1:${MQTT_PORT}, Web: :8000)"
"$SCRIPT_DIR/sim/run_sim.sh" \
    --mqtt-host 127.0.0.1 \
    --mqtt-port "$MQTT_PORT" &
SIM_PID=$!

echo -n "[run_test] 等待模拟器 Web 就绪"
for i in $(seq 1 30); do
  if curl -s -o /dev/null http://localhost:8000/ 2>/dev/null; then
    echo " ✓"
    echo "[run_test] 模拟器已就绪 → http://localhost:8000"
    break
  fi
  echo -n "."
  sleep 1
done

# ----------------------------------------------------------------------
# 步骤 3：启动 Qt 客户端（沿用 run_docker.sh 的完整逻辑）
# ----------------------------------------------------------------------
echo "[run_test] Step 3/3: 启动 RM26 客户端..."
echo "[run_test] ----------------------------------------"
"$SCRIPT_DIR/run_docker.sh" "$@"
