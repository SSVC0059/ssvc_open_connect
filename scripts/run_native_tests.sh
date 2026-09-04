#!/usr/bin/env bash
# Compile & run native unit tests with the system g++ (no PlatformIO, no network).
#
# Use this as an offline fallback when `pio test -e native` cannot install the
# native platform or the `doctest` dependency (e.g. no access to the PlatformIO
# registry). CI and online environments still use `pio test -e native`.
#
# Requirements: g++ in PATH (MinGW-w64 on Windows, or WSL), bash.
set -euo pipefail
cd "$(dirname "$0")/.."

OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

INCLUDES="-I lib/ssvcOpenConnect -I lib/doctest/src"
TESTS=(test_stringview test_utils test_uart_api_spec test_rectification_parse)

for t in "${TESTS[@]}"; do
  echo "== build+run: ${t}"
  g++ -std=gnu++14 ${INCLUDES} "test/native/${t}/test_main.cpp" -o "${OUT_DIR}/${t}"
  "${OUT_DIR}/${t}"
done

echo "All native tests passed."
