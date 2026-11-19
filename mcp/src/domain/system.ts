/**
 * Доменный слой подсистем и состояния контроллера (WP9, mcp-server-agents.md §13;
 * контракт прошивки — mcp-contract-notes.md §B.8, §B.18, §D).
 *
 * Ключевые решения:
 *
 *  1. `PUT /rest/subsystem` — **долгая** операция: включение `telegram_bot`/`vk_bot`
 *     запускает сетевую подсистему и может занять больше минуты. Таймаут запроса
 *     120 с (а не 10 с по умолчанию): иначе инструмент обрывал бы включение,
 *     которое на устройстве уже началось.
 *  2. `success:false` — это **не** исключение: устройство вернуло отчёт об ошибке
 *     по конкретной подсистеме, и агент должен его увидеть. Инструмент всё равно
 *     перечитывает `GET /rest/subsystem`, потому что часть подсистем могла
 *     переключиться (это же требуется и при успехе).
 *  3. Состав карты подсистем зависит от сборки прошивки, поэтому ключи не
 *     фиксируются: наружу уходит фактический словарь `bool`.
 *  4. `/rest/systemStatus` отдаёт `free_heap`/`firmware_version`/`cpu_type` и ещё
 *     два десятка полей. Наружу — нормализованный `system` с каноническими
 *     именами и псевдонимами `heap`/`firmware`/`chip` из §13 (старые имена
 *     принимаются и на входе, если прошивка когда-нибудь их вернёт).
 *  5. `POST /rest/restart` отвечает 200, после чего контроллер уходит в перезагрузку.
 *     Инструмент возвращает `hint` про 20–40 секунд недоступности и `/readyz`:
 *     ждать перезагрузку внутри вызова бессмысленно — клиент потеряет соединение.
 */

import type { DeviceClient } from '../device/DeviceClient.js';
import {
  subsystemMapSchema,
  subsystemUpdateResponseSchema,
  systemStatusBodySchema,
  type SubsystemErrorEntry,
  type SubsystemMap,
  type SystemInfo,
  type SystemStatusBody,
} from '../device/schemas/system.js';
import { OcError, httpError, truncateBody } from '../errors.js';
import { parseJsonResponse, readJson } from './readJson.js';

export const SUBSYSTEM_PATH = '/rest/subsystem';
export const SYSTEM_STATUS_PATH = '/rest/systemStatus';
export const RESTART_PATH = '/rest/restart';

/**
 * Таймаут `PUT /rest/subsystem`. Telegram/VK включаются дольше 60 с
 * (mcp-server.md §2.5 п.12), поэтому 120 000 мс, а не общий 10-секундный.
 */
export const SUBSYSTEM_TIMEOUT_MS = 120_000;

/** Таймаут `POST /rest/restart`: ответ приходит до перезагрузки, долго ждать нечего. */
export const RESTART_TIMEOUT_MS = 30_000;

/** Подсказка после успешной перезагрузки — дословно по §13. */
export const RESTART_HINT =
  'Контроллер перезагружается: 20–40 секунд он будет недоступен, затем проверьте /readyz.';

export interface SubsystemsSnapshot {
  systems: SubsystemMap;
  observed_at: string;
}

export interface SubsystemFailure {
  /** Имя подсистемы из отчёта устройства; `null`, если прошивка прислала строку. */
  subsystem: string | null;
  message: string;
}

export interface SetSubsystemResult {
  name: string;
  enabled: boolean;
  /** `true` только если прошивка подтвердила переключение. */
  success: boolean;
  errors: SubsystemFailure[];
  /** Перечитанная карта; `null`, если после записи прочитать её не удалось. */
  subsystems: SubsystemMap | null;
  /** Причина, по которой карта не перечитана (если была). */
  warning: string | null;
  observed_at: string;
}

export interface SystemStatusSnapshot {
  system: SystemInfo;
  observed_at: string;
}

export interface RestartResult {
  ok: true;
  hint: string;
  observed_at: string;
}

export interface SetSubsystemInput {
  name: unknown;
  enabled: unknown;
}

