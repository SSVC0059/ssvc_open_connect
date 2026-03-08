#!/bin/bash
# Сборка прошивки в Docker
# Запуск: ./scripts/docker/docker_build.sh [env]
# Пример: ./scripts/docker/docker_build.sh esp32-s3-devkitc-1-16m
ENV="${1:-esp32-s3-devkitc-1-16m}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
cd "$PROJECT_ROOT"
docker run --rm -v "$PROJECT_ROOT:/project" ssvc-open-connect run -e "$ENV"
