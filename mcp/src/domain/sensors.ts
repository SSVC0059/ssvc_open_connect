/**
 * Доменный слой «датчики, зоны и пороги тревог» (WP4, mcp-server-agents.md §8).
 *
 * Контракт устройства (mcp-contract-notes.md §B.2–§B.4, §B.7):
 *  - `GET /rest/sensor` → карта `{ "<zone>": { "<ADDRESS>": {v,u,type} } }` без обёртки;
 *  - `GET /rest/zones`  → `{"zones": {"<ADDRESS>": <zoneId|"zoneName">}}`;
 *  - `PUT /rest/sensors/zone?address=<hex>&zone=<name>` — одна привязка, параметры
 *    ТОЛЬКО в query (JSON-тело обработчик не разбирает, §B.7);
 *  - `GET/POST /rest/alarms` → `{"thresholds": {"<ADDRESS>": {...}}}`.
 *
 * Что определяет реализацию:
 *
 * 1. Все четыре маршрута admin-only. 403 транспорт маппит в
 *    `forbidden_admin_required` с русской подсказкой про учётную запись admin
 *    (`httpError`), поэтому домен не подменяет код.
 * 2. Адрес 1-Wire валидируется ЛОКАЛЬНО (16 hex-символов, §D.4): без этого
 *    опечатка уходит на устройство и превращается в 400/404, а «невалидный адрес»
 *    должен быть отказом ввода без сетевого запроса. Прошивка печатает адреса
 *    в верхнем регистре (`sprintf("%02X")`), поэтому адрес нормализуется к верхнему.
 * 3. `PUT /rest/sensors/zone` отвечает 404 не только на «датчик не найден», но и
 *    на идемпотентную запись: `SensorConfigService::setZoneForSensor` возвращает
 *    `false`, когда зона не изменилась (StatefulService отдаёт UNCHANGED). Поэтому
 *    после записи карта зон перечитывается и 404 трактуется по факту: если в карте
 *    уже стоит запрошенная зона — привязка достигнута, иначе это настоящая ошибка.
 *    Наружу всегда уходит ФАКТИЧЕСКАЯ зона из `GET /rest/zones`, а не эхо аргумента.
 * 4. Зоны в этой прошивке отдаются числовыми id (0…9). Домен переводит известные id
 *    в канонические имена (`tank`, `act`, …): инструмент `oc_set_zone` принимает имя,
 *    а описание инструмента обещает именно «карту адрес → зона» (куб, царга).
 *    Неизвестный id остаётся числом — выдумывать имя нельзя.
 * 5. `POST /rest/alarms` при валидном JSON-объекте отвечает 200 даже если ничего не
 *    применил (§B.4: `thresholds` не объект → UNCHANGED + 200). Поэтому после записи
 *    пороги перечитываются, а расхождение отправленного и фактического возвращается
 *    полями `applied`/`not_applied`/`mismatched`, а не «тихим успехом».
 * 6. SPA-заглушка (`200 text/html` на неизвестный GET, mcp-server.md §2.5 п.16) —
 *    это `feature_unsupported`, а не «пустые датчики».
 *
 * Записи идут через `client.withWrite('sensors', …)`: зоны и пороги делят один
 * доменный ключ, чтобы две записи не перемешались. Риск 1 (обратимая запись),
 * `confirm` не требуется.
 */

import type { z } from 'zod';
import type { DeviceClient, HttpResponse } from '../device/DeviceClient.js';
import {
  bodyForDetails,
  headerValue,
  isJsonContentType,
  parseJsonBody,
} from '../device/schemas/common.js';
import {
  ZONE_NAMES_BY_ID,
  alarmsEnvelopeSchema,
  sensorMapSchema,
  zonesEnvelopeSchema,
  type AlarmThresholds,
  type SensorMapRaw,
  type ZoneValue,
} from '../device/schemas/sensors.js';
import { OcError, httpError } from '../errors.js';
import { normalizeScalar } from './telemetry.js';

export const SENSOR_PATH = '/rest/sensor';
export const ZONES_PATH = '/rest/zones';
export const SENSOR_ZONE_PATH = '/rest/sensors/zone';
export const ALARMS_PATH = '/rest/alarms';

/** Ключ очереди записей домена: зоны и пороги меняются последовательно. */
const WRITE_KEY = 'sensors';

