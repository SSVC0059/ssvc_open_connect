/**
 * Схемы снимка телеметрии: `GET /rest/telemetry` (контракт §5, mcp-server.md §2.6).
 *
 * Ответ:
 * {
 *   "telemetry": {"type":"heads","tank_mmhg":12.5,"common":{...},"stop":false,"stops":0,...},
 *   "status": {"stage":"heads","status":"running","startTime":"...","endTime":"",
 *              "stages":{...},"uartConnectionError":false},
 *   "lastUpdate": 123456
 * }
 *
 * `telemetry` — это JSON, который прислал SSVC по UART: набор полей зависит от версии
 * API и стадии процесса. Поэтому внутри — `.passthrough()`, а обязательными остаются
 * только контейнеры `telemetry` и `status`: без них снимок бессмысленен.
 *
 * `lastUpdate` прошивка заполняет `millis()` (время работы контроллера), а не epoch:
 * сравнивать его с часами сервера нельзя. Возраст снимка считается от uptime
 * устройства (`GET /rest/systemStatus`), см. `snapshotAgeMs`.
 */

import { z } from 'zod';

export const telemetryValuesSchema = z
  .object({
    /** Тип фракции/стадии: heads, late_heads, hearts, tails, … */
    type: z.string().optional(),
    /** Номер процесса. */
    pid: z.number().optional(),
    /** Давление в кубе, мм рт. ст. */
    tank_mmhg: z.number().optional(),
    /** Признак остановки процесса. */
    stop: z.boolean().optional(),
    /** Количество остановок. */
    stops: z.number().optional(),
    /** Общие поля SSVC (mmhg, tp1, tp2, relay, signal, …) — структура версионная. */
    common: z.record(z.unknown()).optional(),
    /** Объёмы по фракциям. */
    volume: z.record(z.unknown()).optional(),
    /** Крепость, %. */
    alc: z.number().optional(),
    /** Декремент. */
    decrement: z.number().optional(),
    /** Служебные текстовые поля SSVC. */
    event: z.string().optional(),
    info: z.string().optional(),
  })
  .passthrough();

export const rectStatusSchema = z
  .object({
    /** Стадия процесса: heads, late_heads, hearts, tails, … */
    stage: z.string().optional(),
    /** Статус: running, paused, stopped, … */
    status: z.string().optional(),
    /** Время начала процесса (строка прошивки). */
    startTime: z.string().optional(),
    /** Время окончания процесса. */
    endTime: z.string().optional(),
    /** Длительности стадий. */
    stages: z.record(z.string()).optional(),
    /** Ошибка связи с SSVC по UART: процесс идёт, а данных нет. */
    uartConnectionError: z.boolean().optional(),
  })
  .passthrough();

export const telemetrySnapshotSchema = z
  .object({
    telemetry: telemetryValuesSchema,
    status: rectStatusSchema,
    /** millis() контроллера на момент обновления снимка (не epoch!). */
    lastUpdate: z.number().optional(),
  })
  .passthrough();

/** Ответ GET /rest/sensor: `{<zone>:{<address>:{v,u,type}}}`. */
export const sensorReadingSchema = z
  .object({
    /** Значение (температура и т.п.). */
    v: z.number().optional(),
    /** Единицы измерения. */
    u: z.string().optional(),
    /** Тип датчика: DS18B20, … */
    type: z.string().optional(),
  })
  .passthrough();

export const sensorMapSchema = z.record(z.record(sensorReadingSchema));

export type TelemetryValues = z.infer<typeof telemetryValuesSchema>;
export type RectStatus = z.infer<typeof rectStatusSchema>;
export type TelemetrySnapshot = z.infer<typeof telemetrySnapshotSchema>;
export type SensorMap = z.infer<typeof sensorMapSchema>;

/** Через сколько снимок считается устаревшим (контракт §4 WP1: 10 с). */
export const TELEMETRY_STALE_MS = 10_000;

/**
 * Возраст снимка в миллисекундах.
 * @param lastUpdate значение `lastUpdate` (millis() устройства)
 * @param deviceUptimeMs текущий `uptime` устройства в миллисекундах (`GET /rest/systemStatus`)
 */
export function snapshotAgeMs(
  lastUpdate: number | undefined,
  deviceUptimeMs: number | undefined,
): number | null {
  if (lastUpdate === undefined || deviceUptimeMs === undefined) return null;
  const age = deviceUptimeMs - lastUpdate;
  return age >= 0 ? age : null;
}

/** Устарел ли снимок (null — определить нельзя, считаем «не устарел»). */
export function isSnapshotStale(
  lastUpdate: number | undefined,
  deviceUptimeMs: number | undefined,
  staleMs = TELEMETRY_STALE_MS,
): boolean | null {
  const age = snapshotAgeMs(lastUpdate, deviceUptimeMs);
  return age === null ? null : age > staleMs;
}

/** Тип фракции из снимка — с фолбэком на стадию процесса. */
export function snapshotFraction(snapshot: TelemetrySnapshot): string | null {
  return snapshot.telemetry.type ?? snapshot.status.stage ?? null;
}