/** `GET /rest/subsystem` — фактическая карта «подсистема → включена». */
export async function getSubsystems(client: DeviceClient): Promise<SubsystemsSnapshot> {
  const systems = await readJson(client, SUBSYSTEM_PATH, subsystemMapSchema);
  return { systems, observed_at: new Date().toISOString() };
}

/**
 * Переключает подсистему и перечитывает карту.
 *
 * Запись идёт через `client.withWrite('subsystems', …)`: это изменение состояния
 * устройства, и оно не должно перемешиваться с другими записями.
 *
 * @throws OcError `validation_failed` (имя/enabled), `feature_unsupported` (SPA/маршрут),
 *         `device_timeout` (не уложились в 120 с), `device_error`.
 */
export async function setSubsystem(
  client: DeviceClient,
  input: SetSubsystemInput,
  opts: { timeoutMs?: number } = {},
): Promise<SetSubsystemResult> {
  const name = normalizeSubsystemName(input.name);
  if (typeof input.enabled !== 'boolean') {
    throw new OcError('validation_failed', 'Поле enabled должно быть true или false', {
      hint:
        'Прошивка трактует не-bool значение как false и выключает подсистему, поэтому ' +
        'сервер требует явный boolean.',
      details: { name, enabled: input.enabled ?? null },
    });
  }
  const enabled = input.enabled;
  const timeoutMs = opts.timeoutMs ?? SUBSYSTEM_TIMEOUT_MS;

  return client.withWrite('subsystems', async () => {
    const response = await client.request('PUT', SUBSYSTEM_PATH, { [name]: enabled }, {
      timeoutMs,
      headers: { accept: 'application/json' },
    });
    const body = parseJsonResponse(response, subsystemUpdateResponseSchema, 'PUT', SUBSYSTEM_PATH);

    const success = typeof body.success === 'boolean' ? body.success : true;
    const errors = normalizeSubsystemErrors(body.errors);

    // Перечитываем всегда: даже при success:false часть подсистем могла примениться,
    // а агенту нужно фактическое состояние, а не эхо запроса.
    let systems: SubsystemMap | null = null;
    let warning: string | null = null;
    try {
      systems = (await getSubsystems(client)).systems;
    } catch (error) {
      warning =
        'Команда переключения отправлена, но перечитать состояние подсистем не удалось: ' +
        (error instanceof Error ? error.message : String(error));
    }

    return {
      name,
      enabled,
      success,
      errors,
      subsystems: systems,
      warning,
      observed_at: new Date().toISOString(),
    };
  });
}

/**
 * `GET /rest/systemStatus` в нормализованном виде.
 *
 * @throws OcError `feature_unsupported` (маршрут/SPA), `device_error` (схема),
 *         `auth_failed`/`forbidden_admin_required` (доступ).
 */
export async function getSystemStatus(client: DeviceClient): Promise<SystemStatusSnapshot> {
  const raw = await readJson(client, SYSTEM_STATUS_PATH, systemStatusBodySchema);
  return { system: mapSystemStatus(raw), observed_at: new Date().toISOString() };
}

/**
 * Приводит ответ прошивки к нормализованному `SystemInfo`.
 *
 * Псевдонимы `heap`/`firmware`/`chip` оставлены намеренно: §13 описывает именно
 * их, а прошивка отдаёт `free_heap`/`firmware_version`/`cpu_type`. Оба имени
 * доступны агенту, значения всегда одинаковые.
 */
export function mapSystemStatus(raw: SystemStatusBody): SystemInfo {
  const freeHeap = firstNumber(raw.free_heap, raw.heap);
  const firmware = raw.firmware_version ?? raw.firmware ?? null;
  const chip = raw.cpu_type ?? raw.esp_platform ?? null;

  return {
    free_heap: freeHeap,
    heap: freeHeap,
    min_free_heap: firstNumber(raw.min_free_heap),
    used_heap: firstNumber(raw.used_heap),
    total_heap: firstNumber(raw.total_heap),
    max_alloc_heap: firstNumber(raw.max_alloc_heap),
    free_psram: firstNumber(raw.free_psram),
    used_psram: firstNumber(raw.used_psram),
    psram_size: firstNumber(raw.psram_size),
    uptime: firstNumber(raw.uptime),
    firmware_version: firmware,
    firmware,
    esp_platform: raw.esp_platform ?? null,
    cpu_type: raw.cpu_type ?? null,
    chip,
    cpu_freq_mhz: firstNumber(raw.cpu_freq_mhz),
    cpu_rev: firstNumber(raw.cpu_rev),
    cpu_cores: firstNumber(raw.cpu_cores),
    flash_chip_size: firstNumber(raw.flash_chip_size),
    flash_chip_speed: firstNumber(raw.flash_chip_speed),
    sketch_size: firstNumber(raw.sketch_size),
    free_sketch_space: firstNumber(raw.free_sketch_space),
    fs_total: firstNumber(raw.fs_total),
    fs_used: firstNumber(raw.fs_used),
    core_temp: firstNumber(raw.core_temp),
    sdk_version: raw.sdk_version ?? null,
    arduino_version: raw.arduino_version ?? null,
    cpu_reset_reason: raw.cpu_reset_reason ?? null,
  };
}

