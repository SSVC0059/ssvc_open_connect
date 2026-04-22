# I2C bus — KC868-A6 (v6)

Общая шина: **SDA** / **SCL** задаются в сборке (`SSVC_I2C_SDA_GPIO` / `SSVC_I2C_SCL_GPIO`), инициализация — [`I2CBusSubsystem`](../../../lib/ssvcOpenConnect/components/subsystem/I2CBusSubsystem.h) (`Wire` через `getWire()` после `enable`).

## Зарезервированные адреса (пример разводки платы)

| Устройство        | Адрес | Примечание |
|-------------------|-------|------------|
| Входы (сухие контакты) | 0x22 | опционально |
| Реле / выходы (PCF8574) | по умолчанию **0x24** | драйвер [`Pcf8574RelayPort`](../../../lib/ssvcOpenConnect/components/io/Pcf8574RelayPort.h); роль для алармов задаётся макросом `SSVC_RELAY_PCF8574_DEVICE_ROLE` (по умолчанию `pcf8574_port0`). Дополнительные микросхемы — отдельный адрес и роль. |
| GP8403 DAC        | 0x58  | план |
| Дисплей / MQ / др.| —     | документировать при добавлении |

## Интеграция нового I2C-устройства

1. Не вызывать `Wire.begin` в драйвере — использовать `I2CBusSubsystem::getWire()` и проверять `isActive()`.
2. При критичных сбоях (NACK, пропала шина) — `AlarmMonitor::raiseHardwareFault(code, addr, "stable_role_string")`; при восстановлении — `clearHardwareFault`.
3. Несколько PCF8574: для каждого чипа — свой экземпляр `Pcf8574RelayPort(i2cAddr, role)` и свои макросы/конфиг в `platformio.ini`.

См. также [GPIO_Pinout.md](GPIO_Pinout.md).

**Полное описание мониторинга шины, кодов сбоев и подписки на алерты:** [I2C: мониторинг и алерты (прошивка)](../develop/i2c_monitoring.md).
