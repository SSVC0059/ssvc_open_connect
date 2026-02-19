# Unit Tests — SSVC OpenConnect

## Структура тестов

- **`native/`** — тесты, выполняемые на ПК (требуют g++/MinGW)
- **`embedded/`** — тесты, выполняемые на ESP32-S3

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

### Если загрузка падает (Upload Error 2)

- **CH343/CH340:** тестовый env использует `upload_speed = 115200` для совместимости.
- **Подключение:** зажмите **BOOT**, нажмите **RESET**, отпустите **RESET**, затем **BOOT** и сразу запустите тест.
- **Порт занят:** закройте Serial Monitor и другие программы, использующие COM-порт.

### Если тест «висит» (Testing... If you don't see any output...)

Тестовый env использует плату `esp32-s3-devkitc-1-n16r8v-uart` (Serial → UART). Убедитесь, что `--test-port` указывает на порт CH343 (COM25).

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

## Примеры тестов

| Тест | Описание |
|------|----------|
| `native/test_stringview` | StringView — конструкторы, `data()`, `size()`, `empty()` |
| `native/test_utils` | Вспомогательные функции (isNumericId, и т.п.) |
| `native/test_uart_api_spec` | SsvcUartApiSpec — лимиты и валидация по UART API v1.7 |
| `embedded/test_stringview_esp32` | StringView на ESP32 (Unity) |
| `embedded/test_uart_api` | UART API: лимиты, валидация, форматы JSON (GET_SETTINGS, VERSION, SET) |
