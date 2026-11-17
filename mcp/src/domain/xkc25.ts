/**
 * Доменный слой датчиков уровня XKC-25 (WP6, mcp-server-agents.md §10).
 *
 * Контракт устройства (mcp-contract-notes.md §B.12, §C.7, §D.5):
 *  - `GET /rest/oc/xkc25/status` — слоты датчиков и предстартовая проверка
 *    (`blockStart`, `blockReasons`, `warnings`);
 *  - `POST /rest/oc/xkc25/calibrate` — один шаг мастера. Для `without_liquid`/`with_liquid`
 *    ответ 200 с итогом шага; для `verify_return` — 202 `{status:"pending"}`, результат
 *    забирается опросом `verify-status`;
 *  - `GET /rest/oc/xkc25/verify-status` — `none` | `pending` | `done`.
 *
 * Что определяет реализацию:
 *
 * 1. Шаг калибровки меняет сырые уровни датчика в конфигурации стенда
 *    (`levelWithoutLiquid`/`levelWithLiquid`), поэтому POST не повторяется ни при каких
 *    обстоятельствах: ни транспорт, ни домен. Повтор — только явный новый вызов
 *    инструмента человеком.
 * 2. `verify_return` асинхронен: 202 означает «задание принято», а не «проверка прошла».
 *    Опрос идёт сразу после постановки и далее каждые 500 мс. Таймаут опроса — НЕ ошибка:
 *    проверка продолжается на контроллере, поэтому отдаём `ok:true` с `verify.status:
 *    "pending"` и подсказкой повторить `oc_xkc25_verify_status`.
 * 3. Коды шага различают три исхода: `sensor_not_available`/`sensor_not_configured` (404) —
 *    датчика нет, это `not_found`; `unstable_signal` и другие 409 — состояние мешает шагу,
 *    это `validation_failed` с кодом устройства в details; `persist_failed` (500) —
 *    контроллер не смог сохранить калибровку, это `device_error`.
 * 4. Запись сериализуется `client.withWrite('xkc25', …)`: одновременно две калибровки
 *    на одном устройстве недопустимы. Риск 1 проверяет инструмент; `confirm` не нужен.
 * 5. Транспорт сам ставит `Content-Length` (chunked прошивка рвёт соединение), домен
 *    ничего не дублирует.
 */

import type { DeviceClient, HttpResponse } from '../device/DeviceClient.js';
import { bodyForDetails, parseJsonBody } from '../device/schemas/common.js';
import {
  XKC25_STEPS,
  xkc25CalibrationSchema,
  xkc25StatusSchema,
  xkc25VerifyPendingSchema,
  xkc25VerifyStatusSchema,
  type Xkc25CalibrationRaw,
  type Xkc25SensorRaw,
  type Xkc25Step,
} from '../device/schemas/xkc25.js';
import { OcError, httpError } from '../errors.js';
import { readJson } from './readJson.js';

export const XKC25_STATUS_PATH = '/rest/oc/xkc25/status';
export const XKC25_CALIBRATE_PATH = '/rest/oc/xkc25/calibrate';
export const XKC25_VERIFY_STATUS_PATH = '/rest/oc/xkc25/verify-status';

/** Ключ очереди записей: калибровка одного устройства выполняется последовательно. */
export const XKC25_WRITE_KEY = 'xkc25';

/** Интервал опроса `verify-status` — 500 мс по §10 (mcp-server-agents.md). */
export const XKC25_POLL_INTERVAL_MS = 500;

export const XKC25_DEFAULT_TIMEOUT_MS = 5_000;
export const XKC25_MIN_TIMEOUT_MS = 1_000;
export const XKC25_MAX_TIMEOUT_MS = 15_000;

/**
 * Предел ожидания возврата на самой прошивке — 5000 мс
 * (`kMaxVerifyReturnTimeoutMs`, `OpenConnectHandler.cpp:610`). Прошивка обрезает большее
 * значение сама, но отправляем уже осмысленное: `timeout_ms` инструмента — это общий
 * бюджет опроса MCP-сервера, а не только ожидание на контроллере.
 */
export const XKC25_MAX_DEVICE_TIMEOUT_MS = 5_000;

/** Подсказка при таймауте опроса — дословно по §10. */
export const XKC25_VERIFY_PENDING_HINT =
  'проверка продолжается, повторите oc_xkc25_verify_status';

