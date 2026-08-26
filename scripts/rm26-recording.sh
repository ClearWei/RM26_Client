#!/usr/bin/env bash

set -euo pipefail

RM26_RECORDING_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RM26_RECORDING_PROJECT_ROOT="$(dirname "$RM26_RECORDING_SCRIPT_DIR")"
RM26_RECORDING_OUTPUT_DIR="${RM_RECORD_OUTPUT_DIR:-$RM26_RECORDING_PROJECT_ROOT/recordings}"
RM26_RECORDING_STATE_DIR="${RM_RECORD_STATE_DIR:-$RM26_RECORDING_PROJECT_ROOT/tmp/recording-state}"
RM26_RECORDING_LOG_DIR="${RM_RECORD_LOG_DIR:-$RM26_RECORDING_PROJECT_ROOT/tmp/log}"
RM26_RECORDING_CONFIG_FILE="${RM_RECORD_CONFIG_FILE:-$RM26_RECORDING_PROJECT_ROOT/config.json}"
RM26_RECORDING_FPS="${RM_RECORD_FPS:-30}"
RM26_RECORDING_CRF="${RM_RECORD_CRF:-23}"
RM26_RECORDING_PRESET="${RM_RECORD_PRESET:-veryfast}"
RM26_RECORDING_DRAW_MOUSE="${RM_RECORD_DRAW_MOUSE:-1}"
RM26_RECORDING_START_DELAY_SEC="${RM_RECORD_START_DELAY_SEC:-1}"
RM26_RECORDING_AUDIO_ENABLED="${RM_RECORD_AUDIO:-1}"
RM26_RECORDING_AUDIO_BITRATE="${RM_RECORD_AUDIO_BITRATE:-192k}"
RM26_RECORDING_AUDIO_SAMPLE_RATE="${RM_RECORD_AUDIO_SAMPLE_RATE:-48000}"
RM26_RECORDING_AUDIO_CHANNELS="${RM_RECORD_AUDIO_CHANNELS:-2}"

rm26_recording_emit() {
    local level="$1"
    shift
    local message="$*"

    if declare -F "$level" >/dev/null 2>&1; then
        "$level" "$message"
        return 0
    fi

    printf '[RM26 RECORD] %s\n' "$message"
}

rm26_recording_enabled() {
    case "${RM_AUTO_RECORD:-1}" in
        0|false|FALSE|off|OFF|no|NO)
            return 1
            ;;
        *)
            return 0
            ;;
    esac
}

rm26_recording_audio_enabled() {
    case "$RM26_RECORDING_AUDIO_ENABLED" in
        0|false|FALSE|off|OFF|no|NO)
            return 1
            ;;
        *)
            return 0
            ;;
    esac
}

rm26_recording_monitor_pid_file() {
    printf '%s/%s-monitor.pid\n' "$RM26_RECORDING_STATE_DIR" "${1:-default}"
}

rm26_recording_ffmpeg_pid_file() {
    printf '%s/%s-ffmpeg.pid\n' "$RM26_RECORDING_STATE_DIR" "${1:-default}"
}

rm26_recording_session_file() {
    printf '%s/%s-session.env\n' "$RM26_RECORDING_STATE_DIR" "${1:-default}"
}

rm26_recording_ensure_dirs() {
    mkdir -p "$RM26_RECORDING_OUTPUT_DIR" "$RM26_RECORDING_STATE_DIR" "$RM26_RECORDING_LOG_DIR"
}

