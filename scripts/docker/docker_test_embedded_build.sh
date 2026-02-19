#!/bin/bash
# Embedded-тесты: сборка без загрузки (в Docker)
# Для полного запуска с загрузкой — используйте pio test локально (нужен COM-порт)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
cd "$PROJECT_ROOT"
docker run --rm -v "$PROJECT_ROOT:/project" ssvc-open-connect test -e esp32-s3-devkitc-1-16m-test --without-uploading --without-testing