// ---------------------------------------------------------------------------
// Статус датчиков
// ---------------------------------------------------------------------------

export interface Xkc25StatusSnapshot {
  /** Слоты датчиков в том виде, в каком их отдала прошивка (без пересборки). */
  sensors: Xkc25SensorRaw[];
  trigger_action: number | null;
  start_guard_mode: number | null;
  block_start: boolean;
  block_reasons: string[];
  warnings: string[];
  /** Время получения ответа MCP-сервером: время калибровки прошивки — не epoch. */
  observed_at: string;
}

/**
 * Читает `GET /rest/oc/xkc25/status`.
 *
 * SPA-заглушка (`200 text/html`) и 404 на неизвестный маршрут превращаются в
 * `feature_unsupported` общим `readJson`: это «маршрута нет в прошивке», а не
 * «датчиков нет».
 */
export async function getXkc25Status(client: DeviceClient): Promise<Xkc25StatusSnapshot> {
  const raw = await readJson(client, XKC25_STATUS_PATH, xkc25StatusSchema);
  return {
    sensors: raw.sensors,
    trigger_action: raw.triggerAction ?? null,
    start_guard_mode: raw.startGuardMode ?? null,
    block_start: raw.blockStart ?? false,
    block_reasons: raw.blockReasons ?? [],
    warnings: raw.warnings ?? [],
    observed_at: new Date().toISOString(),
  };
}

// ---------------------------------------------------------------------------
// Результат асинхронной проверки возврата
// ---------------------------------------------------------------------------

/**
 * Результат `verify-status`. Поля `ok/completed/code/...` появляются только у `done`;
 * `null` означает «прошивка их не прислала», а не `false`.
 */
export interface Xkc25VerifyView {
  status: string;
  index: number | null;
  ok: boolean | null;
  completed: boolean | null;
  code: string | null;
  message: string | null;
  raw_level: number | null;
}

function toVerifyView(raw: {
  status: string;
  index?: number;
  ok?: boolean;
  completed?: boolean;
  code?: string;
  message?: string;
  rawLevel?: number;
}): Xkc25VerifyView {
  return {
    status: raw.status,
    index: raw.index ?? null,
    ok: raw.ok ?? null,
    completed: raw.completed ?? null,
    code: raw.code ?? null,
    message: raw.message ?? null,
    raw_level: raw.rawLevel ?? null,
  };
}

/** Читает `GET /rest/oc/xkc25/verify-status`; 404/SPA → `feature_unsupported`. */
export async function getXkc25VerifyStatus(
  client: DeviceClient,
): Promise<Xkc25VerifyView & { observed_at: string }> {
  const raw = await readJson(client, XKC25_VERIFY_STATUS_PATH, xkc25VerifyStatusSchema);
  return { ...toVerifyView(raw), observed_at: new Date().toISOString() };
}

// ---------------------------------------------------------------------------
// Калибровка
// ---------------------------------------------------------------------------

export interface Xkc25CalibrationRequest {
  /** Слот датчика: целое 0..3 (локальная проверка до сети). */
  index: number;
  step: Xkc25Step;
  /** Общий бюджет ожидания результата `verify_return`, 1000..15000 мс. */
  timeout_ms: number;
}

function describeValue(value: unknown): string {
  if (typeof value === 'string') return value;
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  if (value === undefined) return 'undefined';
  if (value === null) return 'null';
  try {
    return JSON.stringify(value);
  } catch {
    return String(value);
  }
}

/**
 * Локальная проверка аргументов шага калибровки: опечатка не должна уходить POST-ом,
 * который перезаписывает сырые уровни датчика.
 *
 * @throws OcError `validation_failed` с указанием поля и причины.
 */
