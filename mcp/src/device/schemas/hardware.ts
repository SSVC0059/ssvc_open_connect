/**
 * Схемы аппаратной конфигурации и сбоев (WP5, mcp-server-agents.md §9;
 * контракт прошивки — mcp-contract-notes.md §B.10, §B.11, §C.5, §D.5).
 *
 * `/rest/oc/hardware-config` — это состояние прошивки целиком (`OpenConnectHardwareConfig::read`),
 * к которому обработчик добавляет `capabilities`, пробы I²C и `needsReboot`. Проблема в том,
 * что состав проб зависит от сборки: проба приходит только если функция включена, а пробы
 * реле — только при `PINOUT_USE_GPIO=0` (`OpenConnectHandler.cpp:329-403`). Поэтому ВСЕ поля,
 * кроме «тело — JSON-объект», необязательны, а `config` отдаётся наружу как есть:
 * домен не пересобирает настройки, чтобы не потерять новые ключи прошивки.
 *
 * `/rest/oc/hardware-faults`: `faults` может отсутствовать (нет сбоев) — это пустой массив,
 * а не ошибка; `i2cScan` может быть пустым, если на шине никто не ответил.
 */

import { z } from 'zod';

/**
 * `capabilities` аппаратной конфигурации (`OpenConnectHandler.cpp:317-326`):
 * `pinoutUseGpio` — GPIO-сборка (реле на PCF8574 недоступны), `relayOptionsAvailable` —
 * доступны ли настройки реле.
 */
export const hardwareCapabilitiesSchema = z
  .object({
    pinoutUseGpio: z.boolean().optional(),
    relayOptionsAvailable: z.boolean().optional(),
  })
  .passthrough();

export type HardwareCapabilitiesRaw = z.infer<typeof hardwareCapabilitiesSchema>;

/**
 * `GET|PUT /rest/oc/hardware-config`.
 *
 * Поля конфигурации перечислены для документации, но не обязательны: PUT принимает
 * частичный объект, а GET на другой прошивке может добавить новые ключи. `pendingReboot` —
 * сохранённый признак «настройки ждут перезагрузки», `needsReboot` — вычисленный
 * обработчиком (§B.10); наружу отдаём второй, с запасным первым.
 */
export const hardwareConfigSchema = z
  .object({
    schemaVersion: z.number().int().optional(),
    pressureSensorEnabled: z.boolean().optional(),
    bmp581I2cAddress: z.number().int().optional(),
    userRelayPcfEnabled: z.boolean().optional(),
    relayPcf8574Addresses: z.array(z.number().int()).optional(),
    pendingReboot: z.boolean().optional(),
    rtcEnabled: z.boolean().optional(),
    ds3231I2cAddress: z.number().int().optional(),
    oledEnabled: z.boolean().optional(),
    lcd1602Enabled: z.boolean().optional(),
    lcd1602I2cAddress: z.number().int().optional(),
    xkc25Sensors: z.array(z.record(z.unknown())).optional(),
    xkc25AutoPause: z.boolean().optional(),
    xkc25AbortRectification: z.boolean().optional(),
    xkc25TriggerAction: z.number().int().optional(),
    xkc25SharedTankIndex: z.number().int().optional(),
    xkc25StartGuardMode: z.number().int().optional(),
    bmp581ProbeOk: z.boolean().optional(),
    ds3231ProbeOk: z.boolean().optional(),
    lcd1602ProbeOk: z.boolean().optional(),
    relayPcf8574ProbeOk: z.array(z.boolean()).optional(),
    relayHardwareOk: z.boolean().optional(),
    capabilities: hardwareCapabilitiesSchema.optional(),
    needsReboot: z.boolean().optional(),
  })
  .passthrough();

export type HardwareConfigRaw = z.infer<typeof hardwareConfigSchema>;

/**
 * Один аппаратный сбой (`OpenConnectHandler.cpp:39-98`). `address` — 7-битный адрес
 * I²C (0 для `ssvc_uart`), `role` — роль устройства на шине.
 */
export const hardwareFaultSchema = z
  .object({
    code: z.string(),
    codeLabel: z.string().optional(),
    address: z.number().int().optional(),
    role: z.string().optional(),
  })
  .passthrough();

export type HardwareFaultRaw = z.infer<typeof hardwareFaultSchema>;

/** `GET /rest/oc/hardware-faults`: все поля, кроме `faults[]`, тоже могут отсутствовать. */
export const hardwareFaultsSchema = z
  .object({
    faults: z.array(hardwareFaultSchema).optional(),
    count: z.number().int().nonnegative().optional(),
    i2cBusActive: z.boolean().optional(),
    i2cScan: z.array(z.number().int()).optional(),
  })
  .passthrough();

export type HardwareFaultsRaw = z.infer<typeof hardwareFaultsSchema>;
