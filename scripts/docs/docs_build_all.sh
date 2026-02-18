#!/usr/bin/env bash
# Сборка полной документации: основной сайт + develop
# Результат: site/ содержит основной сайт, site/develop/ — документация для разработчиков
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
cd "$PROJECT_ROOT"

echo "Building main site..."
mkdocs build || exit $?

echo "Building develop site (output: site/develop/)..."
mkdocs build -f mkdocs-develop.yml -d site/develop || exit $?

echo "Done. Output: site/"