export function normalizeCalibrationArgs(
  args: Record<string, unknown>,
): Xkc25CalibrationRequest {
  const rawIndex = args.index;
  if (typeof rawIndex !== 'number' || !Number.isInteger(rawIndex) || rawIndex < 0 || rawIndex > 3) {
    throw new OcError('validation_failed', 'Номер датчика index должен быть целым числом 0..3', {
      hint:
        'Возьмите index из oc_get_xkc25_status: это 0-based слот конфигурации. Запрос к ' +
        'контроллеру не отправлялся.',
      details: {
        field: 'index',
        reason: 'invalid_index',
        value: describeValue(rawIndex),
        allowed: '0..3',
      },
    });
  }

  const rawStep = args.step;
  if (typeof rawStep !== 'string' || !(XKC25_STEPS as readonly string[]).includes(rawStep)) {
    throw new OcError(
      'validation_failed',
      `Неизвестный шаг калибровки «${describeValue(rawStep)}»`,
      {
        hint:
          `Допустимые шаги: ${XKC25_STEPS.join(', ')}. Запрос к контроллеру не отправлялся.`,
        details: {
          field: 'step',
          reason: 'invalid_step',
          value: describeValue(rawStep),
          allowed: [...XKC25_STEPS],
        },
      },
    );
  }

  const rawTimeout = args.timeout_ms;
  let timeoutMs = XKC25_DEFAULT_TIMEOUT_MS;
  if (rawTimeout !== undefined) {
    if (
      typeof rawTimeout !== 'number' ||
      !Number.isInteger(rawTimeout) ||
      rawTimeout < XKC25_MIN_TIMEOUT_MS ||
      rawTimeout > XKC25_MAX_TIMEOUT_MS
    ) {
      throw new OcError(
        'validation_failed',
        `Таймаут timeout_ms должен быть целым числом ${XKC25_MIN_TIMEOUT_MS}..${XKC25_MAX_TIMEOUT_MS}`,
        {
          hint:
            'По умолчанию 5000 мс. Для verify_return это общий бюджет ожидания результата: ' +
            'на контроллере сама проверка ограничена 5 секундами. Запрос не отправлялся.',
          details: {
            field: 'timeout_ms',
            reason: 'invalid_timeout',
            value: describeValue(rawTimeout),
            allowed: `${XKC25_MIN_TIMEOUT_MS}..${XKC25_MAX_TIMEOUT_MS}`,
          },
        },
      );
    }
    timeoutMs = rawTimeout;
  }

  return { index: rawIndex, step: rawStep as Xkc25Step, timeout_ms: timeoutMs };
}

/** Итог инструмента `oc_xkc25_calibrate` (конверт `ok:true` добавляет инструмент). */
export interface Xkc25CalibrationResult {
  /**
   * Поле `ok` прошивки: шаг выполнен. Отдельное имя нужно потому, что `ok` верхнего
   * уровня занят конвертом инструмента.
   */
  device_ok: boolean;
  completed: boolean;
  code: string;
  message: string;
  checklist: string[];
  sensor: Xkc25SensorRaw | null;
  /** Только для шага `verify_return`: итог асинхронной проверки. */
  verify?: Xkc25VerifyView;
  /** Только при таймауте опроса: проверка продолжается. */
  hint?: string;
  observed_at: string;
}

export interface Xkc25CalibrateOptions {
  /** Интервал опроса verify-status, мс (в тестах уменьшается). По умолчанию 500. */
  pollMs?: number;
  /** Инъекция времени/паузы для тестов. */
  now?: () => number;
  sleep?: (ms: number) => Promise<void>;
}

function defaultSleep(ms: number): Promise<void> {
  return new Promise((resolve) => {
    const timer = setTimeout(resolve, ms);
    timer.unref?.();
  });
}

/**
 * Выполняет один шаг калибровки.
 *
 * `without_liquid`/`with_liquid` — ровно один POST, тело `{index, step}`, без повторов.
 * `verify_return` — POST `{index, step, timeoutMs}`, затем опрос `verify-status` до
 * `status !== "pending"` или до общего `timeout_ms`.
 *
 * @throws OcError `validation_failed` (локальная проверка, 400, 409),
 *         `not_found` (404 sensor_not_available/configured), `device_error` (500/битый JSON),
 *         `feature_unsupported` (404 маршрута/SPA при чтении статуса), коды транспорта.
 */
export async function calibrateXkc25(
  client: DeviceClient,
  request: Xkc25CalibrationRequest,
  options: Xkc25CalibrateOptions = {},
): Promise<Xkc25CalibrationResult> {
  if (request.step !== 'verify_return') {
    return runCalibrationStep(client, request);
  }
  return runVerifyReturn(client, request, options);
}

/** Шаг, который сразу отдаёт итог: один POST и тело шага как есть. */
async function runCalibrationStep(
  client: DeviceClient,
  request: Xkc25CalibrationRequest,
): Promise<Xkc25CalibrationResult> {
  const body = { index: request.index, step: request.step };
  const response = await client.withWrite(XKC25_WRITE_KEY, () =>
    client.request('POST', XKC25_CALIBRATE_PATH, body, {
      headers: { accept: 'application/json' },
    }),
  );
  return calibrationResult(parseCalibrationBody(response, request), 'ok');
}