/**
 * `POST /rest/restart` → 200. Возвращает `ok` и подсказку, что делать дальше.
 *
 * @throws OcError `feature_unsupported` (маршрут/SPA), `auth_failed`
 *         (перезагрузка требует admin), `device_timeout`.
 */
export async function restartController(
  client: DeviceClient,
  opts: { timeoutMs?: number } = {},
): Promise<RestartResult> {
  // Как и остальные записи, перезагрузка идёт через общую очередь: она не должна
  // пересекаться с записью настроек или профиля, идущей параллельно.
  return client.withWrite('restart', async () => {
    const response = await client.request('POST', RESTART_PATH, undefined, {
      headers: { accept: 'application/json, text/plain, */*' },
      timeoutMs: opts.timeoutMs ?? RESTART_TIMEOUT_MS,
    });

    if (response.status >= 200 && response.status < 300) {
      return { ok: true, hint: RESTART_HINT, observed_at: new Date().toISOString() };
    }
    if (response.status === 404) {
      throw new OcError('feature_unsupported', 'Прошивка не отдаёт POST /rest/restart', {
        hint:
          'Маршрут перезагрузки регистрируется только в сборке с сервисом перезагрузки. ' +
          'Проверьте версию OpenConnect; повтор запроса не поможет.',
        details: {
          status: response.status,
          body: truncateBody(response.body),
        },
      });
    }
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'POST',
      path: RESTART_PATH,
    });
  });
}

function normalizeSubsystemName(raw: unknown): string {
  if (typeof raw !== 'string') {
    throw new OcError('validation_failed', 'Поле name должно быть строкой с именем подсистемы', {
      hint: 'Имя подсистемы — ключ карты oc_get_subsystems, например "thermal".',
      details: { name_type: typeof raw },
    });
  }
  const name = raw.trim();
  if (name === '') {
    throw new OcError('validation_failed', 'Имя подсистемы не может быть пустым', {
      hint: 'Возьмите имя из карты oc_get_subsystems, например "thermal", "telegram_bot".',
      details: { name: raw },
    });
  }
  if (name.length > 64) {
    throw new OcError('validation_failed', 'Имя подсистемы длиннее 64 символов', {
      hint: 'Возьмите точное имя из карты oc_get_subsystems.',
      details: { name_length: name.length, max_length: 64 },
    });
  }
  if (!/^[A-Za-z0-9_]+$/.test(name)) {
    throw new OcError('validation_failed', 'Имя подсистемы содержит недопустимые символы', {
      hint: 'Допустимы латиница, цифры и подчёркивание; возьмите имя из oc_get_subsystems.',
      details: { name },
    });
  }
  return name;
}

function normalizeSubsystemErrors(entries: SubsystemErrorEntry[] | undefined): SubsystemFailure[] {
  if (entries === undefined) return [];
  return entries.map((entry) => {
    if (typeof entry === 'string') return { subsystem: null, message: entry };
    return {
      subsystem: entry.subsystem ?? null,
      message: entry.message ?? '',
    };
  });
}

/** Первое числовое значение из списка; `null`, если чисел нет (или все `null`). */
function firstNumber(...values: (number | null | undefined)[]): number | null {
  for (const value of values) {
    if (typeof value === 'number' && Number.isFinite(value)) return value;
  }
  return null;
}
