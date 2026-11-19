/**
 * Доменный слой «состояние и телеметрия» (WP1, mcp-server-agents.md §5 + «Обновление после §9»).
 *
 * Источники:
 *  - `GET /rest/oc/snapshot` — один admin-запрос: `{observed_at, info, telemetry, status,
 *    lastUpdate, sensor, faults, partial[]}` (основной инструмент `oc_get_state`);
 *  - `GET /rest/telemetry`  — совместимость (`oc_get_telemetry`, `oc_get_process_status`);
 *  - `GET /rest/sensor`     — показания датчиков по зонам.
 *
 * Три особенности прошивки, из-за которых этот слой существует:
 *
 * 1. `lastUpdate` и `observed_at` — это `millis()` устройства (uptime), НЕ epoch.
 *    Сравнивать их с часами сервера нельзя. Поэтому «свежесть» считается по памяти:
 *    запоминаем значение `lastUpdate` и МОНОТОННОЕ время его получения (время ответа
 *    сервера). Если значение не менялось дольше 10 с — данные устарели (`stale:true`).
 *    `observed_at` — время сборки снимка: оно меняется на каждом запросе и о свежести
 *    телеметрии ничего не говорит, поэтому берётся только как запасной вариант,
 *    когда `lastUpdate` в ответе нет.
 *
 * 2. Числа в ответах бывают и числом, и строкой (`"766.9"`). Нормализуем единообразно,
 *    не теряя точность: числовая строка → число; слишком длинное целое (>15 цифр) и
 *    нечисловые строки остаются строкой.
 *
 * 3. `partial[]` — имена компонентов, которые прошивка не смогла собрать. Это «данных
 *    нет», а не «ноль»: список обязательно доносится до агента, а отсутствующий
 *    запрошенный компонент добавляется в него на стороне инструмента.
 *
 * Неизвестный GET прошивка отдаёт SPA-заглушкой `200 text/html` (mcp-server.md §2.5
 * п.16), поэтому отсутствие маршрута `/rest/oc/snapshot` на старой прошивке
 * определяется по Content-Type/телу и превращается в `feature_unsupported`, а не в
 * `device_error` или падение.
 */

import { z } from 'zod';
import type { DeviceClient } from '../device/DeviceClient.js';
import { OcError } from '../errors.js';
import { ocInfoSchema } from '../device/schemas/info.js';
import {
  TELEMETRY_STALE_MS,
  rectStatusSchema,
  type RectStatus,
} from '../device/schemas/telemetry.js';
import { mapInfo, type InfoView } from './info.js';
import { readJson } from './readJson.js';

export const SNAPSHOT_PATH = '/rest/oc/snapshot';
export const TELEMETRY_PATH = '/rest/telemetry';
export const SENSOR_PATH = '/rest/sensor';

// ---------------------------------------------------------------------------
// Схемы ответов (терпимые к прошивке: лишние поля проходят, числа — числом или строкой)
// ---------------------------------------------------------------------------

/** Значение датчика: прошивка отдаёт то числом, то строкой. */
const readingValueSchema = z.union([z.number(), z.string()]);

export const sensorReadingRawSchema = z
  .object({
    v: readingValueSchema.optional(),
    u: z.string().optional(),
    type: z.string().optional(),
  })
  .passthrough();

export const sensorMapRawSchema = z.record(z.record(sensorReadingRawSchema));

export const hardwareFaultSchema = z
  .object({
    code: z.string().optional(),
    codeLabel: z.string().optional(),
    address: z.union([z.number(), z.string()]).optional(),
    role: z.string().optional(),
  })
  .passthrough();

export const hardwareFaultsSchema = z
  .object({
    faults: z.array(hardwareFaultSchema).optional(),
    count: z.number().optional(),
    i2cBusActive: z.boolean().optional(),
    i2cScan: z.array(z.union([z.number(), z.string()])).optional(),
  })
  .passthrough();

/** `observed_at`/`lastUpdate` — millis() устройства; допускаем строку и нормализуем. */
const uptimeSchema = z.union([z.number(), z.string()]);

export const snapshotSchema = z
  .object({
    observed_at: uptimeSchema.optional(),
    lastUpdate: uptimeSchema.optional(),
    info: ocInfoSchema.optional(),
    telemetry: z.record(z.unknown()).optional(),
    status: rectStatusSchema.optional(),
    sensor: sensorMapRawSchema.optional(),
    faults: hardwareFaultsSchema.optional(),
    partial: z.array(z.string()).optional(),
  })
  .passthrough();

export const telemetryResponseSchema = z
  .object({
    telemetry: z.record(z.unknown()),
    status: rectStatusSchema,
    lastUpdate: uptimeSchema.optional(),
  })
  .passthrough();

