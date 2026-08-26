#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="$ROOT_DIR/run_docker.sh"
TMP_SCRIPT="$(mktemp)"
trap 'rm -f "$TMP_SCRIPT"' EXIT

tr -d '\r' < "$SCRIPT" > "$TMP_SCRIPT"

bash -n "$TMP_SCRIPT"

grep -q 'COMPOSE_ARGS=(up)' "$SCRIPT"
grep -q '^main "$@"$' "$SCRIPT"
grep -q '^validate_field_runtime_config()' "$SCRIPT"
grep -Fq '${RM_SERVER_IP:?请显式设置 RM_SERVER_IP}' "$ROOT_DIR/docker-compose.field.yml"
grep -Fq '${RM_MQTT_BROKER:?请显式设置 RM_MQTT_BROKER}' "$ROOT_DIR/docker-compose.field.yml"
grep -Fq '${RM_CLIENT_ROBOT_ID:?请显式设置 RM_CLIENT_ROBOT_ID}' "$ROOT_DIR/docker-compose.field.yml"

if grep -qE 'start_field_client_detached|up -d' "$SCRIPT"; then
    echo "field client must remain attached to foreground logs" >&2
    exit 1
fi

# 只抽取纯校验函数，避免单元测试启动 Docker 或修改现场状态。
log_error() { :; }
eval "$(sed -n '/^is_loopback_host()/,/^}/p; /^validate_field_runtime_config()/,/^}/p' "$SCRIPT")"

RM_SERVER_IP=127.0.0.1
RM_MQTT_BROKER=127.0.0.1
RM_CLIENT_ROBOT_ID=1
if validate_field_runtime_config; then
    echo "field validation must reject loopback defaults" >&2
    exit 1
fi

RM_SERVER_IP=192.0.2.10
RM_MQTT_BROKER=192.0.2.11
RM_CLIENT_ROBOT_ID=1
validate_field_runtime_config

printf 'test_run_docker_field: PASS\n'
