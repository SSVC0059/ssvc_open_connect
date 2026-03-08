#!/usr/bin/env bash
# Запуск native-тестов
# Вызвать из MSYS2 UCRT64: ./scripts/test/run_native_tests.sh
# Или: bash scripts/test/run_native_tests.sh (из MSYS2)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
cd "$PROJECT_ROOT"
pio test -e native
