/**
 * Схемы `/rest/settings` (WP3, mcp-server-agents.md §7; контракт прошивки —
 * mcp-contract-notes.md §B.5, §D.2, фикстуры §F.4–F.6).
 *
 * Две формы, которые нельзя путать (mcp-server.md §2.5 п.5):
 *  - GET отдаёт ВЛОЖЕННЫЙ объект `{ "ssvcSettings": { ... } }`;
 *  - PUT принимает ПЛОСКИЙ объект `{ "hyst": 0.5 }` — без обёртки.
 *
 * Обёртку разворачивает домен; наружу уходит только содержимое `ssvcSettings`.
 * Схемы терпимы к набору ключей: прошивка добавляет/убирает параметры в зависимости
 * от режима и версии UART API (`tails`/`parallel*` могут отсутствовать), поэтому
 * содержимое настроек — `z.record(z.unknown())`, а не закрытый объект.
 */

import { z } from 'zod';

/** Ответ `GET /rest/settings`: обёртка обязательна, остальные поля — passthrough. */
export const settingsEnvelopeSchema = z
  .object({
    /** Полный набор действующих настроек SSVC. */
    ssvcSettings: z.record(z.unknown()),
  })
  .passthrough();

export type SettingsEnvelope = z.infer<typeof settingsEnvelopeSchema>;

/** Настройки SSVC как «ключ → значение»: набор ключей зависит от режима прошивки. */
export type SsvcSettings = Record<string, unknown>;

/**
 * Ответ `PUT /rest/settings` — успех (200) и частичное применение (503 `partial:true`).
 *
 * Поля необязательны, потому что прошивка опускает пустые `ignored`/`skipped`, а старые
 * версии могут не присылать `accepted`. `ignored`/`skipped`/`failed` описаны как
 * `z.array(z.unknown())` и нормализуются в строки в домене: элементы — имена ключей
 * (`"sound"`) или пары (`"decrement=3"`), но схема не должна падать на форме элемента.
 */
export const settingsWriteResponseSchema = z
  .object({
    success: z.boolean().optional(),
    message: z.string().optional(),
    /** Сколько SET-параметров ушло в очередь контроллера. */
    accepted: z.number().int().nonnegative().optional(),
    /** Ключи, которых прошивка не знает: это не ошибка, запрос не отменяется. */
    ignored: z.array(z.unknown()).optional(),
    /** Параметры, отброшенные по версии UART API (`SsvcApiCapabilities`). */
    skipped: z.array(z.unknown()).optional(),
    /** При `partial:true` — что именно не попало в очередь (например `decrement=3`). */
    failed: z.array(z.unknown()).optional(),
    /** Сколько чанков SET не ушло (503, частичное применение). */
    failedChunks: z.number().int().nonnegative().optional(),
    /** 503: часть SET уже отправлена — повторять весь запрос нельзя. */
    partial: z.boolean().optional(),
    error: z.union([z.string(), z.boolean()]).optional(),
  })
  .passthrough();

export type SettingsWriteResponseBody = z.infer<typeof settingsWriteResponseSchema>;

/**
 * Ключи записи из `PARAM_HANDLERS` прошивки — только подсказка в описании инструмента.
 *
 * Валидировать по этому списку НЕЛЬЗЯ: прошивка сама отбрасывает неизвестные ключи
 * в `ignored`, а новая версия может добавить параметр. Список нужен агенту, чтобы
 * не изобретать имена; источник — mcp-contract-notes.md §C.3.
 */
export const KNOWN_SETTING_KEYS: readonly string[] = [
  'heads',
  'hearts',
  'late_heads',
  'tails',
  'hyst',
  'decrement',
  'tank_mmhg',
  'tank_mmhg_act',
  'heads_timer',
  'late_heads_timer',
  'hearts_timer',
  'tails_temp',
  'start_delay',
  'hearts_finish_temp',
  'formula_start_temp',
  'valve_bw',
  'release_speed',
  'release_timer',
  'heads_final',
  'parallel',
  'parallel_v1',
  'parallel_v3',
  's_temp',
  's_hyst',
  's_speed',
  's_decrement',
  's_timer',
  'formula',
  'predec',
  'extended_heads',
];

/**
 * Ключи, которые `GET /rest/settings` отдаёт, но `PUT` молча игнорирует:
 * их нет в `PARAM_HANDLERS` прошивки (mcp-contract-notes.md §C.3).
 *
 * Знание не выводится из устройства: `ignored` в ответе PUT приходит без причины,
 * поэтому список зафиксирован здесь и используется в `ignored_reasons`
 * (`oc_update_settings`) и в `settings.readonly` (`oc_get_capabilities`).
 */
export const READ_ONLY_SETTING_KEYS: readonly string[] = [
  'sound',
  'pressure',
  'relay_inverted',
  'relay_autostart',
  'autoresume',
  'auto_mode',
  'hearts_temp_shift',
  'hearts_pause',
  'tp2_shift',
  'tp_filter',
  'signal_tp1_control',
  'signal_inverted',
  'tp1_control_temp',
  'tp1_control_start',
  'stab_limit_time',
  'stab_limit_finish',
  'backlight',
];

/** Ключи, которые прошивка принимает в `PUT /rest/settings` (подсказка и `settings.writable`). */
export const WRITABLE_SETTING_KEYS: readonly string[] = KNOWN_SETTING_KEYS.filter(
  (key) => !READ_ONLY_SETTING_KEYS.includes(key),
);
