#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="$ROOT_DIR/sim/run_sim.sh"

bash -n "$SCRIPT"

TEST_DIR="$(mktemp -d)"
trap 'rm -rf "$TEST_DIR"' EXIT
mkdir -p "$TEST_DIR/sim" "$TEST_DIR/bin"
cp "$SCRIPT" "$TEST_DIR/sim/run_sim.sh"
cp "$ROOT_DIR/sim/pyproject.toml" "$TEST_DIR/sim/pyproject.toml"

# 该用例验证 Linux 安装提示，不能依赖执行测试的宿主系统。
printf '%s\n' \
    '#!/bin/bash' \
    'printf "Linux\n"' \
    >"$TEST_DIR/bin/uname"
chmod +x "$TEST_DIR/bin/uname"

printf '%s\n' \
    '#!/bin/bash' \
    'if [[ "${1:-}" == "-c" && "${2:-}" == *"sys.version_info"* ]]; then' \
    "    printf '3.12\\n'" \
    'fi' \
    'exit 1' \
    >"$TEST_DIR/bin/python3"
chmod +x "$TEST_DIR/bin/python3"

set +e
output="$(PATH="$TEST_DIR/bin:/usr/bin:/bin" "$TEST_DIR/sim/run_sim.sh" --no-video 2>&1)"
status=$?
set -e

if [ "$status" -eq 0 ]; then
    printf 'dependency check unexpectedly succeeded\n' >&2
    exit 1
fi

required_hints=(
    'Ubuntu/Debian 首次安装请依次运行'
    'sudo apt install -y python3.12-venv python3-pip python3-dev build-essential'
    '/usr/bin/python3 -m venv --clear .venv'
    'source .venv/bin/activate'
    'python -m pip install --upgrade pip setuptools wheel'
    'python -m pip install -e .'
)

for hint in "${required_hints[@]}"; do
    if ! grep -Fq "$hint" <<<"$output"; then
        printf 'missing Linux dependency hint: %s\n' "$hint" >&2
        exit 1
    fi
done

if grep -Fq -- '--break-system-packages' "$SCRIPT"; then
    printf 'unsafe --break-system-packages hint must not be added\n' >&2
    exit 1
fi

printf 'test_linux_sim_dependency_hint: PASS\n'
