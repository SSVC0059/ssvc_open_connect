/**
 * Схемы датчиков, зон и порогов тревог (WP4, mcp-server-agents.md §8;
 * контракт прошивки — mcp-contract-notes.md §B.2–§B.4, §B.7, §C.10, §D.4, §F.7–§F.8).
 *
 * Три ответа устроены по-разному, и путать их нельзя:
 *  - `GET /rest/sensor` — карта БЕЗ обёртки: `{ "<zone>": { "<ADDRESS>": {v,u,type} } }`;
 *    пустой ответ — `{}`, это «датчиков нет», а не ошибка;
 *  - `GET /rest/zones`  — обёртка `{"zones": {"<ADDRESS>": <zoneId|"zoneName">}}`;
 *    прошивка этой версии пишет числовой `zoneId`, но принимает и строку (§B.3);
 *  - `GET /rest/alarms` — обёртка `{"thresholds": {"<ADDRESS>": {...}}}`.
 *
 * `thresholds` описываем как `z.record(z.record(z.unknown()))`: набор полей
 * (`enabled/min/dangerous/critical`) может расшириться, а частичный набор — валидный
 * случай (недостающие поля прошивка берёт из значений по умолчанию, §B.4). Поэтому
 * пороги возвращаются наружу как есть, без отбрасывания неизвестных полей.
 */

import { z } from 'zod';

/**
 * Показание одного датчика. `v` бывает числом и строкой (см. WP1, `normalizeScalar`),
 * `u`/`type` — пустые строки, если датчик не найден (§B.2).
 */
export const sensorReadingSchema = z
  .object({
    /** Значение: температура, давление или уровень. */
    v: z.union([z.number(), z.string()]).optional(),
    /** Единицы: `°C`, `мм рт. ст.`, у XKC-25 — пустая строка. */
    u: z.string().optional(),
    /** Тип датчика: thermal | pressure | level | undefined. */
    type: z.string().optional(),
  })
  .passthrough();

/** `GET /rest/sensor`: зона → адрес (16 hex) → показание. Пустая карта — валидный ответ. */
export const sensorMapSchema = z.record(z.record(sensorReadingSchema));

export type SensorReadingRaw = z.infer<typeof sensorReadingSchema>;
export type SensorMapRaw = z.infer<typeof sensorMapSchema>;

/**
 * Значение привязки в `/rest/zones`: числовой id зоны (так отдаёт эта прошивка)
 * или её строковое имя (форма, которую прошивка принимает на запись, §B.3).
 */
export const zoneValueSchema = z.union([z.string(), z.number()]);

export type ZoneValue = z.infer<typeof zoneValueSchema>;

/** `GET /rest/zones`: обёртка `zones` обязательна, пустая карта валидна. */
export const zonesEnvelopeSchema = z
  .object({
    zones: z.record(zoneValueSchema),
  })
  .passthrough();

export type ZonesEnvelope = z.infer<typeof zonesEnvelopeSchema>;

/**
 * Один порог тревоги. Поля не обязательны: инструмент принимает частичный набор
 * (например, только `min`), а наружу отдаёт то, что реально лежит в прошивке.
 */
export const alarmThresholdEntrySchema = z.record(z.unknown());

export type AlarmThresholdEntry = z.infer<typeof alarmThresholdEntrySchema>;

/** `GET /rest/alarms`: обёртка `thresholds` обязательна (§B.4). */
export const alarmsEnvelopeSchema = z
  .object({
    thresholds: z.record(z.record(z.unknown())),
  })
  .passthrough();

export type AlarmsEnvelope = z.infer<typeof alarmsEnvelopeSchema>;

/** Карта «адрес датчика → пороги» в том виде, в каком её отдаёт прошивка. */
export type AlarmThresholds = AlarmsEnvelope['thresholds'];

/**
 * Канонические имена зон по числовым кодам (`SensorZone.h:28-40`, §C.10).
 * `deleted` в `/rest/sensor` не встречается, но в `/rest/zones` может.
 */
export const ZONE_NAMES_BY_ID: Readonly<Record<number, string>> = {
  0: 'unknown',
  1: 'inlet_water',
  2: 'outlet_water',
  3: 'act',
  4: 'tank',
  5: 'outdoor',
  6: 'deleted',
  7: 'receiver_tank_1',
  8: 'receiver_tank_2',
  9: 'receiver_tank_3',
};

/**
 * Имена зон для подсказки в описании аргумента `zone`.
 * Список не используется для жёсткой валидации: новая прошивка может добавить зону,
 * а неизвестное имя прошивка всё равно молча превращает в `unknown` — фактическое
 * значение после записи покажет сама карта зон.
 */
export const KNOWN_ZONE_NAMES: readonly string[] = Object.values(ZONE_NAMES_BY_ID);
