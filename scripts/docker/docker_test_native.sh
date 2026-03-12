#!/bin/bash
# Native-тесты в Docker
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
cd "$PROJECT_ROOT"
docker run --rm -v "$PROJECT_ROOT:/project" ssvc-open-connect test -e native
