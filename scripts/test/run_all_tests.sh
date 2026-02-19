#!/usr/bin/env bash
# Полный запуск тестов: native + embedded
# Использование: ./scripts/test/run_all_tests.sh [COM25]
# Вызвать из MSYS2 UCRT64 или с g++ в PATH
PORT="${1:-COM25}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
cd "$PROJECT_ROOT"

echo "=== Native tests ==="
pio test -e native || exit $?

echo ""
echo "=== Embedded tests (port: $PORT) ==="
pio test -e esp32-s3-devkitc-1-16m-test --upload-port "$PORT" --test-port "$PORT"