export type SnapshotRaw = z.infer<typeof snapshotSchema>;
export type TelemetryResponseRaw = z.infer<typeof telemetryResponseSchema>;
export type SensorMap = Record<string, Record<string, Record<string, unknown>>>;

// ---------------------------------------------------------------------------
// Нормализация чисел/строк
// ---------------------------------------------------------------------------

/**
 * Числовая строка → число, иначе строка как есть.
 * Длинные целые (>15 цифр) не трогаем: `Number` потерял бы точность.
 */
export function normalizeScalar(value: unknown): unknown {
  if (typeof value === 'string') {
    const text = value.trim();
    if (text === '') return value;
    const numeric = /^[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?$/.test(text);
    if (!numeric) return value;
    const digits = text.replace(/^[-+]/, '').replace(/\./g, '');
    if (/^[-+]?\d+$/.test(text) && digits.length > 15) return value;
    const parsed = Number(text);
    return Number.isFinite(parsed) ? parsed : value;
  }
  return value;
}

function normalizeDeep(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(normalizeDeep);
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [key, item] of Object.entries(value as Record<string, unknown>)) {
      out[key] = normalizeDeep(item);
    }
    return out;
  }
  return normalizeScalar(value);
}

/**
 * Приводит телеметрию к единому виду: числовые строки → числа, вложенные объекты —
 * рекурсивно. Имена полей и значения-строки (`"waiting"`, `"00:41:03"`) сохраняются.
 */
export function normalizeTelemetryTree(value: Record<string, unknown>): Record<string, unknown> {
  return normalizeDeep(value) as Record<string, unknown>;
}

/** Нормализует показания датчиков, сохраняя контракт прошивки (`v/u/type`). */
export function normalizeSensorMap(raw: z.infer<typeof sensorMapRawSchema>): SensorMap {
  const out: SensorMap = {};
  for (const [zone, readings] of Object.entries(raw)) {
    const zoneOut: Record<string, Record<string, unknown>> = {};
    for (const [address, reading] of Object.entries(readings)) {
      zoneOut[address] = {
        ...reading,
        v: normalizeScalar(reading.v) ?? null,
        u: reading.u ?? null,
        type: reading.type ?? null,
      };
    }
    out[zone] = zoneOut;
  }
  return out;
}

// ---------------------------------------------------------------------------
// Статус процесса и сбои
// ---------------------------------------------------------------------------

/** Плоский статус ректификации — то, что нужно агенту, без camelCase прошивки. */
export interface ProcessStatusView {
  stage: string | null;
  status: string | null;
  uart_error: boolean;
  started_at: string | null;
  ended_at: string | null;
  stages: Record<string, string>;
}

export function normalizeStatus(status: RectStatus | undefined): ProcessStatusView {
  const stages: Record<string, string> = {};
  for (const [stage, state] of Object.entries(status?.stages ?? {})) {
    stages[stage] = String(state);
  }
  return {
    stage: status?.stage ?? null,
    status: status?.status ?? null,
    uart_error: status?.uartConnectionError === true,
    started_at: status?.startTime ?? null,
    ended_at: status?.endTime ?? null,
    stages,
  };
}

export interface HardwareFaultsView {
  faults: Array<Record<string, unknown>>;
  count: number;
  i2c_bus_active: boolean | null;
  i2c_scan: Array<number | string>;
}

export function normalizeFaults(
  faults: z.infer<typeof hardwareFaultsSchema>,
): HardwareFaultsView {
  const list = (faults.faults ?? []).map((fault) => ({ ...fault }));
  return {
    faults: list,
    count: faults.count ?? list.length,
    i2c_bus_active: faults.i2cBusActive ?? null,
    i2c_scan: faults.i2cScan ?? [],
  };
}

// ---------------------------------------------------------------------------
// Свежесть данных (stale) — по монотонному времени получения ответа
// ---------------------------------------------------------------------------

export interface Staleness {
  /** null — значения нет, определить нельзя. */
  stale: boolean | null;
  /** Сколько миллисекунд значение `lastUpdate` не менялось (null — неизвестно). */
  age_ms: number | null;
  /** Наблюдённое значение `lastUpdate`/`observed_at` (millis() устройства). */
  last_update_ms: number | null;
}

export interface StalenessTrackerOptions {
  /** Монотонное «текущее время»: в тестах подменяется, в бою — Date.now. */
  now?: () => number;
  /** Порог устаревания; по умолчанию 10 с (контракт WP1). */
  staleMs?: number;
}

/**
 * Помнит последнее значение `lastUpdate`/`observed_at` и монотонное время его появления.
 *
 * Почему так, а не «сравнить lastUpdate с часами»: `lastUpdate` — это `millis()`
 * устройства (uptime), у него другая точка отсчёта и переполнение через ~49.7 суток.
 * Единственный корректный способ — заметить, что значение ПЕРЕСТАЛО МЕНЯТЬСЯ.
 *
 * «Не менялось дольше 10 с» — строго больше порога: значение, пришедшее ровно 10 с
 * назад, ещё считается свежим.
 */
