/**
 * Схемы датчиков уровня XKC-25 (WP6, mcp-server-agents.md §10;
 * контракт прошивки — mcp-contract-notes.md §B.12, §C.7, §D.5, §F.9).
 *
 * Три ответа устроены по-разному, и путать их нельзя:
 *  - `GET /rest/oc/xkc25/status` — снимок слотов конфигурации и предстартовой проверки;
 *  - `POST /rest/oc/xkc25/calibrate` — либо итог шага (`ok/completed/code/...`), либо
 *    `202 {status:"pending"}` для асинхронного `verify_return`;
 *  - `GET /rest/oc/xkc25/verify-status` — `none` | `pending` | `done` с результатом.
 *
 * Поля слота в §F.9 перечислены по коду `appendXkc25SensorStatus`
 * (`OpenConnectHandler.cpp:621-651`): часть из них появляется не всегда (например,
 * `pcfAddress`/`pcfBit` осмысленны только для PCF-источника), поэтому обязателен
 * только `index`, остальное — `.optional()` и `.passthrough()`: новая прошивка может
 * добавить поля, и это не должно превращаться в `device_error`.
 */

import { z } from 'zod';

/** Шаги мастера калибровки (`OpenConnectHandler.cpp:798-823`). */
export const XKC25_STEPS = ['without_liquid', 'with_liquid', 'verify_return'] as const;

export type Xkc25Step = (typeof XKC25_STEPS)[number];

/**
 * Один слот датчика. `rawLevel = -1` — живой подсистемы нет (`OpenConnectHandler.cpp:649`),
 * `filled` в этом случае тоже `false`.
 */
export const xkc25SensorSchema = z
  .object({
    /** 0-based слот конфигурации (0..3 в контракте MCP-сервера). */
    index: z.number().int(),
    enabled: z.boolean().optional(),
    /** 0 — GPIO, 1 — PCF8574. */
    pinSource: z.number().int().optional(),
    gpio: z.number().int().optional(),
    pcfAddress: z.number().int().optional(),
    pcfBit: z.number().int().optional(),
    /** Подпись пина: «GPIO 4» или подпись линии PCF8574. */
    pinLabel: z.string().optional(),
    zoneIndex: z.number().int().optional(),
    /** Зона: receiver_tank_1…3 и другие имена `SensorZone`. */
    zone: z.string().optional(),
    zoneLabel: z.string().optional(),
    sharedAcrossTanks: z.boolean().optional(),
    activeLow: z.boolean().optional(),
    calibrated: z.boolean().optional(),
    /** Время калибровки в формате прошивки (не epoch MCP-сервера). */
    calibratedAt: z.number().int().optional(),
    levelWithoutLiquid: z.number().int().optional(),
    levelWithLiquid: z.number().int().optional(),
    /** Датчик поднят подсистемой и опрашивается. */
    live: z.boolean().optional(),
    /** Сырой уровень пина; -1 — чтение не выполнялось. */
    rawLevel: z.number().int().optional(),
    filled: z.boolean().optional(),
  })
  .passthrough();

export type Xkc25SensorRaw = z.infer<typeof xkc25SensorSchema>;

/**
 * `GET /rest/oc/xkc25/status`.
 *
 * `sensors` обязателен, потому что это единственное поле, на котором держится
 * инструмент; остальные — данные предстартовой проверки, и их отсутствие не ломает
 * ответ. `blockStart` выводится из `checkStart()`, а не из конфигурации.
 */
export const xkc25StatusSchema = z
  .object({
    sensors: z.array(xkc25SensorSchema),
    triggerAction: z.number().int().optional(),
    sharedTankIndex: z.number().int().optional(),
    startGuardMode: z.number().int().optional(),
    blockStart: z.boolean().optional(),
    blockReasons: z.array(z.string()).optional(),
    warnings: z.array(z.string()).optional(),
  })
  .passthrough();

export type Xkc25StatusRaw = z.infer<typeof xkc25StatusSchema>;

/**
 * Итог шага калибровки (`buildXkc25CalibrationBody`, `OpenConnectHandler.cpp:676-704`).
 *
 * `ok` — «шаг выполнен», `completed` — «калибровка целиком завершена». Код `verify_failed`
 * приходит с HTTP 200: шаг выполнен, но обратная проверка не прошла, поэтому это тело
 * шага, а не ошибка транспорта.
 */
export const xkc25CalibrationSchema = z
  .object({
    ok: z.boolean(),
    completed: z.boolean().optional(),
    code: z.string().optional(),
    rawLevel: z.number().int().optional(),
    message: z.string().optional(),
    /** Чек-лист подключения при отклонённой калибровке — строки. */
    checklist: z.array(z.string()).optional(),
    sensor: xkc25SensorSchema.optional(),
  })
  .passthrough();

export type Xkc25CalibrationRaw = z.infer<typeof xkc25CalibrationSchema>;

/**
 * Ответ `202` на постановку асинхронной проверки возврата
 * (`buildXkc25VerifyPendingBody`, `OpenConnectHandler.cpp:707-717`).
 */
export const xkc25VerifyPendingSchema = z
  .object({
    status: z.string(),
    index: z.number().int().optional(),
    code: z.string().optional(),
    message: z.string().optional(),
  })
  .passthrough();

export type Xkc25VerifyPendingRaw = z.infer<typeof xkc25VerifyPendingSchema>;

/**
 * `GET /rest/oc/xkc25/verify-status` (`OpenConnectHandler.cpp:829-857`).
 *
 * `none` — проверку ещё не запускали (или подсистема не активна, тогда есть `code`),
 * `pending` — идёт, `done` — есть результат с `ok/completed/code/rawLevel/message`.
 */
export const xkc25VerifyStatusSchema = z
  .object({
    status: z.string(),
    index: z.number().int().optional(),
    ok: z.boolean().optional(),
    completed: z.boolean().optional(),
    code: z.string().optional(),
    rawLevel: z.number().int().optional(),
    message: z.string().optional(),
  })
  .passthrough();

export type Xkc25VerifyStatusRaw = z.infer<typeof xkc25VerifyStatusSchema>;