/** `verify_return`: 202 → опрос; 2xx без 202 (защита от другой прошивки) → готовый итог. */
async function runVerifyReturn(
  client: DeviceClient,
  request: Xkc25CalibrationRequest,
  options: Xkc25CalibrateOptions,
): Promise<Xkc25CalibrationResult> {
  // Прошивка обрезает значение до своего предела; отправляем уже обрезанное, чтобы
  // тело запроса соответствовало документированному диапазону 0..5000.
  const timeoutMs = Math.min(request.timeout_ms, XKC25_MAX_DEVICE_TIMEOUT_MS);
  const body = { index: request.index, step: request.step, timeoutMs };

  const response = await client.withWrite(XKC25_WRITE_KEY, () =>
    client.request('POST', XKC25_CALIBRATE_PATH, body, {
      headers: { accept: 'application/json' },
    }),
  );

  if (response.status === 202) {
    const pending = parseVerifyPending(response, request);
    const verify = await pollVerifyReturn(client, request, options);
    if (verify.status === 'pending') {
      return {
        device_ok: true,
        completed: false,
        code: pending.code ?? 'ok',
        message: pending.message ?? 'Проверка возврата запущена',
        checklist: [],
        sensor: null,
        verify,
        hint: XKC25_VERIFY_PENDING_HINT,
        observed_at: new Date().toISOString(),
      };
    }
    return {
      device_ok: verify.ok ?? false,
      completed: verify.completed ?? false,
      code: verify.code ?? 'ok',
      message: verify.message ?? '',
      checklist: [],
      sensor: null,
      verify,
      observed_at: new Date().toISOString(),
    };
  }

  if (response.status >= 200 && response.status < 300) {
    const raw = parseCalibrationBody(response, request);
    return {
      ...calibrationResult(raw, 'ok'),
      verify: {
        status: 'done',
        index: request.index,
        ok: raw.ok,
        completed: raw.completed ?? false,
        code: raw.code ?? 'ok',
        message: raw.message ?? null,
        raw_level: raw.rawLevel ?? null,
      },
    };
  }

  throw calibrationHttpError(response, request);
}

/**
 * Опрос `verify-status`: первый запрос сразу после постановки, далее каждые `pollMs`.
 * Возвращает последнее наблюдение, не бросая на `pending`.
 */
async function pollVerifyReturn(
  client: DeviceClient,
  request: Xkc25CalibrationRequest,
  options: Xkc25CalibrateOptions,
): Promise<Xkc25VerifyView> {
  const now = options.now ?? Date.now;
  const sleep = options.sleep ?? defaultSleep;
  const pollMs = Math.max(1, options.pollMs ?? XKC25_POLL_INTERVAL_MS);
  const started = now();

  for (;;) {
    const raw = await readJson(client, XKC25_VERIFY_STATUS_PATH, xkc25VerifyStatusSchema);
    const view = toVerifyView(raw);
    if (view.status !== 'pending') return view;

    const remaining = request.timeout_ms - (now() - started);
    if (remaining <= 0) return view;
    await sleep(Math.min(pollMs, remaining));
  }
}

function parseCalibrationBody(
  response: HttpResponse,
  request: Xkc25CalibrationRequest,
): Xkc25CalibrationRaw {
  if (response.status < 200 || response.status >= 300) {
    throw calibrationHttpError(response, request);
  }

  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) {
    throw new OcError('device_error', 'Ответ на шаг калибровки не является JSON', {
      hint:
        'Похоже, прошивка другой версии или маршрут перехвачен. Включите SSVC_RAW_LOG=all и ' +
        'посмотрите сырой обмен; калибровка при этом могла уже выполниться — проверьте ' +
        'oc_get_xkc25_status перед повтором.',
      details: {
        status: response.status,
        path: XKC25_CALIBRATE_PATH,
        parse_error: parsed.error,
        body: bodyForDetails(response.body),
      },
    });
  }

  const result = xkc25CalibrationSchema.safeParse(parsed.value);
  if (!result.success) {
    throw new OcError(
      'device_error',
      'Ответ на шаг калибровки не соответствует ожидаемой схеме',
      {
        hint:
          'Ожидался объект {ok, completed, code, message, checklist, sensor}. Проверьте версию ' +
          'прошивки и details.issues / details.body. Не повторяйте шаг вслепую: он мог выполниться.',
        details: {
          status: response.status,
          path: XKC25_CALIBRATE_PATH,
          issues: result.error.issues.map((issue) => ({
            path: issue.path.join('.'),
            message: issue.message,
          })),
          body: bodyForDetails(response.body),
        },
      },
    );
  }
  return result.data;
}

