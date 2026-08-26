#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RECORDINGS_DIR="${RM_RECORD_OUTPUT_DIR:-$SCRIPT_DIR/recordings}"

if ! command -v mpv >/dev/null 2>&1; then
    echo "未找到 mpv，请先运行: bash scripts/rm26-recording-install.sh"
    exit 1
fi

TARGET_FILE="${1:-}"

if [ -z "$TARGET_FILE" ]; then
    TARGET_FILE="$(find "$RECORDINGS_DIR" -maxdepth 1 -type f -name '*.mp4' | sort | tail -n 1)"
fi

if [ -z "$TARGET_FILE" ] || [ ! -f "$TARGET_FILE" ]; then
    echo "未找到可播放的录屏文件。目录: $RECORDINGS_DIR"
    exit 1
fi

exec mpv "$TARGET_FILE"