export class StalenessTracker {
  private readonly now: () => number;
  private readonly staleMs: number;
  private value: number | null = null;
  private changedAt: number | null = null;

  constructor(opts: StalenessTrackerOptions = {}) {
    this.now = opts.now ?? Date.now;
    this.staleMs = opts.staleMs ?? TELEMETRY_STALE_MS;
  }

  /** Регистрирует наблюдение и возвращает текущую оценку свежести. */
  observe(value: number | null | undefined): Staleness {
    if (value === null || value === undefined) {
      // Значения нет — не сбрасываем память: следующее числовое наблюдение продолжит её.
      return { stale: null, age_ms: null, last_update_ms: null };
    }
    const now = this.now();
    if (this.value === null || value !== this.value) {
      this.value = value;
      this.changedAt = now;
    }
    const age = this.changedAt === null ? 0 : Math.max(0, now - this.changedAt);
    return { stale: age > this.staleMs, age_ms: age, last_update_ms: value };
  }

  /** Забыть наблюдения (например, после перезапуска сервиса). */
  reset(): void {
    this.value = null;
    this.changedAt = null;
  }
}

/**
 * Один трекер на контроллер (ключ — `DeviceClient`, общий для всех сессий MCP).
 * `WeakMap` не течёт и даёт изоляцию между тестовыми контекстами.
 */
const trackers = new WeakMap<object, StalenessTracker>();

export function stalenessTrackerFor(client: object): StalenessTracker {
  let tracker = trackers.get(client);
  if (tracker === undefined) {
    tracker = new StalenessTracker();
    trackers.set(client, tracker);
  }
  return tracker;
}

/**
 * Подмена трекера (в первую очередь — инъекция часов в тестах).
 * В рабочем коде не вызывается: трекер создаётся лениво на контроллер.
 */
export function bindStalenessTracker(client: object, tracker: StalenessTracker): void {
  trackers.set(client, tracker);
}

// ---------------------------------------------------------------------------
// Чтение устройства
// ---------------------------------------------------------------------------

function toMillis(value: number | string | undefined): number | null {
  if (value === undefined) return null;
  if (typeof value === 'number') return Number.isFinite(value) ? value : null;
  const parsed = Number(value.trim());
  return Number.isFinite(parsed) ? parsed : null;
}

const SNAPSHOT_UNSUPPORTED_MESSAGE = 'Контроллер не отдаёт снимок состояния /rest/oc/snapshot';
const SNAPSHOT_UNSUPPORTED_HINT =
  'Нужна прошивка OpenConnect с GET /rest/oc/snapshot (0.2.9.x или новее). ' +
  'Пока используйте oc_get_telemetry и oc_get_process_status: они читают /rest/telemetry.';

/**
 * Читает `/rest/oc/snapshot` и проверяет форму ответа.
 *
 * Отсутствие маршрута на старой прошивке выглядит как успех: `200 text/html` (SPA-заглушка)
 * или `501`. Поэтому «нет маршрута» определяется общим разбором (`readJson.ts`: статус,
 * Content-Type, JSON, схема), а домен задаёт только текст с подсказкой про запасные
 * инструменты. Разбор в домене не дублируется.
 */
export async function fetchSnapshot(client: DeviceClient): Promise<SnapshotRaw> {
  return readJson(client, SNAPSHOT_PATH, snapshotSchema, {
    unsupportedStatuses: [501],
    notFound: { message: SNAPSHOT_UNSUPPORTED_MESSAGE, hint: SNAPSHOT_UNSUPPORTED_HINT },
    notJson: { message: SNAPSHOT_UNSUPPORTED_MESSAGE, hint: SNAPSHOT_UNSUPPORTED_HINT },
    invalidJson: { message: 'Ответ /rest/oc/snapshot не является корректным JSON' },
    schemaMismatch: {
      message: 'Ответ /rest/oc/snapshot не соответствует ожидаемой схеме',
      hint:
        'Расхождение контракта с прошивкой: посмотрите details.issues/details.body ' +
        'и при необходимости включите SSVC_RAW_LOG=all.',
    },
  });
}

/**
 * Читает `GET /rest/telemetry` (совместимость со старыми прошивками).
 * Отсутствие блока `telemetry` — ошибка устройства, а не «нули».
 */