rm26_recording_config_window_title() {
    if [ -n "${RM_RECORD_WINDOW_NAME_PATTERN:-}" ]; then
        printf '%s\n' "$RM_RECORD_WINDOW_NAME_PATTERN"
        return 0
    fi

    if [ ! -f "$RM26_RECORDING_CONFIG_FILE" ] || ! command -v python3 >/dev/null 2>&1; then
        printf '%s\n' 'FDU EGA 2026 Command Center'
        return 0
    fi

    python3 - <<'PY' "$RM26_RECORDING_CONFIG_FILE"
import json
import sys

config_path = sys.argv[1]
try:
    with open(config_path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    title = data.get("ui_text", {}).get("window_title") or "FDU EGA 2026 Command Center"
except Exception:
    title = "FDU EGA 2026 Command Center"
print(title)
PY
}

rm26_recording_audio_source() {
    if ! rm26_recording_audio_enabled; then
        return 1
    fi

    if [ -n "${RM_RECORD_AUDIO_SOURCE:-}" ]; then
        printf '%s\n' "$RM_RECORD_AUDIO_SOURCE"
        return 0
    fi

    if ! command -v pactl >/dev/null 2>&1; then
        return 1
    fi

    local sink
    sink="$(pactl get-default-sink 2>/dev/null | tr -d '\r' || true)"
    if [ -n "$sink" ]; then
        local monitor_source="${sink}.monitor"
        if pactl list short sources 2>/dev/null | awk '{print $2}' | grep -Fxq "$monitor_source"; then
            printf '%s\n' "$monitor_source"
            return 0
        fi
    fi

    local fallback_monitor
    fallback_monitor="$(
        pactl list short sources 2>/dev/null |
            awk '$2 ~ /\.monitor$/ { print $2; exit }'
    )"
    if [ -n "$fallback_monitor" ]; then
        printf '%s\n' "$fallback_monitor"
        return 0
    fi

    return 1
}

rm26_recording_write_session_var() {
    local session_file="$1"
    local key="$2"
    local value="${3:-}"
    printf '%s=%s\n' "$key" "$value" >> "$session_file"
}

rm26_recording_can_run() {
    if ! rm26_recording_enabled; then
        return 1
    fi

    if [ "$(uname -s)" != "Linux" ]; then
        rm26_recording_emit log_warn "自动录屏仅在 Linux/X11 宿主机启用，当前已跳过"
        return 1
    fi

    if [ -z "${DISPLAY:-}" ]; then
        rm26_recording_emit log_warn "未检测到 DISPLAY，自动录屏已跳过"
        return 1
    fi

    local missing=()
    local tool
    for tool in ffmpeg xdotool; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing+=("$tool")
        fi
    done

    if [ "${#missing[@]}" -gt 0 ]; then
        rm26_recording_emit log_warn "缺少录屏依赖: ${missing[*]}，请先运行 scripts/rm26-recording-install.sh"
        return 1
    fi

    return 0
}

rm26_recording_stop_pid_file() {
    local pid_file="$1"

    if [ ! -f "$pid_file" ]; then
        return 0
    fi

    local pid
    pid="$(cat "$pid_file" 2>/dev/null || true)"
    rm -f "$pid_file"

    if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
        return 0
    fi

    kill -INT "$pid" 2>/dev/null || true
    for _ in $(seq 1 20); do
        if ! kill -0 "$pid" 2>/dev/null; then
            wait "$pid" 2>/dev/null || true
            return 0
        fi
        sleep 0.25
    done

    kill -TERM "$pid" 2>/dev/null || true
    for _ in $(seq 1 20); do
        if ! kill -0 "$pid" 2>/dev/null; then
            wait "$pid" 2>/dev/null || true
            return 0
        fi
        sleep 0.25
    done

    kill -KILL "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
}

rm26_recording_stop() {
    local namespace="${1:-default}"

    rm26_recording_ensure_dirs
    rm26_recording_stop_pid_file "$(rm26_recording_ffmpeg_pid_file "$namespace")"
    rm26_recording_stop_pid_file "$(rm26_recording_monitor_pid_file "$namespace")"
    rm -f "$(rm26_recording_session_file "$namespace")"
}

rm26_recording_status() {
    local namespace="${1:-default}"
    local ffmpeg_pid_file
    local session_file

    ffmpeg_pid_file="$(rm26_recording_ffmpeg_pid_file "$namespace")"
    session_file="$(rm26_recording_session_file "$namespace")"

    if [ -f "$ffmpeg_pid_file" ]; then
        local pid
        pid="$(cat "$ffmpeg_pid_file" 2>/dev/null || true)"
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            printf 'recording namespace=%s pid=%s\n' "$namespace" "$pid"
            if [ -f "$session_file" ]; then
                cat "$session_file"
            fi
            return 0
        fi
    fi

    printf 'recording namespace=%s stopped\n' "$namespace"
}

