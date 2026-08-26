#!/bin/bash
# 连接现场引擎机。目标必须由现场配置显式提供，避免误连到旧环境。

set -euo pipefail

if [[ -z "${RM_ENGINE_SSH_TARGET:-}" ]]; then
  echo "错误：未设置 RM_ENGINE_SSH_TARGET。" >&2
  echo "示例：RM_ENGINE_SSH_TARGET=user@engine-host $0 [command]" >&2
  exit 2
fi

if [[ "$RM_ENGINE_SSH_TARGET" == -* ]]; then
  echo "错误：RM_ENGINE_SSH_TARGET 不能以 '-' 开头。" >&2
  exit 2
fi

exec ssh "$RM_ENGINE_SSH_TARGET" "$@"
