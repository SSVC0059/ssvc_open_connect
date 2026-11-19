/**
 * Схемы `/rest/subsystem`, `/rest/systemStatus`, `/rest/restart` (WP9,
 * mcp-server-agents.md §13; контракт прошивки — mcp-contract-notes.md §B.8,
 * §B.18, §D, §G.9, §G.11).
 *
 * Важные расхождения с §13, которые учтены здесь:
 *  - `GET /rest/systemStatus` отдаёт **не** `{heap, firmware}`, а `free_heap`,
 *    `firmware_version`, `cpu_type` и ещё два десятка полей (`SystemStatus.cpp`).
 *    Старые имена принимаются как совместимость, но наружу уходят канонические;
 *  - набор подсистем зависит от сборки: `atm_sensor` есть только при включённом
 *    датчике давления, `lcd1602_display`/`xkc25_sensor` — только при своём железе.
 *    Поэтому карта — «словарь bool с произвольными ключами», а не закрытый объект;
 *  - `PUT /rest/subsystem` отвечает `{success, errors[]}`, где `errors` — массив
 *    объектов `{subsystem, message}`; не-bool значение на устройстве трактуется
 *    как `false`, поэтому `enabled` в инструменте обязателен и строго boolean.
 */

import { z } from 'zod';

/** `GET /rest/subsystem` — плоская карта «имя → включена». */
export const subsystemMapSchema = z.record(z.boolean());

export type SubsystemMap = z.infer<typeof subsystemMapSchema>;

/**
 * Запись в `errors[]` ответа `PUT /rest/subsystem`.
 *
 * Прошивка складывает туда `{subsystem, message}`, но текст может прийти и строкой;
 * оба варианта нормализуются в домене, чтобы инструмент всегда отдавал объекты.
 */
export const subsystemErrorEntrySchema = z.union([
  z.string(),
  z
    .object({
      subsystem: z.string().optional(),
      message: z.string().optional(),
    })
    .passthrough(),
]);

export type SubsystemErrorEntry = z.infer<typeof subsystemErrorEntrySchema>;

/** `PUT /rest/subsystem` → `{success, errors[]}`. */
export const subsystemUpdateResponseSchema = z
  .object({
    success: z.boolean().optional(),
    errors: z.array(subsystemErrorEntrySchema).optional(),
  })
  .passthrough();

export type SubsystemUpdateResponseBody = z.infer<typeof subsystemUpdateResponseSchema>;

/**
 * `GET /rest/systemStatus`.
 *
 * Все поля необязательны: PSRAM отдаётся только при наличии чипа, `used_psram` —
 * вместе с `psram_size`, а `core_temp` на некоторых ревизиях может быть `null`.
 * `passthrough` оставляет неизвестные поля прошивки, но наружу они не уходят:
 * домен отдаёт только нормализованный набор.
 *
 * `heap`/`firmware` — имена из §13 (в прошивке их нет); принимаются как fallback,
 * чтобы инструмент не отдавал `null` на старом контракте.
 */
export const systemStatusBodySchema = z
  .object({
    esp_platform: z.string().optional(),
    firmware_version: z.string().optional(),
    firmware: z.string().optional(),
    max_alloc_heap: z.number().optional(),
    free_psram: z.number().optional(),
    used_psram: z.number().optional(),
    psram_size: z.number().optional(),
    cpu_freq_mhz: z.number().optional(),
    cpu_type: z.string().optional(),
    cpu_rev: z.number().optional(),
    cpu_cores: z.number().optional(),
    heap: z.number().optional(),
    free_heap: z.number().optional(),
    used_heap: z.number().optional(),
    total_heap: z.number().optional(),
    min_free_heap: z.number().optional(),
    sketch_size: z.number().optional(),
    free_sketch_space: z.number().optional(),
    sdk_version: z.string().optional(),
    arduino_version: z.string().optional(),
    flash_chip_size: z.number().optional(),
    flash_chip_speed: z.number().optional(),
    fs_total: z.number().optional(),
    fs_used: z.number().optional(),
    core_temp: z.number().nullable().optional(),
    cpu_reset_reason: z.string().optional(),
    uptime: z.number().optional(),
  })
  .passthrough();

export type SystemStatusBody = z.infer<typeof systemStatusBodySchema>;

/** Нормализованное состояние системы: то, что видит агент. */
export interface SystemInfo {
  /** Свободная куча, байт (каноническое имя прошивки). */
  free_heap: number | null;
  /** Псевдоним `free_heap` из §13: агенты ищут и «heap». */
  heap: number | null;
  min_free_heap: number | null;
  used_heap: number | null;
  total_heap: number | null;
  max_alloc_heap: number | null;
  free_psram: number | null;
  used_psram: number | null;
  psram_size: number | null;
  /** Uptime устройства в секундах (`millis()/1000`, SystemStatus.cpp:163). */
  uptime: number | null;
  firmware_version: string | null;
  /** Псевдоним `firmware_version` из §13. */
  firmware: string | null;
  esp_platform: string | null;
  cpu_type: string | null;
  /** Псевдоним `cpu_type` из §13. */
  chip: string | null;
  cpu_freq_mhz: number | null;
  cpu_rev: number | null;
  cpu_cores: number | null;
  flash_chip_size: number | null;
  flash_chip_speed: number | null;
  sketch_size: number | null;
  free_sketch_space: number | null;
  fs_total: number | null;
  fs_used: number | null;
  core_temp: number | null;
  sdk_version: string | null;
  arduino_version: string | null;
  cpu_reset_reason: string | null;
}
