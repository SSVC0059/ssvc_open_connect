/**
 * Схемы реле на PCF8574 (WP5, mcp-server-agents.md §9;
 * контракт прошивки — mcp-contract-notes.md §B.13, §C.6, §D.5).
 *
 * Три ответа `/rest/oc/relay/*` объединены флагом `supported`:
 *  - на GPIO-сборке все три GET отвечают `{"supported": false}` с HTTP 200 —
 *    это не сбой устройства, а «функция недоступна в этой прошивке» (§2.5 п.9);
 *  - на сборке с PCF8574 `metadata` отдаёт `relays[]` (index 1-based, `targetBit`
 *    0-based) и `relayLineCount` — по нему домен проверяет `bit` ЛОКАЛЬНО;
 *  - `state` отдаёт `userShadow`/`userShadows` — активный НИЗКИЙ уровень
 *    (`RelayPortCoordinator.cpp:139-145`): включённое реле = бит 0.
 *
 * Обязательным делаем только `supported`: остальные поля появляются лишь в
 * поддерживающей сборке, а `.passthrough()` не даёт новой прошивке сломать разбор.
 */

import { z } from 'zod';

/** `GET /rest/oc/relay/capabilities` — `pcf8574` есть только при `supported:true`. */
export const relayCapabilitiesSchema = z
  .object({
    supported: z.boolean(),
    /** `linesPerChip`, адреса, зарезервированные биты аварий; форма §B.13. */
    pcf8574: z.record(z.unknown()).optional(),
  })
  .passthrough();

export type RelayCapabilitiesRaw = z.infer<typeof relayCapabilitiesSchema>;

/**
 * Одно реле в `metadata`.
 *
 * `index` — 1-based номер для человека (`displayIndex`), `targetBit` — 0-based
 * глобальный бит, который принимает `override` (`OpenConnectHandler.cpp:172-187`).
 * `editable:false` — системное реле AlarmManager (биты 0 и 1 первого чипа).
 */
export const relayEntrySchema = z
  .object({
    index: z.number().int(),
    label: z.string(),
    editable: z.boolean(),
    targetBit: z.number().int().nonnegative(),
    /** Причина запрета ручного управления; прошивка даёт её только для `editable:false`. */
    reason: z.string().optional(),
  })
  .passthrough();

export type RelayEntryRaw = z.infer<typeof relayEntrySchema>;

/** `GET /rest/oc/relay/metadata` — `relays[]` и счётчики только при `supported:true`. */
export const relayMetadataSchema = z
  .object({
    supported: z.boolean(),
    relays: z.array(relayEntrySchema).optional(),
    relayChipCount: z.number().int().nonnegative().optional(),
    relayLineCount: z.number().int().nonnegative().optional(),
  })
  .passthrough();

export type RelayMetadataRaw = z.infer<typeof relayMetadataSchema>;

/**
 * `GET /rest/oc/relay/state`: `userShadow` — маска первого чипа, `userShadows` —
 * по чипу. Значения — байты теневого регистра пользователя (активный низкий уровень).
 */
export const relayStateSchema = z
  .object({
    supported: z.boolean(),
    userShadow: z.number().int().nonnegative().optional(),
    userShadows: z.array(z.number().int().nonnegative()).optional(),
  })
  .passthrough();

export type RelayStateRaw = z.infer<typeof relayStateSchema>;

/** `POST /rest/oc/relay/override` → `{"ok": true}` (§B.13). */
export const relayOverrideResponseSchema = z
  .object({
    ok: z.boolean(),
  })
  .passthrough();

export type RelayOverrideResponse = z.infer<typeof relayOverrideResponseSchema>;
