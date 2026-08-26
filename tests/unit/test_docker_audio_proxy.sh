#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HELPER="$ROOT_DIR/scripts/docker-audio-proxy.sh"
RUN_DOCKER="$ROOT_DIR/run_docker.sh"
TMP_HELPER="$(mktemp)"
TMP_RUN_DOCKER="$(mktemp)"
trap 'rm -f "$TMP_HELPER" "$TMP_RUN_DOCKER"' EXIT

tr -d '\r' < "$HELPER" > "$TMP_HELPER"
tr -d '\r' < "$RUN_DOCKER" > "$TMP_RUN_DOCKER"

bash -n "$TMP_HELPER"
bash -n "$TMP_RUN_DOCKER"

grep -q '^rm26_ensure_docker_audio()' "$HELPER"
grep -q '^rm26_start_socat_audio_proxy()' "$HELPER"
grep -q 'TCP-LISTEN:' "$HELPER"
grep -q 'UNIX-CONNECT:' "$HELPER"
grep -q 'RM_AUDIO_PROXY_PORT' "$HELPER"
grep -q 'RM_PULSE_COOKIE_HOST' "$HELPER"
grep -q 'sudo apt-get install -y socat pulseaudio-utils' "$HELPER"
grep -Fq '. "$SCRIPT_DIR/scripts/docker-audio-proxy.sh"' "$RUN_DOCKER"
grep -q '^    rm26_ensure_docker_audio$' "$RUN_DOCKER"
grep -q 'PULSE_SERVER: ${RM_PULSE_SERVER:-tcp:127.0.0.1:47130}' "$ROOT_DIR/docker-compose.yml"
grep -q 'PULSE_SERVER: ${RM_PULSE_SERVER:-tcp:127.0.0.1:47130}' "$ROOT_DIR/docker-compose.field.yml"
grep -q 'PULSE_COOKIE:' "$ROOT_DIR/docker-compose.yml"

# 关闭音频时不应依赖 pactl、socat 或正在监听的套接字。
log_info() { :; }
log_error() { :; }
. "$TMP_HELPER"
RM_DISABLE_AUDIO=1 rm26_ensure_docker_audio

printf 'test_docker_audio_proxy: PASS\n'
