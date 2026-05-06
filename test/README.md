# Unit Tests — SSVC OpenConnect

## Структура тестов

- **`native/`** — тесты, выполняемые на ПК (требуют g++/MinGW)
- **`embedded/`** — тесты, выполняемые на ESP32-S3
- **[`scripts/smoke_rest.py`](../scripts/smoke_rest.py)** — интеграционные HTTP-смоук-тесты против прошивки (или `--self-test` без устройства)

## Запуск всех тестов одной командой

### Полный набор на живом устройстве (USB + сеть)

Одна команда выполняет:

1. **`pio test`** — окружения `native` и `esp32-s3-devkitc-1-16m-test` (плата по USB: тестовый бинарник и Unity). После шага на flash **остаётся не основная прошивка** (см. ниже раздел «После тестов в Serial „тишина“»).
2. **`pio run -e <боевой env> -t upload`** — по умолчанию `esp32-s3-devkitc-1-16m`, чтобы **смоук шёл уже по рабочему приложению** (HTTP, REST), а не по Unity-образу. Для KC868 и т.п. задайте другой env (см. параметры скрипта ниже). Короткая пауза после upload — время на перезагрузку и Wi‑Fi.
3. **`scripts/smoke_rest.py --extended`** — HTTP-смоук по **базовому URL** (тот же ESP32 в LAN).

Подставьте **COM-порт** вашей платы и **URL** устройства в LAN (без завершающего слэша):

**PowerShell** (из корня репозитория):

```powershell
.\scripts\run_full_tests_live.ps1 -Port COM25 -Url http://10.5.1.117
```

С другими учётными данными:

```powershell
.\scripts\run_full_tests_live.ps1 -Port COM25 -Url http://10.5.1.117 -User admin -Password admin
```

Параметры PowerShell (частые):

| Параметр | Значение по умолчанию | Смысл |
|----------|----------------------|--------|
| `-ProductionEnv` | `esp32-s3-devkitc-1-16m` | Какой env прошить перед смоуком после embedded-тестов |
| `-PostFlashDelaySeconds` | `12` | Пауза перед HTTP после upload (поднятие Wi‑Fi) |
| `-SkipProductionFlash` | выкл. | Не делать боевой upload (смоук тогда проверяет то, что осталось после Unity — обычно **не нужно**) |
| `-SkipNative` | выкл. | Только embedded + upload + smoke |

**Git Bash / Linux / macOS:**

```bash
chmod +x scripts/run_full_tests_live.sh   # один раз
./scripts/run_full_tests_live.sh COM25 http://10.5.1.117
```

Переменные окружения для bash-версии: `SKIP_NATIVE=1`, `PRODUCTION_ENV=kincony-kc868-a6`, `POST_FLASH_DELAY_SEC=15`, `SKIP_PRODUCTION_FLASH=1` (см. комментарии в начале [`scripts/run_full_tests_live.sh`](../scripts/run_full_tests_live.sh)).

Дополнительные аргументы в конце передаются в `smoke_rest.py` (например `--command-timeout 240`).

Требования: в PATH — `pio` и `python`/`python3`; для native-тестов на Windows при необходимости см. раздел про MSYS2 ниже.

---

PlatformIO позволяет передать **несколько окружений** в одном вызове: сначала выполнятся native-тесты на ПК, затем embedded на порту платы.

### Только Unit-тесты PlatformIO (native + ESP32)

Подставьте свой COM-порт (тот же для загрузки и вывода Unity, см. раздел про UART ниже):

```bash
pio test -e native -e esp32-s3-devkitc-1-16m-test --upload-port COM25 --test-port COM25
```

В **PowerShell** эквивалентно (пример порта):

```powershell
pio test -e native -e esp32-s3-devkitc-1-16m-test --upload-port COM25 --test-port COM25
```

Без подключённой платы выполняйте только хостовые тесты:

```bash
pio test -e native
```

### Unit-тесты + смоук-скрипт (без устройства в сети)

После успешных native-тестов имеет смысл прогнать смоук в режиме встроенного mock-сервера (проверка самого Python-скрипта, без ESP32 по HTTP):

```bash
pio test -e native && python scripts/smoke_rest.py --self-test
```

В PowerShell 7+:

```powershell
pio test -e native; if ($LASTEXITCODE -eq 0) { python scripts/smoke_rest.py --self-test }
```

### Ручная цепочка вместо обёртки

Эквивалент [`run_full_tests_live.ps1`](../scripts/run_full_tests_live.ps1) / [`run_full_tests_live.sh`](../scripts/run_full_tests_live.sh):

```bash
pio test -e native -e esp32-s3-devkitc-1-16m-test --upload-port COM25 --test-port COM25 && python scripts/smoke_rest.py http://192.168.1.42 --user admin --password admin --extended
```

Смоук по сети **не** входит в `pio test`. Флаг `--self-test` у `smoke_rest.py` — только локальный mock, без проверки прошивки по HTTP.

## Embedded-тесты (ESP32-S3)

### Сборка без загрузки и запуска

```bash
pio test -e esp32-s3-devkitc-1-16m-test --without-uploading --without-testing
```

### Полный запуск (устройство подключено по USB)

```bash
pio test -e esp32-s3-devkitc-1-16m-test
```

При необходимости укажите порт:

```bash
pio test -e esp32-s3-devkitc-1-16m-test --upload-port COM3 --test-port COM3
```

