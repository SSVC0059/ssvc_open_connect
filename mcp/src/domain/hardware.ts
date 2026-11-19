/**
 * Доменный слой «аппаратная конфигурация и сбои» (WP5, mcp-server-agents.md §9).
 *
 * Контракт устройства (mcp-contract-notes.md §B.10, §B.11, §C.5, §D.5):
 *  - `GET /rest/oc/hardware-config` → состояние конфигурации + `capabilities` + пробы + `needsReboot`;
 *  - `PUT /rest/oc/hardware-config` — ЧАСТИЧНЫЙ объект тех же ключей, ответ — та же структура,
 *    что GET; при 400 прошивка не применила ни одного поля;
 *  - `GET /rest/oc/hardware-faults` → `faults[]`, `count`, `i2cBusActive`, `i2cScan[]`.
 *
 * Что определяет реализацию:
 *
 * 1. Тело PUT — ровно аргумент `config`, без обёртки: обработчик прошивки читает ключи
 *    конфигурации с верхнего уровня (`OpenConnectHandler.cpp:406-476`). Обёртка сделала бы
 *    из настроек неизвестные ключи, то есть «успех без изменений».
 * 2. Состав проб зависит от сборки: проба приходит только для включённой функции, пробы
 *    реле — только при `PINOUT_USE_GPIO=0` (§B.10). Поэтому отсутствие `bmp581ProbeOk`
 *    или `relayPcf8574ProbeOk` — норма, а не ошибка схемы.
 * 3. `needsReboot` — вычисленный признак; если прошивка прислала только `pendingReboot`,
 *    используем его как запасной. Инструмент добавляет `hint` с `oc_restart`, не выполняя
 *    перезагрузку сам (§9): домен только сообщает, что изменения ждут рестарта.
 * 4. GET/PUT конфигурации не ретраятся: PUT идёт через `client.withWrite('hardware', …)`,
 *    чтобы две записи не перемешались; риск 1 проверяет инструмент.
 * 5. `faults` может отсутствовать (сбоев нет) — отдаём пустой массив; `i2cScan` может быть
 *    пустым (на шине никто не ответил) — это данные, а не ошибка.
 * 6. SPA-заглушка (`200 text/html`) — `feature_unsupported`: маршрута нет в прошивке.
 */

import type { DeviceClient } from '../device/DeviceClient.js';
import { parseJsonBody } from '../device/schemas/common.js';
import {
  hardwareConfigSchema,
  hardwareFaultsSchema,
  type HardwareConfigRaw,
  type HardwareFaultsRaw,
} from '../device/schemas/hardware.js';
import { OcError, httpError } from '../errors.js';
import { parseJsonResponse, readJson } from './readJson.js';

export const HARDWARE_CONFIG_PATH = '/rest/oc/hardware-config';
export const HARDWARE_FAULTS_PATH = '/rest/oc/hardware-faults';

/** Ключ очереди записей домена: конфигурация оборудования меняется последовательно. */
const WRITE_KEY = 'hardware';

/** Подсказка при `needs_reboot:true` — дословно по §9. */
export const REBOOT_HINT = 'изменения вступят в силу после oc_restart';

// ---------------------------------------------------------------------------
// Аппаратная конфигурация
// ---------------------------------------------------------------------------

/** Результаты проб I²C: поля появляются только для включённых функций (§B.10). */
export interface HardwareProbes {
  bmp581_probe_ok?: boolean;
  ds3231_probe_ok?: boolean;
  lcd1602_probe_ok?: boolean;
  relay_pcf8574_probe_ok?: boolean[];
  relay_hardware_ok?: boolean;
}

export interface HardwareConfigSnapshot {
  /** Конфигурация целиком в том виде, в каком её отдала прошивка (без пересборки). */
  config: Record<string, unknown>;
  /** `pinoutUseGpio`, `relayOptionsAvailable`; пустой объект, если прошивка не прислала. */
  capabilities: Record<string, unknown>;
  needs_reboot: boolean;
  probes: HardwareProbes;
  observed_at: string;
  /** Только при `needs_reboot:true`: изменения ждут `oc_restart`. */
  hint?: string;
}

