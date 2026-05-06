#!/usr/bin/env bash
# Полный прогон: PlatformIO (native + embedded по USB), затем боевой upload и HTTP-смоук.
# Использование: ./scripts/run_full_tests_live.sh COM25 http://10.5.1.117
#
# Опционально (переменные окружения):
#   SKIP_NATIVE=1              — только embedded + upload + smoke (без native)
#   SKIP_PRODUCTION_FLASH=1    — не делать pio run боевой прошивки перед смоуком (не рекомендуется)
#   PRODUCTION_ENV=esp32-s3-devkitc-1-16m  — env для upload после тестов (по умолчанию как здесь)
#   POST_FLASH_DELAY_SEC=12    — пауза после upload перед смоуком (сек)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PORT="${1:?Usage: $0 <COM_PORT> <DEVICE_BASE_URL> [extra smoke_rest.py args...]}"
URL="${2:?}"
shift 2

PRODUCTION_ENV="${PRODUCTION_ENV:-esp32-s3-devkitc-1-16m}"
POST_FLASH_DELAY_SEC="${POST_FLASH_DELAY_SEC:-12}"

if [[ "${SKIP_NATIVE:-0}" == "1" ]]; then
  echo "== PlatformIO: esp32-s3-devkitc-1-16m-test only (${PORT}); native skipped =="
  pio test -e esp32-s3-devkitc-1-16m-test --upload-port "$PORT" --test-port "$PORT"
else
  echo "== PlatformIO: native + esp32-s3-devkitc-1-16m-test (${PORT}) =="
  pio test -e native -e esp32-s3-devkitc-1-16m-test --upload-port "$PORT" --test-port "$PORT"
fi

if [[ "${SKIP_PRODUCTION_FLASH:-0}" != "1" ]]; then
  echo "== Restore production firmware: ${PRODUCTION_ENV} (${PORT}) — затем HTTP-смоук =="
  echo "(После pio test на flash остаётся Unity; смоук ожидает основную прошивку.)"
  pio run -e "$PRODUCTION_ENV" -t upload --upload-port "$PORT"
  if [[ "${POST_FLASH_DELAY_SEC}" -gt 0 ]]; then
    echo "Waiting ${POST_FLASH_DELAY_SEC}s for reboot / Wi‑Fi…"
    sleep "${POST_FLASH_DELAY_SEC}"
  fi
fi

echo "== smoke_rest.py: ${URL} --extended =="
python3 scripts/smoke_rest.py "$URL" \
  --user "${SMOKE_USER:-admin}" \
  --password "${SMOKE_PASSWORD:-admin}" \
  --extended \
  "$@"