### После тестов в Serial «тишина»

`pio test` прошивает **отдельный тестовый** образ (Unity), он **заменяет** основную прошивку в разделе приложения. После выполнения тестов в коде обычно пустой `loop()`, поэтому привычные логи OpenConnect не выводятся — это не полное стирание флеша и не обязательно очистка NVS.

Чтобы снова получить рабочее приложение:

```bash
pio run -e esp32-s3-devkitc-1-16m -t upload
```

Используйте тот же `env`, что для повседневной разработки на этой плате (например `kincony-kc868-a6`), если он не `esp32-s3-devkitc-1-16m`.

### Если загрузка падает (Upload Error 2)

- **CH343/CH340:** тестовый env использует `upload_speed = 115200` для совместимости.
- **Подключение:** зажмите **BOOT**, нажмите **RESET**, отпустите **RESET**, затем **BOOT** и сразу запустите тест.
- **Порт занят:** закройте Serial Monitor и другие программы, использующие COM-порт.

### Если тест «висит» (Testing... If you don't see any output...)

Тестовый env наследует плату `esp32-s3-devkitc-1-n16r8v`, как [`env:esp32-s3-devkitc-1-16m`](../platformio.ini), и в [`platformio.ini`](../platformio.ini) для него заданы `-DARDUINO_USB_MODE=0` и `-DARDUINO_USB_CDC_ON_BOOT=0` (Serial → UART0 / CH343). Убедитесь, что `--test-port` указывает на порт CH343 (например COM25).

### Если сборка падает (No such file or directory, path errors)

Очистите кэш и пересоберите:
```bash
pio run -e esp32-s3-devkitc-1-16m-test -t clean
pio test -e esp32-s3-devkitc-1-16m-test --upload-port COM25 --test-port COM25
```

## Native-тесты (на ПК)

Требуется компилятор C++ (g++, MinGW на Windows).

**Важно для Windows (MSYS2):** G++ из MSYS2 UCRT64 должен быть **первым** в PATH. Перед запуском выполните:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cd D:\microcontrollers\esp32\OpenConnect\ssvc_open_connect
pio test -e native
```

Либо откройте **MSYS2 UCRT64** и выполните (если pio установлен глобально):
```bash
cd /d/microcontrollers/esp32/OpenConnect/ssvc_open_connect
pio test -e native
```

### Если native: `*** [test_main.o] Error 1` без сообщений компилятора

Часто PlatformIO вызывает **`g++` не из MSYS2** (из другого MinGW или вовсе не находит компилятор). В той же сессии PowerShell проверьте:

```powershell
where.exe g++
g++ --version
```

Ожидается путь вида `...\msys64\ucrt64\bin\g++.exe`. Если нет — задайте PATH **перед** `pio test`, как в блоке выше. Скрипт [`scripts/run_full_tests_live.ps1`](../scripts/run_full_tests_live.ps1) сам добавляет `C:\msys64\ucrt64\bin` в начало PATH (если каталог есть), чтобы совпадало с типичной сессией MSYS2 UCRT64.

### Если `where g++` уже указывает на ucrt64, но всё равно `Error 1` и нет текста ошибки

Значит падает не «не тот g++», а **внутренний компилятор** (`cc1plus.exe`) или его DLL (часто после частичного обновления MSYS2 или смешения каталогов MinGW в PATH). Быстрая проверка в PowerShell:

```powershell
g++ -print-prog-name=cc1plus
$p = (g++ -print-prog-name=cc1plus).Trim()
& $p --help 2>&1 | Select-Object -First 3
```

Если команда завершилась с ошибкой или пустым выводом, переустановите тулчейн в **MSYS2 UCRT64**, например:

```text
pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-gcc-libs
```

Не смешивайте в начале PATH **`mingw64\bin`** и **`ucrt64\bin`** — оставьте один префикс, соответствующий используемому `g++`.

Пока хостовые тесты чините отдельно, полный прогон на устройстве можно выполнить без native:

```powershell
.\scripts\run_full_tests_live.ps1 -Port COM25 -Url http://10.5.1.117 -SkipNative
```

### Если embedded env: `UnknownBoard: ...` для платы

Проверьте, что в [`platformio.ini`](../platformio.ini) тестовый `env:esp32-s3-devkitc-1-16m-test` ссылается на существующий `board` (тот же `esp32-s3-devkitc-1-n16r8v`, что и у `env:esp32-s3-devkitc-1-16m`) и что в `boards/` есть соответствующий JSON, либо используется встроенная плата из PlatformIO.

## Примеры тестов

| Тест | Описание |
|------|----------|
| `native/test_stringview` | StringView — конструкторы, `data()`, `size()`, `empty()` |
| `native/test_utils` | Вспомогательные функции (isNumericId, и т.п.) |
| `native/test_uart_api_spec` | SsvcUartApiSpec — лимиты и валидация по UART API v1.7 |
| `native/test_rectification_parse` | Разбор/утилиты ректификации (host) |
| `embedded/test_stringview_esp32` | StringView на ESP32 (Unity) |
| `embedded/test_uart_api` | UART API: лимиты, валидация, форматы JSON (GET_SETTINGS, VERSION, SET) |
| `scripts/smoke_rest.py` | HTTP-смоук: REST, телеметрия `/rest/telemetry`, расширенный режим `--extended` |