export async function fetchTelemetry(client: DeviceClient): Promise<{
  telemetry: Record<string, unknown>;
  status: RectStatus;
  lastUpdate: number | null;
}> {
  const data = await readJson(client, TELEMETRY_PATH, telemetryResponseSchema, {
    notFound: {
      message: 'Прошивка не отдаёт телеметрию /rest/telemetry',
      hint:
        'Проверьте версию прошивки SSVC: /rest/telemetry — базовый маршрут, его отсутствие ' +
        'означает несовместимую сборку.',
    },
    notJson: {
      code: 'device_error',
      message: 'Ответ /rest/telemetry не является JSON',
      hint: 'Похоже, прошивка не знает маршрута и вернула веб-страницу. Проверьте версию прошивки.',
    },
    invalidJson: { message: 'Ответ /rest/telemetry не является корректным JSON' },
    schemaMismatch: {
      message: 'Ответ /rest/telemetry не содержит телеметрии или имеет неожиданную форму',
      hint:
        'Прошивка должна вернуть {telemetry:{...}, status:{...}, lastUpdate}. ' +
        'Отсутствие telemetry — это «данных нет», а не нулевые показания.',
    },
  });
  return {
    telemetry: data.telemetry,
    status: data.status,
    lastUpdate: toMillis(data.lastUpdate),
  };
}

/** Читает `GET /rest/sensor` (admin). Возвращает нормализованную карту зон. */
export async function fetchSensors(client: DeviceClient): Promise<SensorMap> {
  const raw = await readJson(client, SENSOR_PATH, sensorMapRawSchema, {
    notFound: { message: 'Прошивка не отдаёт датчики /rest/sensor' },
    notJson: {
      code: 'device_error',
      message: 'Ответ /rest/sensor не является JSON',
      hint: 'Проверьте версию прошивки контроллера.',
    },
    invalidJson: {
      message: 'Ответ /rest/sensor не является корректным JSON',
      hint: 'Проверьте версию прошивки контроллера.',
    },
    schemaMismatch: {
      message: 'Ответ /rest/sensor не соответствует ожидаемой схеме',
      hint: 'Ожидалась карта «зона → адрес → {v,u,type}». Проверьте версию прошивки.',
    },
  });
  return normalizeSensorMap(raw);
}

// ---------------------------------------------------------------------------
// Снимок состояния для oc_get_state
// ---------------------------------------------------------------------------

export interface StateView {
  /** millis() устройства на момент сборки снимка (не epoch!). */
  observed_at: number | null;
  /** millis() устройства на момент обновления телеметрии (не epoch!). */
  last_update_ms: number | null;
  stale: boolean | null;
  /** Сколько миллисекунд `lastUpdate` не менялся (по времени получения ответа). */
  freshness_age_ms: number | null;
  info: InfoView | null;
  telemetry: Record<string, unknown> | null;
  status: ProcessStatusView;
  sensor: SensorMap | null;
  faults: HardwareFaultsView | null;
  partial: string[];
}

/**
 * Превращает ответ `/rest/oc/snapshot` в модель состояния.
 * Отсутствие `telemetry` — ошибка устройства: снимок без телеметрии бессмысленен,
 * а отдавать «нули» вместо данных нельзя (контракт §5, «Обновление после §9»).
 *
 * `requireTelemetry:false` — вызывающий явно запросил только другие компоненты
 * (`oc_get_state` с include без telemetry): отсутствие блока не должно ломать ответ.
 */
export function buildStateView(
  snapshot: SnapshotRaw,
  tracker: StalenessTracker,
  opts: { requireTelemetry?: boolean } = {},
): StateView {
  const requireTelemetry = opts.requireTelemetry ?? true;
  if (requireTelemetry && snapshot.telemetry === undefined) {
    throw new OcError('device_error', 'Снимок состояния не содержит телеметрии', {
      hint:
        'Контроллер вернул /rest/oc/snapshot без блока telemetry. Проверьте partial: ' +
        'возможно, SSVC недоступен по UART или компонент не собрался.',
      details: {
        observed_at: toMillis(snapshot.observed_at),
        has_status: snapshot.status !== undefined,
        partial: snapshot.partial ?? [],
      },
    });
  }

  const observedAt = toMillis(snapshot.observed_at);
  const lastUpdate = toMillis(snapshot.lastUpdate);
  // lastUpdate — время обновления телеметрии; observed_at меняется на каждом запросе
  // и потому годится только как запасной сигнал, если lastUpdate не пришёл.
  const freshness = tracker.observe(lastUpdate ?? observedAt);

  return {
    observed_at: observedAt,
    last_update_ms: lastUpdate,
    stale: freshness.stale,
    freshness_age_ms: freshness.age_ms,
    info: snapshot.info === undefined ? null : mapInfo(snapshot.info),
    telemetry: snapshot.telemetry === undefined ? null : normalizeTelemetryTree(snapshot.telemetry),
    status: normalizeStatus(snapshot.status),
    sensor: snapshot.sensor === undefined ? null : normalizeSensorMap(snapshot.sensor),
    faults: snapshot.faults === undefined ? null : normalizeFaults(snapshot.faults),
    partial: [...(snapshot.partial ?? [])],
  };
}