/** Адрес 1-Wire: 8 байт, 16 hex-символов (§B.7, `SensorManager::addressToString`). */
export const SENSOR_ADDRESS_RE = /^[0-9a-fA-F]{16}$/;

/** Допуск сравнения чисел при проверке применённых порогов (float32 в прошивке). */
const THRESHOLD_EPSILON = 0.001;

// ---------------------------------------------------------------------------
// Чтение JSON с диагностикой SPA-заглушки
// ---------------------------------------------------------------------------

/**
 * GET JSON-ответа с проверкой Content-Type.
 *
 * 404 здесь означает «маршрута нет в этой прошивке»: неизвестный путь прошивка
 * отдаёт SPA-страницей или 404, и «пустые данные» были бы ложью.
 */
async function readJson<T>(
  client: DeviceClient,
  path: string,
  schema: z.ZodType<T>,
): Promise<T> {
  const response = await client.request('GET', path, undefined, {
    headers: { accept: 'application/json' },
  });

  if (response.status === 404) {
    throw new OcError('feature_unsupported', `Прошивка не отдаёт ${path}`, {
      hint:
        'Обновите прошивку OpenConnect: маршрут должен быть в сборке с датчиками и зонами. ' +
        'Повтор запроса не поможет.',
      details: { status: response.status, path },
    });
  }
  if (response.status < 200 || response.status >= 300) {
    throw httpError({ status: response.status, body: response.body, method: 'GET', path });
  }

  const contentType = headerValue(response.headers, 'content-type') ?? null;
  if (!isJsonContentType(response.headers)) {
    throw new OcError(
      'feature_unsupported',
      `Ответ ${path} не является JSON: похоже на SPA-заглушку прошивки`,
      {
        hint:
          'Неизвестный маршрут прошивка отдаёт веб-страницей (200 text/html). Проверьте ' +
          `версию прошивки OpenConnect: в ней должен быть GET ${path}.`,
        details: {
          status: response.status,
          content_type: contentType,
          body: bodyForDetails(response.body),
        },
      },
    );
  }

  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) {
    throw new OcError('device_error', `Ответ ${path} не является корректным JSON`, {
      hint: 'Проверьте версию прошивки контроллера и повторите запрос.',
      details: {
        status: response.status,
        content_type: contentType,
        parse_error: parsed.error,
        body: bodyForDetails(response.body),
      },
    });
  }

  const result = schema.safeParse(parsed.value);
  if (!result.success) {
    throw new OcError('device_error', `Ответ ${path} не соответствует ожидаемой схеме`, {
      hint:
        'Расхождение контракта с прошивкой: проверьте версию контроллера и поля ответа ' +
        'в details.issues / details.body.',
      details: {
        status: response.status,
        issues: result.error.issues.map((issue) => ({
          path: issue.path.join('.'),
          message: issue.message,
        })),
        body: bodyForDetails(response.body),
      },
    });
  }
  return result.data;
}

// ---------------------------------------------------------------------------
// Нормализация адресов, зон и значений
// ---------------------------------------------------------------------------

/**
 * Проверяет и нормализует адрес датчика.
 *
 * Валидация локальная и намеренно строгая: адрес — единственный аргумент записи,
 * по которому прошивка ищет датчик. Опечатка не должна доходить до сети, а формат
 * задан прошивкой: ровно 16 hex-символов, регистр не важен.
 */
export function normalizeAddress(value: unknown): string {
  const text = typeof value === 'string' ? value.trim() : '';
  if (!SENSOR_ADDRESS_RE.test(text)) {
    throw new OcError(
      'validation_failed',
      'Адрес датчика должен состоять из 16 шестнадцатеричных символов',
      {
        hint:
          'Адрес 1-Wire — 8 байт, 16 hex-символов, например 28FF64A1B2C3D4E5. Возьмите адрес ' +
          'из oc_get_zones или oc_get_sensors. Проверка локальная: запрос к контроллеру ' +
          'не отправлялся.',
        details: {
          field: 'address',
          reason: 'invalid_address_format',
          value: typeof value === 'string' ? value.slice(0, 64) : String(value),
        },
      },
    );
  }
  return text.toUpperCase();
}

