#!/bin/bash

# Linux 主机共用的 Docker 音频初始化。run_docker.sh 会加载本文件，
# run_test.sh 通过 run_docker.sh 沿用同一套处理逻辑。

rm26_audio_server_works() {
    local server="$1"
    PULSE_SERVER="$server" pactl info >/dev/null 2>&1
}

rm26_audio_proxy_pid_matches() {
    local pid="$1"
    local listen_port="$2"
    local native_socket="$3"
    local command_line

    [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null || return 1
    command_line="$(ps -p "$pid" -o args= 2>/dev/null || true)"
    [[ "$command_line" == *"socat"* ]] &&
        [[ "$command_line" == *"TCP-LISTEN:${listen_port}"* ]] &&
        [[ "$command_line" == *"UNIX-CONNECT:${native_socket}"* ]]
}

rm26_start_socat_audio_proxy() {
    local listen_port="$1"
    local native_socket="$2"
    local pulse_server="tcp:127.0.0.1:$listen_port"
    local runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
    local state_dir="${RM_AUDIO_PROXY_STATE_DIR:-$runtime_dir/rm26}"
    local pid_file="$state_dir/docker-audio-proxy.pid"
    local log_file="$state_dir/docker-audio-proxy.log"
    local old_pid=""
    local proxy_pid
    local attempt

    if ! command -v socat >/dev/null 2>&1; then
        log_error "当前 PipeWire 环境需要 socat 转发 Docker 音频"
        log_error "请安装: sudo apt-get install -y socat pulseaudio-utils"
        return 1
    fi

    if [ ! -S "$native_socket" ]; then
        log_error "未找到宿主机 PipeWire/Pulse socket: $native_socket"
        return 1
    fi

    mkdir -p "$state_dir"
    if [ -f "$pid_file" ]; then
        read -r old_pid < "$pid_file" || true
    fi

    if rm26_audio_proxy_pid_matches "$old_pid" "$listen_port" "$native_socket"; then
        if rm26_audio_server_works "$pulse_server"; then
            log_info "Docker 音频代理已可用: $pulse_server (PID: $old_pid)"
            return 0
        fi
        kill "$old_pid" 2>/dev/null || true
        wait "$old_pid" 2>/dev/null || true
    fi

    nohup socat \
        "TCP-LISTEN:${listen_port},fork,bind=127.0.0.1,reuseaddr" \
        "UNIX-CONNECT:${native_socket}" \
        >"$log_file" 2>&1 &
    proxy_pid=$!
    printf '%s\n' "$proxy_pid" > "$pid_file"

    for attempt in $(seq 1 30); do
        if rm26_audio_server_works "$pulse_server"; then
            log_info "已启动 Docker 音频代理: $pulse_server -> $native_socket (PID: $proxy_pid)"
            return 0
        fi
        if ! kill -0 "$proxy_pid" 2>/dev/null; then
            break
        fi
        sleep 0.1
    done

    log_error "Docker 音频代理启动失败，日志: $log_file"
    return 1
}

rm26_ensure_docker_audio() {
    if [ "$(uname -s)" != "Linux" ]; then
        return 0
    fi

    if [ "${RM_DISABLE_AUDIO:-0}" = "1" ]; then
        log_info "已设置 RM_DISABLE_AUDIO=1，跳过 Docker 音频配置"
        return 0
    fi

    if ! command -v pactl >/dev/null 2>&1; then
        log_error "未找到 pactl，无法配置或验证 Docker 音频"
        log_error "请安装: sudo apt-get install -y socat pulseaudio-utils"
        return 1
    fi

    local proxy_port="${RM_AUDIO_PROXY_PORT:-47130}"
    export RM_PULSE_SERVER="${RM_PULSE_SERVER:-tcp:127.0.0.1:$proxy_port}"
    export RM_PULSE_COOKIE_HOST="${RM_PULSE_COOKIE_HOST:-$HOME/.config/pulse/cookie}"
    export RM_PULSE_COOKIE_CONTAINER="${RM_PULSE_COOKIE_CONTAINER:-/tmp/rm26-pulse-cookie}"

    if [ ! -f "$RM_PULSE_COOKIE_HOST" ]; then
        log_error "未找到 PulseAudio/PipeWire 认证 cookie: $RM_PULSE_COOKIE_HOST"
        return 1
    fi

    if rm26_audio_server_works "$RM_PULSE_SERVER"; then
        log_info "Docker 音频代理已可用: $RM_PULSE_SERVER"
        return 0
    fi

    local native_socket="${RM_PULSE_NATIVE_SOCKET:-${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native}"
    rm26_start_socat_audio_proxy "$proxy_port" "$native_socket"
}