/** Признак «нужна перезагрузка»: `needsReboot`, иначе сохранённый `pendingReboot`. */
function needsRebootOf(raw: HardwareConfigRaw): boolean {
  if (typeof raw.needsReboot === 'boolean') return raw.needsReboot;
  if (typeof raw.pendingReboot === 'boolean') return raw.pendingReboot;
  return false;
}

function probesOf(raw: HardwareConfigRaw): HardwareProbes {
  const probes: HardwareProbes = {};
  if (typeof raw.bmp581ProbeOk === 'boolean') probes.bmp581_probe_ok = raw.bmp581ProbeOk;
  if (typeof raw.ds3231ProbeOk === 'boolean') probes.ds3231_probe_ok = raw.ds3231ProbeOk;
  if (typeof raw.lcd1602ProbeOk === 'boolean') probes.lcd1602_probe_ok = raw.lcd1602ProbeOk;
  if (Array.isArray(raw.relayPcf8574ProbeOk)) {
    probes.relay_pcf8574_probe_ok = [...raw.relayPcf8574ProbeOk];
  }
  if (typeof raw.relayHardwareOk === 'boolean') probes.relay_hardware_ok = raw.relayHardwareOk;
  return probes;
}

/** Единое представление конфигурации для GET и PUT: прошивка отдаёт одну структуру. */
function toSnapshot(raw: HardwareConfigRaw): HardwareConfigSnapshot {
  return {
    config: { ...raw },
    capabilities:
      typeof raw.capabilities === 'object' && raw.capabilities !== null
        ? { ...raw.capabilities }
        : {},
    needs_reboot: needsRebootOf(raw),
    probes: probesOf(raw),
    observed_at: new Date().toISOString(),
  };
}

/** Читает `GET /rest/oc/hardware-config`. */
export async function getHardwareConfig(client: DeviceClient): Promise<HardwareConfigSnapshot> {
  const raw = await readJson(client, HARDWARE_CONFIG_PATH, hardwareConfigSchema);
  return toSnapshot(raw);
}

/**
 * Проверяет тело PUT: непустой JSON-объект.
 *
 * Пустой объект отвергаем локально: прошивка ответит 200 и ничего не изменит, а агент
 * решит, что настройки применены. Неизвестные ключи не фильтруем — новая прошивка может
 * знать больше, чем MCP-сервер (та же логика, что у порогов тревог WP4).
 */
export function normalizeHardwareConfigPatch(raw: unknown): Record<string, unknown> {
  if (typeof raw !== 'object' || raw === null || Array.isArray(raw)) {
    throw new OcError('validation_failed', 'Поле config должно быть JSON-объектом', {
      hint:
        'Передайте частичный объект настроек оборудования: {"pressureSensorEnabled": true, ' +
        '"lcd1602Enabled": false, …}. Массив и null не принимаются.',
      details: { field: 'config', reason: 'not_an_object' },
    });
  }
  if (Object.keys(raw).length === 0) {
    throw new OcError('validation_failed', 'Пустой объект настроек оборудования', {
      hint:
        'Передайте хотя бы один ключ конфигурации: пустой PUT ничего не меняет. Текущие ' +
        'настройки можно взять из oc_get_hardware_config.',
      details: { field: 'config', reason: 'empty_config' },
    });
  }
  return raw as Record<string, unknown>;
}

/**
 * Частично обновляет аппаратную конфигурацию.
 *
 * Тело — ровно аргумент `config` (без обёртки), запись сериализуется `withWrite('hardware')`.
 * Если ответ требует перезагрузки, в снимок добавляется `hint`; перезагрузку никто не
 * выполняет — это отдельный инструмент `oc_restart` (WP9).
 *
 * @throws OcError `validation_failed` (локальная проверка или 400 прошивки),
 *         `feature_unsupported` (404/SPA), коды транспорта.
 */