/** Имя зоны: пустая строка — отказ ввода; регистр приводится к нижнему (имена прошивки). */
export function normalizeZoneName(value: unknown): string {
  const text = typeof value === 'string' ? value.trim().toLowerCase() : '';
  if (text === '') {
    throw new OcError('validation_failed', 'Имя зоны не может быть пустым', {
      hint:
        'Передайте имя зоны: unknown, inlet_water, outlet_water, act, tank, outdoor, ' +
        'receiver_tank_1, receiver_tank_2, receiver_tank_3.',
      details: { field: 'zone', reason: 'empty_zone' },
    });
  }
  return text;
}

/**
 * Числовой id зоны → каноническое имя. Неизвестный id не выдумываем: остаётся числом,
 * чтобы расхождение было видно, а не превращалось в «unknown».
 */
export function zoneNameOf(value: ZoneValue): string | number {
  if (typeof value === 'number') return ZONE_NAMES_BY_ID[value] ?? value;
  return value;
}

/** Порядок ключей в ответах: прошивка отдаёт std::map, но порядок JSON не гарантирован. */
function compareText(a: string, b: string): number {
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
}

// ---------------------------------------------------------------------------
// Датчики: плоский маппинг зон
// ---------------------------------------------------------------------------

export interface SensorReading {
  /** Адрес 1-Wire (16 hex, верхний регистр — как у прошивки). */
  address: string;
  /** Значение: число, если строка была числовой; иначе строка как есть; null — нет значения. */
  value: number | string | null;
  /** Единицы измерения или null. */
  unit: string | null;
  /** Тип датчика или null. */
  type: string | null;
}

export interface SensorZoneReadings {
  zone: string;
  readings: SensorReading[];
}

export interface SensorsSnapshot {
  zones: SensorZoneReadings[];
  /** Время получения ответа (ISO 8601): прошивка своего времени в этом ответе не даёт. */
  observed_at: string;
}

/**
 * «Плоский» маппинг ответа: зона → массив показаний.
 *
 * Датчики в ответе сгруппированы по зонам, агенту же нужен один список зон с
 * readings[]. Зоны и адреса сортируются, чтобы ответ был детерминированным.
 * Пустая карта — пустой массив, не ошибка.
 */
export function groupSensorReadings(
  raw: SensorMapRaw,
  zoneFilter?: string | null,
): SensorZoneReadings[] {
  const wanted =
    zoneFilter !== undefined && zoneFilter !== null && zoneFilter.trim() !== ''
      ? zoneFilter.trim().toLowerCase()
      : null;

  return Object.entries(raw)
    .filter(([zone]) => wanted === null || zone.trim().toLowerCase() === wanted)
    .sort(([left], [right]) => compareText(left, right))
    .map(([zone, readings]) => ({
      zone,
      readings: Object.entries(readings)
        .sort(([left], [right]) => compareText(left, right))
        .map(([address, reading]) => ({
          address,
          value:
            reading.v === undefined
              ? null
              : ((normalizeScalar(reading.v) as number | string | null) ?? null),
          unit: reading.u ?? null,
          type: reading.type ?? null,
        })),
    }));
}

/** Читает `GET /rest/sensor` и группирует показания по зонам. */
export async function getSensors(
  client: DeviceClient,
  opts: { zone?: string | null } = {},
): Promise<SensorsSnapshot> {
  const raw = await readJson(client, SENSOR_PATH, sensorMapSchema);
  return {
    zones: groupSensorReadings(raw, opts.zone ?? null),
    observed_at: new Date().toISOString(),
  };
}

// ---------------------------------------------------------------------------
// Зоны
// ---------------------------------------------------------------------------

export interface ZoneMapSnapshot {
  /** Карта «адрес → имя зоны»; неизвестный числовой id остаётся числом. */
  zones: Record<string, string | number>;
  observed_at: string;
}

/** Читает `GET /rest/zones` и переводит числовые id зон в имена. */
export async function getZones(client: DeviceClient): Promise<ZoneMapSnapshot> {
  const data = await readJson(client, ZONES_PATH, zonesEnvelopeSchema);
  const zones: Record<string, string | number> = {};
  for (const address of Object.keys(data.zones).sort(compareText)) {
    zones[address] = zoneNameOf(data.zones[address]);
  }
  return { zones, observed_at: new Date().toISOString() };
}

