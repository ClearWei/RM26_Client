#!/usr/bin/env bash

set -euo pipefail

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export QT_X11_NO_MITSHM="${QT_X11_NO_MITSHM:-1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/runtime-root}"

mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
mkdir -p /opt/rmclient/tmp/log

cd /opt/rmclient

exec /opt/rmclient/build/RoboMasterClient2025 "$@"