function parseVerifyPending(
  response: HttpResponse,
  request: Xkc25CalibrationRequest,
): { status: string; index?: number; code?: string; message?: string } {
  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) {
    throw new OcError('device_error', 'Ответ 202 на проверку возврата не является JSON', {
      hint:
        'Проверка могла быть запущена. Не повторяйте POST вслепую: вызовите ' +
        'oc_xkc25_verify_status, чтобы узнать состояние.',
      details: {
        status: response.status,
        path: XKC25_CALIBRATE_PATH,
        index: request.index,
        parse_error: parsed.error,
        body: bodyForDetails(response.body),
      },
    });
  }
  const result = xkc25VerifyPendingSchema.safeParse(parsed.value);
  if (!result.success) {
    throw new OcError('device_error', 'Ответ 202 на проверку возврата не соответствует схеме', {
      hint:
        'Ожидался объект {status, index, code, message}. Проверка могла быть запущена: ' +
        'не повторяйте POST, а вызовите oc_xkc25_verify_status.',
      details: {
        status: response.status,
        path: XKC25_CALIBRATE_PATH,
        index: request.index,
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

function calibrationResult(
  raw: Xkc25CalibrationRaw,
  fallbackCode: string,
): Xkc25CalibrationResult {
  return {
    device_ok: raw.ok,
    completed: raw.completed ?? false,
    code: raw.code ?? fallbackCode,
    message: raw.message ?? '',
    checklist: raw.checklist ?? [],
    sensor: raw.sensor ?? null,
    observed_at: new Date().toISOString(),
  };
}

/**
 * Маппинг HTTP-отказа калибровки (§D.5, §C.7):
 *  - 404 `sensor_not_available` / `sensor_not_configured` — датчика нет → `not_found`;
 *  - 409 `unstable_signal` и другие коды состояния → `validation_failed` с кодом в details;
 *  - 500 `persist_failed` — не удалось сохранить калибровку → `device_error`;
 *  - прочее — общий маппинг транспорта.
 */
function calibrationHttpError(
  response: HttpResponse,
  request: Xkc25CalibrationRequest,
): OcError {
  const device = readDeviceError(response.body);

  if (response.status === 404) {
    if (device.code === 'sensor_not_available' || device.code === 'sensor_not_configured') {
      const notConfigured = device.code === 'sensor_not_configured';
      return new OcError(
        'not_found',
        notConfigured
          ? `Датчик уровня XKC-25 с номером ${request.index} не настроен`
          : `Датчик уровня XKC-25 с номером ${request.index} недоступен`,
        {
          hint: notConfigured
            ? 'Номер слота не совпадает с конфигурацией: возьмите index из oc_get_xkc25_status ' +
              'и проверьте, что датчик включён в oc_get_hardware_config.'
            : 'Подсистема XKC-25 не активна. Проверьте, что датчик включён в аппаратной ' +
              'конфигурации (oc_get_hardware_config) и подсистема xkc25_sensor поднята; после ' +
              'изменения конфигурации может потребоваться перезагрузка контроллера.',
          details: {
            status: response.status,
            path: XKC25_CALIBRATE_PATH,
            index: request.index,
            step: request.step,
            device_code: device.code,
            body: bodyForDetails(response.body),
          },
        },
      );
    }
    return httpError({
      status: response.status,
      body: response.body,
      method: 'POST',
      path: XKC25_CALIBRATE_PATH,
    });
  }

  if (response.status === 409) {
    const code = device.code ?? 'unknown';
    return new OcError('validation_failed', messageForCalibrationCode(code, request.step), {
      hint: hintForCalibrationCode(code),
      details: {
        status: response.status,
        path: XKC25_CALIBRATE_PATH,
        index: request.index,
        step: request.step,
        device_code: code,
        device_message: device.message,
        checklist: device.checklist,
        body: bodyForDetails(response.body),
      },
    });
  }

  if (response.status === 500) {
    return new OcError('device_error', 'Контроллер не смог сохранить калибровку датчика', {
      hint:
        'Прошивка ответила persist_failed: калибровка не записана. Проверьте журнал ' +
        'контроллера и свободное место в файловой системе; шаг можно повторить, когда ' +
        'причина устранена.',
      details: {
        status: response.status,
        path: XKC25_CALIBRATE_PATH,
        index: request.index,
        step: request.step,
        device_code: device.code,
        device_message: device.message,
        body: bodyForDetails(response.body),
      },
    });
  }

  if (response.status === 400) {
    return new OcError('validation_failed', 'Контроллер отклонил параметры калибровки', {
      hint:
        'Проверьте index (0..3) и step. Локальная проверка уже пропустила значения, поэтому ' +
        'вероятно расхождение с версией прошивки: посмотрите details.body.',
      details: {
        status: response.status,
        path: XKC25_CALIBRATE_PATH,
        index: request.index,
        step: request.step,
        device_code: device.code,
        body: bodyForDetails(response.body),
      },
    });
  }

  return httpError({
    status: response.status,
    body: response.body,
    method: 'POST',
    path: XKC25_CALIBRATE_PATH,
  });
}

interface DeviceErrorInfo {
  code: string | null;
  message: string | null;
  checklist: string[];
}

/** Код отказа у прошивки лежит то в `code`, то в `error` (§D.5). */
function readDeviceError(body: string): DeviceErrorInfo {
  const parsed = parseJsonBody(body);
  if (!parsed.ok || typeof parsed.value !== 'object' || parsed.value === null) {
    return { code: null, message: null, checklist: [] };
  }
  const record = parsed.value as Record<string, unknown>;
  const code =
    typeof record.code === 'string'
      ? record.code
      : typeof record.error === 'string'
        ? record.error
        : null;
  const message = typeof record.message === 'string' ? record.message : null;
  const checklist = Array.isArray(record.checklist)
    ? record.checklist.filter((item): item is string => typeof item === 'string')
    : [];
  return { code, message, checklist };
}

function messageForCalibrationCode(code: string, step: Xkc25Step): string {
  switch (code) {
    case 'unstable_signal':
      return 'Сигнал датчика нестабилен: уровень менялся во время калибровки';
    case 'no_external_pull':
      return 'На линии датчика нет внешней подтяжки';
    case 'not_reacted':
      return 'Датчик не реагирует на уровень жидкости';
    case 'not_calibrated':
      return 'Датчик не откалиброван: сначала выполните шаги без жидкости и с жидкостью';
    case 'verify_in_progress':
      return 'Проверка возврата уже выполняется';
    case 'verify_failed':
      return 'Проверка возврата не удалась';
    default:
      return `Шаг калибровки «${step}» отклонён контроллером (${code})`;
  }
}

function hintForCalibrationCode(code: string): string {
  switch (code) {
    case 'unstable_signal':
      return (
        'Убедитесь, что уровень жидкости не меняется, датчик закреплён, а линия не ' +
        'дребезжит; затем повторите шаг.'
      );
    case 'no_external_pull':
      return (
        'Проверьте подтяжку линии входа (внешний резистор или PCF8574): без неё уровень ' +
        'пина не определён. Исправьте схему и повторите шаг.'
      );
    case 'not_reacted':
      return (
        'Датчик не увидел изменения уровня. Проверьте, что жидкость действительно ' +
        'напротив датчика, и повторите шаг.'
      );
    case 'not_calibrated':
      return (
        'Сначала выполните шаги without_liquid и with_liquid, затем verify_return. ' +
        'Калибровка должна завершиться (completed:true) до проверки возврата.'
      );
    case 'verify_in_progress':
      return (
        'Вторая проверка не запускается: дождитесь результата через oc_xkc25_verify_status ' +
        'и не отправляйте POST повторно.'
      );
    case 'verify_failed':
      return (
        'Проверка возврата не прошла: уровень не вернулся к уровню «без жидкости». ' +
        'Проверьте датчик и жидкость, при необходимости откалибруйте заново.'
      );
    default:
      return (
        'Состояние датчика мешает шагу; файл калибровки не изменён. Проверьте подключение ' +
        'и повторите шаг, когда состояние стабилизируется.'
      );
  }
}