export interface ZoneUpdateResult {
  address: string;
  /** Фактическая зона после записи (из перечитанной карты), не эхо аргумента. */
  zone: string | number | null;
  /** Совпадает ли фактическая зона с запрошенной. */
  applied: boolean;
  /** true — прошивка ответила 404, потому что зона уже была такой (идемпотентный вызов). */
  already_set: boolean;
  observed_at: string;
  /** Предупреждение, если прошивка приняла запись, но фактическое значение иное. */
  warning?: string;
}

/**
 * Привязывает один датчик к зоне.
 *
 * После `PUT /rest/sensors/zone` карта зон перечитывается: наружу уходит фактическое
 * значение, а не аргумент. Пустое тело ответа (200/204) — норма: данные берём из
 * повторного `GET /rest/zones`.
 *
 * @throws OcError `validation_failed` (адрес/зона), `forbidden_admin_required` (403),
 *         `not_found` (404 и фактическая зона не совпала), прочие коды транспорта.
 */
export async function setZone(
  client: DeviceClient,
  address: string,
  zone: string,
): Promise<ZoneUpdateResult> {
  const normalizedAddress = normalizeAddress(address);
  const normalizedZone = normalizeZoneName(zone);

  // 404 не бросаем сразу: у прошивки это ещё и «зона не изменилась» (см. шапку).
  const notFound: HttpResponse | null = await client.withWrite(WRITE_KEY, async () => {
    const response = await client.request('PUT', SENSOR_ZONE_PATH, undefined, {
      query: { address: normalizedAddress, zone: normalizedZone },
    });
    if (response.status >= 200 && response.status < 300) return null;
    if (response.status === 404) return response;
    if (response.status === 400) throw invalidZoneError(response);
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'PUT',
      path: SENSOR_ZONE_PATH,
    });
  });

  const snapshot = await getZones(client);
  const actual = snapshot.zones[normalizedAddress] ?? null;

  if (notFound !== null) {
    // Прошивка ответила 404, но карта зон подтверждает желаемое значение:
    // это идемпотентная запись (StatefulService вернул UNCHANGED → 404 в обработчике).
    if (actual !== null && String(actual) === normalizedZone) {
      return {
        address: normalizedAddress,
        zone: actual,
        applied: true,
        already_set: true,
        observed_at: snapshot.observed_at,
      };
    }
    throw new OcError('not_found', 'Контроллер не изменил привязку датчика к зоне', {
      hint:
        'Прошивка ответила 404: датчик с таким адресом не найден или привязка не применилась. ' +
        'Перечитайте oc_get_zones: если адреса в карте нет, проверьте адрес 1-Wire по oc_get_sensors.',
      details: {
        status: notFound.status,
        address: normalizedAddress,
        requested_zone: normalizedZone,
        actual_zone: actual,
        body: bodyForDetails(notFound.body),
      },
    });
  }

  const applied = actual !== null && String(actual) === normalizedZone;
  const result: ZoneUpdateResult = {
    address: normalizedAddress,
    zone: actual,
    applied,
    already_set: false,
    observed_at: snapshot.observed_at,
  };
  if (!applied) {
    result.warning =
      'Контроллер принял запрос, но перечитанная карта зон показывает другое значение. ' +
      'Проверьте адрес и имя зоны (неизвестное имя прошивка молча превращает в unknown).';
  }
  return result;
}

// ---------------------------------------------------------------------------
// Пороги тревог
// ---------------------------------------------------------------------------

export interface AlarmsSnapshot {
  /** Пороги в том виде, в каком их отдаёт прошивка: все поля, без пересборки. */
  thresholds: AlarmThresholds;
  observed_at: string;
}

/** Читает `GET /rest/alarms`. */
export async function getAlarms(client: DeviceClient): Promise<AlarmsSnapshot> {
  const data = await readJson(client, ALARMS_PATH, alarmsEnvelopeSchema);
  return { thresholds: data.thresholds, observed_at: new Date().toISOString() };
}

export interface ThresholdMismatch {
  address: string;
  field: string;
  requested: unknown;
  actual: unknown;
}

