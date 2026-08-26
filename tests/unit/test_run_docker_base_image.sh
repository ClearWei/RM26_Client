#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUN_DOCKER="$ROOT_DIR/run_docker.sh"
CLIENT_DOCKERFILE="$ROOT_DIR/docker/client.Dockerfile"
COMPOSE_FILE="$ROOT_DIR/docker-compose.yml"
FIELD_COMPOSE_FILE="$ROOT_DIR/docker-compose.field.yml"
ENV_EXAMPLE="$ROOT_DIR/.env.docker.example"
TMP_RUN_DOCKER="$(mktemp)"
trap 'rm -f "$TMP_RUN_DOCKER"' EXIT

tr -d '\r' < "$RUN_DOCKER" > "$TMP_RUN_DOCKER"

bash -n "$TMP_RUN_DOCKER"

grep -q '^ARG RM26_CLIENT_BASE_IMAGE=ubuntu:24.04$' "$CLIENT_DOCKERFILE"
grep -q '^FROM ${RM26_CLIENT_BASE_IMAGE}$' "$CLIENT_DOCKERFILE"
grep -q '^ensure_client_base_image()' "$RUN_DOCKER"
grep -q 'docker pull "$image"' "$RUN_DOCKER"
grep -q 'RM26_CLIENT_BASE_IMAGE="${RM26_CLIENT_BASE_IMAGE:-ubuntu:24.04}"' "$RUN_DOCKER"
grep -q 'RM_DOCKER_PULL_RETRIES="${RM_DOCKER_PULL_RETRIES:-3}"' "$RUN_DOCKER"
grep -q 'RM_DOCKER_PULL_RETRY_DELAY_SEC="${RM_DOCKER_PULL_RETRY_DELAY_SEC:-5}"' "$RUN_DOCKER"
grep -q 'RM26_CLIENT_BASE_IMAGE: ${RM26_CLIENT_BASE_IMAGE:-ubuntu:24.04}' "$COMPOSE_FILE"
grep -q 'RM26_CLIENT_BASE_IMAGE: ${RM26_CLIENT_BASE_IMAGE:-ubuntu:24.04}' "$FIELD_COMPOSE_FILE"
grep -q '^RM26_CLIENT_BASE_IMAGE=ubuntu:24.04$' "$ENV_EXAMPLE"
grep -q '^RM_DOCKER_PULL_RETRIES=3$' "$ENV_EXAMPLE"
grep -q '^RM_DOCKER_PULL_RETRY_DELAY_SEC=5$' "$ENV_EXAMPLE"

printf 'test_run_docker_base_image: PASS\n'