export async function updateHardwareConfig(
  client: DeviceClient,
  patch: unknown,
): Promise<HardwareConfigSnapshot> {
  const normalized = normalizeHardwareConfigPatch(patch);

  const raw = await client.withWrite(WRITE_KEY, async () => {
    const response = await client.request('PUT', HARDWARE_CONFIG_PATH, normalized, {
      headers: { accept: 'application/json' },
    });
    if (response.status >= 200 && response.status < 300) {
      return parseJsonResponse(response, hardwareConfigSchema, 'PUT', HARDWARE_CONFIG_PATH);
    }
    if (response.status === 400) throw invalidHardwareConfigError(response);
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'PUT',
      path: HARDWARE_CONFIG_PATH,
    });
  });

  const snapshot = toSnapshot(raw);
  if (snapshot.needs_reboot) snapshot.hint = REBOOT_HINT;
  return snapshot;
}

/** 400 PUT `/rest/oc/hardware-config`: `body_must_be_object` или `validation_failed` (§D.5). */
function invalidHardwareConfigError(response: { status: number; body: string }): OcError {
  const parsed = parseJsonBody(response.body);
  let deviceCode: string | null = null;
  let deviceMessage: string | null = null;
  if (parsed.ok && typeof parsed.value === 'object' && parsed.value !== null) {
    const record = parsed.value as { error?: unknown; message?: unknown };
    if (typeof record.error === 'string') deviceCode = record.error;
    if (typeof record.message === 'string') deviceMessage = record.message;
  }

  return new OcError(
    'validation_failed',
    deviceMessage === null
      ? 'Контроллер отклонил аппаратную конфигурацию'
      : `Контроллер отклонил аппаратную конфигурацию: ${deviceMessage}`,
    {
      hint:
        'Проверьте адреса I²C (допустимы 0x08…0x77), номера GPIO и привязку зон XKC-25. ' +
        'При 400 прошивка не применила ни одного поля: исправьте значения и повторите.',
      details: {
        status: response.status,
        method: 'PUT',
        path: HARDWARE_CONFIG_PATH,
        device_error: deviceCode,
        body: response.body.slice(0, 2_000),
      },
    },
  );
}

// ---------------------------------------------------------------------------
// Аппаратные сбои
// ---------------------------------------------------------------------------

export interface HardwareFault {
  /** `none` | `i2c_nack` | `i2c_bus_down` | `device_not_present` | `uart_link_lost` | `unknown`. */
  code: string;
  /** Русская подпись кода; null, если прошивка её не прислала. */
  code_label: string | null;
  /** 7-битный I²C-адрес (0 для `ssvc_uart`); null, если адрес не относится к сбою. */
  address: number | null;
  /** Роль устройства: `i2c_bus`, `bmp581`, `lcd1602`, `pcf8574_portN`, `ssvc_uart`, … */
  role: string | null;
}

export interface HardwareFaultsSnapshot {
  faults: HardwareFault[];
  /** `count` прошивки, иначе длина `faults[]` (отсутствие сбоев — 0). */
  count: number;
  i2c_bus_active: boolean | null;
  /** Ответившие адреса I²C; пустой массив — на шине никто не ответил. */
  i2c_scan: number[];
  observed_at: string;
}

/** Читает `GET /rest/oc/hardware-faults`; отсутствие `faults` — это «сбоев нет». */
export async function getHardwareFaults(
  client: DeviceClient,
): Promise<HardwareFaultsSnapshot> {
  const raw: HardwareFaultsRaw = await readJson(
    client,
    HARDWARE_FAULTS_PATH,
    hardwareFaultsSchema,
  );

  const faults: HardwareFault[] = (raw.faults ?? []).map((fault) => ({
    code: fault.code,
    code_label: fault.codeLabel ?? null,
    address: fault.address ?? null,
    role: fault.role ?? null,
  }));

  return {
    faults,
    count: raw.count ?? faults.length,
    i2c_bus_active: raw.i2cBusActive ?? null,
    i2c_scan: raw.i2cScan ?? [],
    observed_at: new Date().toISOString(),
  };
}