export interface AlarmWriteResult extends AlarmsSnapshot {
  /** Все отправленные пороги совпали с перечитанными. */
  applied: boolean;
  /** Адреса, которые были в запросе (для сверки с not_applied). */
  requested_addresses: string[];
  /** Адреса, которых после записи нет в перечитанной карте. */
  not_applied: string[];
  /** Поля, значение которых после записи отличается от отправленного. */
  mismatched: ThresholdMismatch[];
  /** Пояснение при `applied:false`: прошивка могла принять запрос и ничего не применить. */
  warning?: string;
}

/**
 * Обновляет пороги указанных датчиков.
 *
 * Тело — ровно `{thresholds: args.thresholds}`. Прошивка обновляет/добавляет только
 * переданные адреса (отсутствующие в запросе НЕ удаляются, удаляются лишь
 * незарегистрированные датчики, §B.4), поэтому частичный набор безопасен.
 *
 * После записи пороги перечитываются: `POST /rest/alarms` может ответить 200 и
 * ничего не применить, если тело не объект. Расхождение возвращается полями
 * `applied`/`not_applied`/`mismatched`, чтобы «тихий успех» был виден агенту.
 *
 * @throws OcError `validation_failed` (тело или 400 прошивки), `forbidden_admin_required`
 *         (403), прочие коды транспорта.
 */
export async function setAlarmThresholds(
  client: DeviceClient,
  thresholds: unknown,
): Promise<AlarmWriteResult> {
  const normalized = normalizeAlarmThresholds(thresholds);

  await client.withWrite(WRITE_KEY, async () => {
    const response = await client.request('POST', ALARMS_PATH, { thresholds: normalized }, {});
    if (response.status >= 200 && response.status < 300) return;
    if (response.status === 400) throw invalidAlarmsError(response);
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'POST',
      path: ALARMS_PATH,
    });
  });

  const snapshot = await getAlarms(client);
  const { notApplied, mismatched } = compareThresholds(normalized, snapshot.thresholds);
  const applied = notApplied.length === 0 && mismatched.length === 0;

  const result: AlarmWriteResult = {
    thresholds: snapshot.thresholds,
    applied,
    requested_addresses: Object.keys(normalized),
    not_applied: notApplied,
    mismatched,
    observed_at: snapshot.observed_at,
  };
  if (!applied) {
    result.warning =
      'Контроллер ответил успехом, но перечитанные пороги отличаются от отправленных: ' +
      'часть адресов не зарегистрирована или поля проигнорированы. Смотрите not_applied ' +
      'и mismatched; не повторяйте запрос вслепую.';
  }
  return result;
}

/**
 * Проверяет тело записи порогов: объект «адрес → объект полей», непустой.
 *
 * Значения не пересобираются: прошивка берёт недостающие поля из значений по
 * умолчанию, а неизвестные поля молча игнорирует — фильтровать их локально значило бы
 * ломать совместимость с новой прошивкой.
 */
export function normalizeAlarmThresholds(raw: unknown): Record<string, unknown> {
  if (!isPlainObject(raw)) {
    throw new OcError('validation_failed', 'Поле thresholds должно быть JSON-объектом', {
      hint:
        'Передайте карту «адрес датчика → пороги»: {"28FF64A1B2C3D4E5": ' +
        '{"enabled": true, "min": 0, "dangerous": 80, "critical": 100}}. Массив и null не ' +
        'принимаются.',
      details: { field: 'thresholds', reason: 'not_an_object' },
    });
  }
  const addresses = Object.keys(raw);
  if (addresses.length === 0) {
    throw new OcError('validation_failed', 'Не передан ни один адрес датчика', {
      hint:
        'Передайте хотя бы один адрес в thresholds: изменение порогов без адресов ничего ' +
        'не делает. Текущие пороги можно взять из oc_get_alarms.',
      details: { field: 'thresholds', reason: 'empty_thresholds' },
    });
  }

  const invalid = addresses.filter((address) => !isPlainObject(raw[address]));
  if (invalid.length > 0) {
    throw new OcError(
      'validation_failed',
      'Пороги каждого датчика должны быть объектами «поле → значение»',
      {
        hint:
          'Для каждого адреса передайте объект: {"enabled": true, "min": 0, "dangerous": 80, ' +
          '"critical": 100}. Прошивка молча пропускает значения другого типа.',
        details: { field: 'thresholds', reason: 'entry_not_object', addresses: invalid },
      },
    );
  }
  return raw;
}

