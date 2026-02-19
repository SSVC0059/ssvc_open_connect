#!/bin/bash
# Сборка Docker-образа ssvc-open-connect
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
cd "$PROJECT_ROOT"
docker build -t ssvc-open-connect .
