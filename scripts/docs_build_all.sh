#!/bin/bash
# Обёртка: см. scripts/docs/
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/docs/docs_build_all.sh" "$@"