/** Сверка отправленных и перечитанных порогов (числа — с допуском float32). */
function compareThresholds(
  requested: Record<string, unknown>,
  actual: AlarmThresholds,
): { notApplied: string[]; mismatched: ThresholdMismatch[] } {
  const notApplied: string[] = [];
  const mismatched: ThresholdMismatch[] = [];

  for (const [address, entryRaw] of Object.entries(requested)) {
    const entry = entryRaw as Record<string, unknown>;
    const actualEntry = actual[address];
    if (actualEntry === undefined) {
      notApplied.push(address);
      continue;
    }
    for (const [field, expected] of Object.entries(entry)) {
      const got = actualEntry[field];
      if (!sameThresholdValue(expected, got)) {
        mismatched.push({
          address,
          field,
          requested: expected,
          actual: got ?? null,
        });
      }
    }
  }
  return { notApplied, mismatched };
}

function sameThresholdValue(expected: unknown, actual: unknown): boolean {
  if (expected === actual) return true;

  const expectedNumber = finiteNumber(expected);
  const actualNumber = finiteNumber(actual);
  if (expectedNumber !== null && actualNumber !== null) {
    return Math.abs(expectedNumber - actualNumber) < THRESHOLD_EPSILON;
  }

  if (
    (typeof expected === 'object' && expected !== null) ||
    (typeof actual === 'object' && actual !== null)
  ) {
    return stableJson(expected) === stableJson(actual);
  }
  return false;
}

function finiteNumber(value: unknown): number | null {
  if (typeof value === 'number') return Number.isFinite(value) ? value : null;
  if (typeof value === 'string' && value.trim() !== '') {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : null;
  }
  return null;
}

function stableJson(value: unknown): string {
  try {
    return JSON.stringify(value) ?? 'undefined';
  } catch {
    return String(value);
  }
}

// ---------------------------------------------------------------------------
// Ошибки записи
// ---------------------------------------------------------------------------

/** 400 `PUT /rest/sensors/zone`: тело прошивки с `message`/`address`/`received_zone`. */
function invalidZoneError(response: HttpResponse): OcError {
  const fields = parseErrorFields(response.body);
  return new OcError('validation_failed', 'Контроллер отклонил адрес или зону датчика', {
    hint:
      'Проверьте адрес (16 hex-символов) и имя зоны (unknown, inlet_water, outlet_water, act, ' +
      'tank, outdoor, receiver_tank_1…3). Ответ прошивки — в details.fields.',
    details: {
      status: response.status,
      method: 'PUT',
      path: SENSOR_ZONE_PATH,
      fields,
      body: bodyForDetails(response.body),
    },
  });
}

/** 400 `POST /rest/alarms`: тело может быть картой «поле → причина» или `{error}`. */
function invalidAlarmsError(response: HttpResponse): OcError {
  const fields = parseErrorFields(response.body);
  const invalidFields = Object.keys(fields).filter((key) => key !== 'error' && key !== 'message');
  const message =
    invalidFields.length > 0
      ? `Контроллер отклонил пороги: ${invalidFields.join(', ')}`
      : 'Контроллер отклонил тело запроса порогов';
  return new OcError('validation_failed', message, {
    hint:
      'Проверьте типы и диапазоны полей enabled/min/dangerous/critical (details.fields). ' +
      'При 400 прошивка не применила ни одного порога: исправьте значения и повторите запрос.',
    details: {
      status: response.status,
      method: 'POST',
      path: ALARMS_PATH,
      fields,
      invalid_fields: invalidFields,
      body: bodyForDetails(response.body),
    },
  });
}

/** Поля ошибки из тела ответа: строками, чтобы их было видно в details.fields. */
function parseErrorFields(body: string): Record<string, string> {
  const fields: Record<string, string> = {};
  const parsed = parseJsonBody(body);
  if (!parsed.ok || !isPlainObject(parsed.value)) return fields;
  for (const [key, value] of Object.entries(parsed.value)) {
    fields[key] = describeValue(value);
  }
  return fields;
}

function describeValue(value: unknown): string {
  if (value === null) return 'null';
  if (typeof value === 'string') return value;
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  try {
    return JSON.stringify(value);
  } catch {
    return String(value);
  }
}

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}
