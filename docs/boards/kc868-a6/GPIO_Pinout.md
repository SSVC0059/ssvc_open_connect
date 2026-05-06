# GPIO Pinout — KC868-A6 (ревизия v6 / A6v3.x по разводке KinCony)

Актуальная таблица для текущей платы в прошивке `kincony-kc868-a6`. Старые сводные таблицы из вики могут отличаться.

| Сигнал | GPIO | Примечание |
| ------ | ---- | ---------- |
| ANALOG_A1 | 4 | АЦП |
| ANALOG_A2 | 5 | АЦП |
| ANALOG_A3 | 6 | АЦП |
| ANALOG_A4 | 7 | АЦП |
| 1-Wire 1 (подтяжка на плате) | 15 | DS18B20 и др. |
| 1-Wire 2 (подтяжка на плате) | 16 | второй канал 1-Wire |
| I2C SDA | 12 | общая шина (PCF8574, дисплей, GP8403 и др.) |
| I2C SCL | 11 | |
| Входы PCF8574 (I2C) | адрес **0x22** | сухие контакты |
| Реле PCF8574 (I2C) | адрес **0x24** | выходы реле |
| GP8403 DAC (I2C) | **0x58** | |
| RS485 RXD | **18** | TTL к трансиверу (SSVC / Modbus) |
| RS485 TXD | **17** | |
| RS232 TXD | 9 | SP3232 |
| RS232 RXD | 10 | |
| SPI (LoRa SX1278 / nRF24) CS | 38 | |
| SPI MOSI | 43 | |
| SPI MISO | 44 | |
| SPI SCK | 39 | |
| LoRa SX1278 RST | 42 | |
| LoRa DIO0 | 41 | |
| nRF24L01 CE | 40 | |
| Свободные GPIO (у модуля ESP32-S3) | **8**, **13**, **14**, **21**, **47**, **48** | не пересекать с I2C/UART/SPI |

## SSVC по RS485

На стороне **SSVC** — UART TTL и внешний преобразователь **RS485↔TTL**; на стороне платы — трансивер с автонаправлением. В прошивке — обычный асинхронный UART на **GPIO17 (TX)** / **GPIO18 (RX)**, без `uart_set_mode(RS485)` и без DE/RE на MCU.

Параметры задаются в `platformio.ini` (`SSVC_OPEN_CONNECT_UART_TX` / `RX`).

## Алерты и реле (PinOut / AlarmMonitor)

При `PINOUT_USE_GPIO=0` уровни **DANGEROUS** / **CRITICAL** идут на выходы через **PCF8574** на I2C: адрес и биты задаются макросами `SSVC_RELAY_PCF8574_*` в сборке (на этой плате по умолчанию адрес **0x24**, биты **0** и **1**, active-LOW на линии). Другая разводка или второй расширитель — другие значения макросов / второй порт в коде. Шина — через `I2CBusSubsystem`; при пустой шине или ошибке записи — `AlarmMonitor::raiseHardwareFault`. Подробнее: [I2C_devices.md](I2C_devices.md).

При `PINOUT_USE_GPIO=1` используются только **GPIO** (`SSVC_PINOUT_ALARM_GPIO_DANGEROUS` / `CRITICAL` в `SsvcConnector.h` или `-D` в env); этот путь **не** использует PCF8574 и задаётся отдельно от I2C-реле.

## WLED (полосы)

Пин статусного NeoPixel задаётся в коде через `DEFAULT_STATUS_LED_PIN` (см. `lib/ssvcOpenConnect/components/Led/StatusLed.h`).

## NeoPixel статус (Open Connect)

`StatusLed` по умолчанию: **GPIO48** (см. `StatusLed.h`).


## Схема (PDF в репозитории)

[Скачать KC868-A6-schematic.pdf](KC868-A6-schematic.pdf) — может относиться к другой ревизии; ориентир для кода — таблица выше и [форум KinCony tid=7622](https://www.kincony.com/forum/showthread.php?tid=7622).

![Схема KC868-A6](KC868-A6-schematic.pdf){ type=application/pdf style="min-height:75vh;width:100%;border:0" }