rm26_recording_monitor_loop() {
    local namespace="$1"
    local title
    local timestamp
    local output_path
    local session_log
    local monitor_pid_file
    local ffmpeg_pid_file
    local session_file
    local window_id
    local ffmpeg_pid
    local audio_source=""
    local -a ffmpeg_audio_args=()

    title="$(rm26_recording_config_window_title)"
    timestamp="$(date '+%Y%m%d-%H%M%S')"
    output_path="$RM26_RECORDING_OUTPUT_DIR/${timestamp}.mp4"
    session_log="$RM26_RECORDING_LOG_DIR/recording-${timestamp}.log"
    monitor_pid_file="$(rm26_recording_monitor_pid_file "$namespace")"
    ffmpeg_pid_file="$(rm26_recording_ffmpeg_pid_file "$namespace")"
    session_file="$(rm26_recording_session_file "$namespace")"

    cat > "$session_file" <<EOF
RM_RECORD_OUTPUT_FILE=$output_path
RM_RECORD_LOG_FILE=$session_log
RM_RECORD_WINDOW_TITLE=$title
RM_RECORD_DISPLAY=${DISPLAY:-}
EOF

    rm26_recording_emit log_info "自动录屏待命中，窗口标题: $title"

    window_id="$(xdotool search --sync --onlyvisible --name "$title" 2>/dev/null | tail -n 1)"
    sleep "$RM26_RECORDING_START_DELAY_SEC"

    cat >> "$session_file" <<EOF
RM_RECORD_WINDOW_ID=$window_id
EOF

    if audio_source="$(rm26_recording_audio_source)"; then
        ffmpeg_audio_args=(
            -thread_queue_size 4096
            -f pulse
            -sample_rate "$RM26_RECORDING_AUDIO_SAMPLE_RATE"
            -channels "$RM26_RECORDING_AUDIO_CHANNELS"
            -i "$audio_source"
            -c:a aac
            -b:a "$RM26_RECORDING_AUDIO_BITRATE"
        )
        rm26_recording_emit log_info "自动录屏将同时录制系统声音，音频源: $audio_source"
        rm26_recording_write_session_var "$session_file" "RM_RECORD_AUDIO_SOURCE" "$audio_source"
    else
        rm26_recording_emit log_warn "未找到可用系统音频 monitor source，当前录屏将退回纯画面"
        rm26_recording_write_session_var "$session_file" "RM_RECORD_AUDIO_SOURCE" "disabled"
    fi

    rm26_recording_emit log_info "自动录屏已开始，文件: $output_path"

    ffmpeg -hide_banner -loglevel warning -y \
        -f x11grab \
        -draw_mouse "$RM26_RECORDING_DRAW_MOUSE" \
        -framerate "$RM26_RECORDING_FPS" \
        -window_id "$window_id" \
        -i "${DISPLAY:-:0}" \
        "${ffmpeg_audio_args[@]}" \
        -c:v libx264 \
        -preset "$RM26_RECORDING_PRESET" \
        -crf "$RM26_RECORDING_CRF" \
        -pix_fmt yuv420p \
        -movflags +faststart \
        "$output_path" >> "$session_log" 2>&1 &
    ffmpeg_pid=$!
    echo "$ffmpeg_pid" > "$ffmpeg_pid_file"
    rm -f "$monitor_pid_file"

    wait "$ffmpeg_pid"
    rm -f "$ffmpeg_pid_file"
}

rm26_recording_start() {
    local namespace="${1:-default}"
    local monitor_pid
    local monitor_pid_file

    if ! rm26_recording_can_run; then
        return 0
    fi

    rm26_recording_ensure_dirs
    rm26_recording_stop "$namespace"

    monitor_pid_file="$(rm26_recording_monitor_pid_file "$namespace")"
    (
        set +e
        rm26_recording_monitor_loop "$namespace"
    ) &
    monitor_pid=$!
    echo "$monitor_pid" > "$monitor_pid_file"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    action="${1:-status}"
    namespace="${2:-default}"

    case "$action" in
        start)
            rm26_recording_start "$namespace"
            ;;
        stop)
            rm26_recording_stop "$namespace"
            ;;
        status)
            rm26_recording_status "$namespace"
            ;;
        *)
            echo "用法: $0 [start|stop|status] [namespace]"
            exit 1
            ;;
    esac
fi
