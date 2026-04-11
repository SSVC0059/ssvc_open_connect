#!/bin/bash
# Обёртка: см. scripts/test/
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/test/run_all_tests.sh" "$@"
