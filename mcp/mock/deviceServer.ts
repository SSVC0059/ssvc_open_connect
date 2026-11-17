/**
 * Мок-устройство OpenConnect (WP11, §15): HTTP-двойник прошивки для e2e и ручных прогонов.
 *
 * Зачем: `test/fakeHttp.ts` подменяет транспорт в памяти и годится для доменных тестов,
 * но не проверяет настоящий путь «MCP-клиент → HTTP → MCP-сервер → HTTP → устройство».
 * Этот файл — отдельный HTTP-сервер с теми же маршрутами, что у прошивки (mcp-server.md
 * §2.4, фикстуры §2.4/§F), поэтому e2e проходит весь стек по сети, на порту 0.
 *
 * Принципы:
 *  - никаких новых зависимостей: только `node:http` и стандартная библиотека;
 *  - данные правдоподобные и согласованы между маршрутами (настройки меняются только
 *    через PUT, пороги — через POST, команды журналируются с `commandId`);
 *  - SPA-заглушки нет: мок отвечает по контракту, а не «как получится»; неизвестный
 *    маршрут — `404` JSON, чтобы «функция не поддерживается» было видно честно;
 *  - стадия процесса меняется сама через `MOCK_STAGE_SWITCH_MS` (по умолчанию 1.5 с),
 *    чтобы `oc_wait_for_change` действительно дождался изменения, а не «совпал сразу».
 *
 * Переменные окружения (все необязательны):
 *  - `MOCK_HOST` (127.0.0.1), `MOCK_PORT` (8099) — где слушать;
 *  - `MOCK_SECURITY=off` — прошивка без аутентификации: `/rest/signIn` отвечает `404`,
 *    и клиент работает анонимно (так ведёт себя не-secure сборка);
 *  - `MOCK_USER`/`MOCK_PASSWORD` — если заданы, `signIn` проверяет учётные данные;
 *  - `MOCK_STAGE_SWITCH_MS` (1500) — через сколько миллисекунд после старта стадия
 *    сменится с `waiting` на `heads`; `0` — не менять вовсе.
 *
 * Запуск: `npm run mock` (сборка в `.test-dist` + `node .test-dist/mock/deviceServer.js`).
 */

import { createServer, type IncomingMessage, type Server, type ServerResponse } from 'node:http';
import { pathToFileURL } from 'node:url';

/** Токен, который мок выдаёт на `POST /rest/signIn` (не секрет: это тестовый двойник). */
export const MOCK_ACCESS_TOKEN = 'mock-access-token';

/** Предел тела запроса: прошивка тоже не принимает гигантские тела. */
const MAX_BODY_BYTES = 256 * 1024;

// ---------------------------------------------------------------------------
// Фикстуры ответов (формы — по mcp-server.md §2.4 и §F, значения правдоподобные)
// ---------------------------------------------------------------------------

const INFO_BODY = {
  ssvc: { version: '2.3.30', api: '1.10', api_code: 110, mode: 'late_heads' },
  oc: {
    version: '0.2.9.2',
    is_support_api: true,
    is_support_release: true,
    api_min: '1.7',
    api_target: '1.10',
    api_compatibility: 'supported',
    api_features: [
      { name: 'base', min_api: '1.7', available: true },
      { name: 'formula', min_api: '1.7', available: true },
      { name: 'predec', min_api: '1.10', available: false },
      { name: 'get_log', min_api: '1.10', available: true },
      { name: 'extended_heads', min_api: '1.10', available: true },
    ],
  },
};

const FEATURES_BODY: Record<string, unknown> = {
  security: true,
  mqtt: true,
  ntp: true,
  telegram: false,
  vk: false,
  firmware_version: '0.2.9.2',
  firmware_name: 'SSVC-Open-Connect',
};

const SENSOR_BODY = {
  tank: { '28FF64A1B2C3D4E5': { v: 78.6, u: '°C', type: 'DS18B20' } },
  outdoor: { '28FF64A1B2C3D4E9': { v: 21.4, u: '°C', type: 'DS18B20' } },
  receiver_tank_1: { '28FF64A1B2C3D4E6': { v: 1, u: '', type: 'level' } },
};

const INITIAL_SETTINGS: Record<string, unknown> = {
  heads: [120, 3000],
  hearts: [96, 2500],
  late_heads: [150, 1800],
  tails: [98, 1000],
  hyst: 0.5,
  decrement: 2,
  tank_mmhg: 766.9,
  start_delay: 30,
  formula: 1,
  predec: false,
  parallel: 0,
  sound: true,
};

const INITIAL_ZONES: Record<string, number> = {
  '28FF64A1B2C3D4E5': 4, // tank
  '28FF64A1B2C3D4E6': 7, // receiver_tank_1
  '28FF64A1B2C3D4E9': 5, // outdoor
};

const INITIAL_ALARMS: Record<string, Record<string, unknown>> = {
  '28FF64A1B2C3D4E5': { enabled: true, min: 0, dangerous: 80, critical: 100 },
  '28FF64A1B2C3D4E9': { enabled: false, min: -20, dangerous: 40, critical: 60 },
};

const FILES_BODY = [
  {
    name: 'config',
    type: 'directory',
    children: [
      { name: 'app.json', type: 'file', size: 128 },
      { name: 'profiles.json', type: 'file', size: 640 },
    ],
  },
  { name: '12.CSV', type: 'file', size: 2048 },
];

const SUBSYSTEM_BODY: Record<string, boolean> = {
  i2c_bus: true,
  thermal: true,
  atmospheric: true,
  settings: true,
  telegram_bot: false,
  vk_bot: false,
};

const HARDWARE_CONFIG_BODY: Record<string, unknown> = {
  schemaVersion: 3,
  pressureSensorEnabled: true,
  bmp581I2cAddress: 119,
  userRelayPcfEnabled: false,
  relayPcf8574Addresses: [],
  rtcEnabled: true,
  ds3231I2cAddress: 104,
  oledEnabled: false,
  lcd1602Enabled: true,
  lcd1602I2cAddress: 39,
  xkc25Sensors: [],
  xkc25AutoPause: true,
  xkc25AbortRectification: false,
  xkc25TriggerAction: 1,
  xkc25SharedTankIndex: 0,
  xkc25StartGuardMode: 2,
  bmp581ProbeOk: true,
  ds3231ProbeOk: true,
  lcd1602ProbeOk: true,
  relayHardwareOk: false,
  needsReboot: false,
};

const HARDWARE_FAULTS_BODY = {
  faults: [
    {
      code: 'i2c_nack',
      codeLabel: 'Нет ответа по I²C',
      address: 0x77,
      role: 'bmp581',
    },
  ],
  count: 1,
  i2cBusActive: true,
  i2cScan: [0x27, 0x68],
};

const XKC25_STATUS_BODY = {
  sensors: [
    {
      index: 0,
      enabled: true,
      pinSource: 0,
      gpio: 4,
      pinLabel: 'GPIO 4',
      zone: 'receiver_tank_1',
      calibrated: true,
      live: true,
      rawLevel: 1,
      filled: false,
    },
    {
      index: 1,
      enabled: false,
      pinSource: 0,
      gpio: 5,
      pinLabel: 'GPIO 5',
      zone: 'receiver_tank_2',
      calibrated: false,
      live: false,
      rawLevel: 0,
      filled: false,
    },
  ],
  triggerAction: 1,
  sharedTankIndex: 0,
  startGuardMode: 2,
  blockStart: false,
  blockReasons: [],
  warnings: [],
};

const PROFILES_BODY = [
  { id: 'p1', name: 'Спирт-сырец', createdAt: '2026-01-12T09:00:00Z', isApplied: true },
  { id: 'p2', name: 'Фруктовая брага', createdAt: '2026-02-03T18:30:00Z', isApplied: false },
];

const PROFILE_CONTENT: Record<string, Record<string, unknown>> = {
  p1: { name: 'Спирт-сырец', settings: { hyst: 0.5, decrement: 2 }, fractions: ['heads', 'hearts'] },
  p2: { name: 'Фруктовая брага', settings: { hyst: 0.7, decrement: 3 }, fractions: ['heads', 'tails'] },
};

const LOGS_CSV = [
  'timestamp;stage;alc;tank_mmhg',
  '2026-01-12T09:00:01;heads;93.4;766.9',
  '2026-01-12T09:00:03;heads;93.2;766.8',
  '2026-01-12T09:00:05;hearts;92.9;766.7',
].join('\r\n');

// ---------------------------------------------------------------------------
// Состояние мока
// ---------------------------------------------------------------------------

export interface MockDeviceOptions {
  /** Порт; `0` — выбрать свободный (так делают тесты). */
  port?: number;
  host?: string;
  /** false — `/rest/signIn` отвечает 404, запросы идут без токена (не-secure сборка). */
  security?: boolean;
  /** Если задано, `signIn` требует именно такие учётные данные. */
  user?: string;
  password?: string;
  /** Через сколько миллисекунд после старта стадия станет `heads`; 0 — не менять. */
  stageSwitchMs?: number;
}

export interface MockDeviceState {
  settings: Record<string, unknown>;
  zones: Record<string, number>;
  alarms: Record<string, Record<string, unknown>>;
  subsystems: Record<string, boolean>;
  hardwareConfig: Record<string, unknown>;
  /** Журнал команд: `commandId` → запись (как кольцо прошивки, но без вытеснения). */
  commands: Map<number, Record<string, unknown>>;
  /** Сколько раз запросили `GET /rest/logs/{id}`: первый ответ — 202, второй — CSV. */
  logsPolls: number;
  profileList: typeof PROFILES_BODY;
  activeProfileId: string;
  stage: string;
}

export interface MockDevice {
  server: Server;
  state: MockDeviceState;
  address: { host: string; port: number };
  /** Базовый URL без завершающего слэша — его подставляют в `SSVC_DEVICE_URL`. */
  url: string;
  close(): Promise<void>;
}

interface RouteMatch {
  method: string;
  path: string;
  url: URL;
  req: IncomingMessage;
  res: ServerResponse;
  params: string[];
}

// ---------------------------------------------------------------------------
// HTTP-помощники
// ---------------------------------------------------------------------------

function sendJson(res: ServerResponse, status: number, body: unknown): void {
  const text = JSON.stringify(body);
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'content-length': Buffer.byteLength(text, 'utf8').toString(),
  });
  res.end(text);
}

function sendText(
  res: ServerResponse,
  status: number,
  body: string,
  contentType = 'text/plain; charset=utf-8',
  extra: Record<string, string> = {},
): void {
  res.writeHead(status, {
    'content-type': contentType,
    'content-length': Buffer.byteLength(body, 'utf8').toString(),
    ...extra,
  });
  res.end(body);
}

function readBody(req: IncomingMessage): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let size = 0;
    req.on('data', (chunk: Buffer) => {
      size += chunk.length;
      if (size > MAX_BODY_BYTES) {
        reject(new Error('body too large'));
        req.destroy();
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    req.on('error', reject);
  });
}

/** Тело JSON-объекта; пустое/битое тело — `null` (маршрут сам решит, что ответить). */
function parseBodyObject(text: string): Record<string, unknown> | null {
  if (text.trim() === '') return null;
  try {
    const value: unknown = JSON.parse(text);
    if (typeof value !== 'object' || value === null || Array.isArray(value)) return null;
    return value as Record<string, unknown>;
  } catch {
    return null;
  }
}

function headerOf(req: IncomingMessage, name: string): string | undefined {
  const value = req.headers[name.toLowerCase()];
  return Array.isArray(value) ? value.join(', ') : value;
}

// ---------------------------------------------------------------------------
// Создание мока
// ---------------------------------------------------------------------------

/**
 * Создаёт мок-устройство (без слушания). Слушание — `listen`/`startMockDevice`,
 * чтобы тесты занимали порт 0 и не зависели от занятых портов.
 */
export function createMockDevice(opts: MockDeviceOptions = {}): {
  server: Server;
  state: MockDeviceState;
  listen(port?: number, host?: string): Promise<{ host: string; port: number }>;
  close(): Promise<void>;
} {
  const security = opts.security ?? true;
  const switchMs = opts.stageSwitchMs ?? 1_500;
  const startedAt = Date.now();

  const state: MockDeviceState = {
    settings: { ...INITIAL_SETTINGS },
    zones: { ...INITIAL_ZONES },
    alarms: JSON.parse(JSON.stringify(INITIAL_ALARMS)) as Record<string, Record<string, unknown>>,
    subsystems: { ...SUBSYSTEM_BODY },
    hardwareConfig: { ...HARDWARE_CONFIG_BODY },
    commands: new Map<number, Record<string, unknown>>(),
    logsPolls: 0,
    profileList: PROFILES_BODY,
    activeProfileId: 'p1',
    stage: 'waiting',
  };

  let commandSeq = 0;

  /** millis() «прошивки»: uptime мока — как `millis()` устройства в реальных ответах. */
  const uptime = (): number => Date.now() - startedAt;

  const currentStage = (): string => {
    if (switchMs > 0 && uptime() >= switchMs) state.stage = 'heads';
    return state.stage;
  };

  const statusBody = (): Record<string, unknown> => {
    const stage = currentStage();
    return {
      stage,
      status: stage === 'waiting' ? 'waiting' : 'running',
      startTime: stage === 'waiting' ? '' : '2026-01-12T09:00:00Z',
      endTime: '',
      stages: {
        waiting: stage === 'waiting' ? 'running' : 'finished',
        heads: stage === 'heads' ? 'running' : 'waiting',
        hearts: 'waiting',
        tails: 'waiting',
      },
      uartConnectionError: false,
    };
  };

  const telemetryBody = (): Record<string, unknown> => ({
    type: currentStage() === 'heads' ? 'heads' : 'waiting',
    pid: 42,
    tank_mmhg: 766.9,
    stop: false,
    stops: 0,
    alc: 93.4,
    decrement: 2,
    common: {
      mmhg: '766.9',
      tp1: '78.6',
      tp2: '77.1',
      tp3: '21.4',
      relay: '0',
      signal: '100',
    },
    volume: { heads: 1200, hearts: 800, tails: 300 },
  });

  /** Авторизация: как у прошивки — Bearer на всё, кроме `signIn`/`healthz`. */
  const authorized = (req: IncomingMessage): boolean => {
    if (!security) return true;
    const header = headerOf(req, 'authorization') ?? '';
    return header === `Bearer ${MOCK_ACCESS_TOKEN}`;
  };

  const notFound = (res: ServerResponse, path: string, method: string): void => {
    sendJson(res, 404, { error: 'not_found', path, method });
  };

  const handle = async (match: RouteMatch): Promise<void> => {
    const { req, res, url, path } = match;

    // --- Служебное ---------------------------------------------------------
    if (path === '/healthz') {
      sendJson(res, 200, { ok: true, service: 'mock-openconnect', uptime_ms: uptime() });
      return;
    }
    if (path === '/rest/signIn' && match.method === 'POST') {
      if (!security) {
        // Не-secure сборка: эндпоинта логина нет, клиент работает анонимно.
        notFound(res, path, match.method);
        return;
      }
      const body = parseBodyObject(await readBody(req));
      const user = typeof body?.username === 'string' ? body.username : '';
      const password = typeof body?.password === 'string' ? body.password : '';
      if (user === '' || password === '') {
        sendJson(res, 400, { error: 'bad_request' });
        return;
      }
      if (opts.user !== undefined && opts.user !== user) {
        sendJson(res, 401, { error: 'invalid_credentials' });
        return;
      }
      if (opts.password !== undefined && opts.password !== password) {
        sendJson(res, 401, { error: 'invalid_credentials' });
        return;
      }
      sendJson(res, 200, { access_token: MOCK_ACCESS_TOKEN });
      return;
    }

    // Всё остальное под /rest требует токена (если security включена).
    if (path.startsWith('/rest/') && !authorized(req)) {
      sendJson(res, 401, { error: 'unauthorized' });
      return;
    }
    if (path === '/rest/verifyAuthorization' && match.method === 'GET') {
      sendJson(res, 200, { ok: true, user: 'admin' });
      return;
    }
    if (path === '/rest/healthz') {
      sendJson(res, 200, { ok: true });
      return;
    }

    // --- Информация, состояние, телеметрия --------------------------------
    if (path === '/rest/oc/info' && match.method === 'GET') {
      sendJson(res, 200, INFO_BODY);
      return;
    }
    if (path === '/rest/features' && match.method === 'GET') {
      sendJson(res, 200, FEATURES_BODY);
      return;
    }
    if (path === '/rest/telemetry' && match.method === 'GET') {
      sendJson(res, 200, {
        telemetry: telemetryBody(),
        status: statusBody(),
        lastUpdate: uptime(),
      });
      return;
    }
    if (path === '/rest/oc/snapshot' && match.method === 'GET') {
      sendJson(res, 200, {
        observed_at: uptime(),
        lastUpdate: uptime(),
        info: INFO_BODY,
        telemetry: telemetryBody(),
        status: statusBody(),
        sensor: SENSOR_BODY,
        faults: HARDWARE_FAULTS_BODY,
        partial: [],
      });
      return;
    }
    if (path === '/rest/sensor' && match.method === 'GET') {
      sendJson(res, 200, SENSOR_BODY);
      return;
    }

    // --- Настройки --------------------------------------------------------
    if (path === '/rest/settings' && match.method === 'GET') {
      sendJson(res, 200, { ssvcSettings: state.settings });
      return;
    }
    if (path === '/rest/settings' && match.method === 'PUT') {
      const body = parseBodyObject(await readBody(req));
      if (body === null) {
        sendJson(res, 400, { error: 'Body must be a JSON object' });
        return;
      }
      const ignored: string[] = [];
      const acceptedKeys: string[] = [];
      for (const [key, value] of Object.entries(body)) {
        if (key in state.settings) {
          state.settings[key] = value;
          acceptedKeys.push(key);
        } else {
          ignored.push(key);
        }
      }
      sendJson(res, 200, {
        success: true,
        accepted: acceptedKeys.length,
        ignored,
        skipped: [],
      });
      return;
    }

    // --- Зоны, датчики, пороги -------------------------------------------
    if (path === '/rest/zones' && match.method === 'GET') {
      sendJson(res, 200, { zones: state.zones });
      return;
    }
    if (path === '/rest/zones' && match.method === 'POST') {
      const body = parseBodyObject(await readBody(req));
      if (body === null || typeof body.zones !== 'object' || body.zones === null) {
        sendJson(res, 400, { error: 'zones must be an object' });
        return;
      }
      const zones: Record<string, number> = {};
      for (const [address, zone] of Object.entries(body.zones as Record<string, unknown>)) {
        if (typeof zone !== 'number') {
          sendJson(res, 400, { error: 'zone must be a number', address });
          return;
        }
        zones[address] = zone;
      }
      state.zones = zones;
      sendJson(res, 200, { success: true });
      return;
    }
    if (path === '/rest/sensors/zone' && match.method === 'PUT') {
      const address = (url.searchParams.get('address') ?? '').toUpperCase();
      const zone = (url.searchParams.get('zone') ?? '').toLowerCase();
      if (address === '' || zone === '') {
        sendJson(res, 400, { error: 'address and zone are required' });
        return;
      }
      const zoneId = Object.entries({
        unknown: 0,
        inlet_water: 1,
        outlet_water: 2,
        act: 3,
        tank: 4,
        outdoor: 5,
        receiver_tank_1: 7,
        receiver_tank_2: 8,
        receiver_tank_3: 9,
      }).find(([name]) => name === zone)?.[1];
      if (zoneId === undefined) {
        sendJson(res, 400, { error: 'unknown zone', received_zone: zone });
        return;
      }
      if (state.zones[address] === zoneId) {
        // Идемпотентная запись: прошивка отвечает 404 при отсутствии изменений.
        notFound(res, path, match.method);
        return;
      }
      state.zones[address] = zoneId;
      sendJson(res, 200, { success: true });
      return;
    }
    if (path === '/rest/alarms' && match.method === 'GET') {
      sendJson(res, 200, { thresholds: state.alarms });
      return;
    }
    if (path === '/rest/alarms' && match.method === 'POST') {
      const body = parseBodyObject(await readBody(req));
      if (body === null || typeof body.thresholds !== 'object' || body.thresholds === null) {
        sendJson(res, 400, { error: 'thresholds must be an object' });
        return;
      }
      for (const [address, entry] of Object.entries(body.thresholds as Record<string, unknown>)) {
        if (typeof entry !== 'object' || entry === null) {
          sendJson(res, 400, { [address]: 'Invalid value' });
          return;
        }
        state.alarms[address] = { ...(state.alarms[address] ?? {}), ...(entry as Record<string, unknown>) };
      }
      sendJson(res, 200, { success: true });
      return;
    }

    // --- Команды ----------------------------------------------------------
    if (path === '/rest/commands' && match.method === 'POST') {
      const body = parseBodyObject(await readBody(req));
      if (body === null || typeof body.commands !== 'string' || body.commands.trim() === '') {
        sendJson(res, 400, { error: 'commands is required' });
        return;
      }
      const command = body.commands.trim();
      commandSeq += 1;
      const id = commandSeq;
      const entry = {
        id,
        command,
        state: command === 'get_log' ? 'running' : 'done',
        queued_at: uptime(),
        started_at: uptime(),
        finished_at: command === 'get_log' ? null : uptime(),
        ok: true,
        reason: '',
        confidence: 'transport',
      };
      state.commands.set(id, entry);
      sendJson(res, 200, {
        status: 'accepted',
        command,
        commandId: id,
        warnings: [],
      });
      return;
    }
    const commandMatch = /^\/rest\/commands\/(\d+)$/.exec(path);
    if (commandMatch !== null && match.method === 'GET') {
      const id = Number(commandMatch[1]);
      const entry = state.commands.get(id);
      if (entry === undefined) {
        // Как у прошивки: запись вытеснена из журнала.
        notFound(res, path, match.method);
        return;
      }
      // `get_log` завершается на третьем опросе, остальные готовы сразу.
      if (entry.state === 'running') {
        const polls = (entry.polls as number | undefined) ?? 0;
        if (polls >= 2) {
          entry.state = 'done';
          entry.finished_at = uptime();
        } else {
          entry.polls = polls + 1;
        }
      }
      sendJson(res, 200, entry);
      return;
    }

    // --- XKC-25 -----------------------------------------------------------
    if (path === '/rest/oc/xkc25/status' && match.method === 'GET') {
      sendJson(res, 200, XKC25_STATUS_BODY);
      return;
    }
    if (path === '/rest/oc/xkc25/verify-status' && match.method === 'GET') {
      sendJson(res, 200, {
        status: 'done',
        index: 0,
        ok: true,
        completed: true,
        code: 'ok',
        rawLevel: 1,
        message: 'Проверка возврата завершена',
      });
      return;
    }
    if (path === '/rest/oc/xkc25/calibrate' && match.method === 'POST') {
      const body = parseBodyObject(await readBody(req));
      const index = typeof body?.index === 'number' ? body.index : 0;
      const step = typeof body?.step === 'string' ? body.step : '';
      if (!['without_liquid', 'with_liquid', 'verify_return'].includes(step)) {
        sendJson(res, 400, { error: 'invalid_step', code: 'invalid_step' });
        return;
      }
      if (step === 'verify_return') {
        sendJson(res, 202, {
          status: 'pending',
          index,
          code: 'ok',
          message: 'Проверка возврата запущена',
        });
        return;
      }
      sendJson(res, 200, {
        ok: true,
        completed: true,
        code: 'ok',
        message: step === 'without_liquid' ? 'Калибровка без жидкости сохранена' : 'Калибровка с жидкостью сохранена',
        checklist: [],
        sensor: XKC25_STATUS_BODY.sensors[index] ?? XKC25_STATUS_BODY.sensors[0],
      });
      return;
    }

    // --- Реле и аппаратная конфигурация ----------------------------------
    // GPIO-сборка: расширителя PCF8574 нет, поэтому `supported:false` — это данные, а не ошибка.
    if (
      (path === '/rest/oc/relay/capabilities' ||
        path === '/rest/oc/relay/metadata' ||
        path === '/rest/oc/relay/state') &&
      match.method === 'GET'
    ) {
      sendJson(res, 200, { supported: false, pinoutUseGpio: true });
      return;
    }
    if (path === '/rest/oc/relay/override' && match.method === 'POST') {
      sendJson(res, 409, { error: 'Relay is not supported on this build' });
      return;
    }
    if (path === '/rest/oc/hardware-config' && match.method === 'GET') {
      sendJson(res, 200, state.hardwareConfig);
      return;
    }
    if (path === '/rest/oc/hardware-config' && match.method === 'PUT') {
      const body = parseBodyObject(await readBody(req));
      if (body === null || Object.keys(body).length === 0) {
        sendJson(res, 400, { error: 'body_must_be_object', message: 'Body must be a JSON object' });
        return;
      }
      const next: Record<string, unknown> = { ...state.hardwareConfig, ...body };
      // IP-адреса конкретных датчиков требуют перезагрузки — как у прошивки.
      next.needsReboot = 'lcd1602I2cAddress' in body || 'bmp581I2cAddress' in body;
      state.hardwareConfig = next;
      sendJson(res, 200, next);
      return;
    }
    if (path === '/rest/oc/hardware-faults' && match.method === 'GET') {
      sendJson(res, 200, HARDWARE_FAULTS_BODY);
      return;
    }

    // --- Файлы, подсистемы, система --------------------------------------
    if (path === '/rest/files' && match.method === 'GET') {
      sendJson(res, 200, FILES_BODY);
      return;
    }
    if (path.startsWith('/rest/files/') && match.method === 'GET') {
      const requested = `/${path.slice('/rest/files/'.length)}`;
      const file = requested.endsWith('/config/app.json')
        ? '{"hyst":0.5,"decrement":2,"sound":true}'
        : null;
      if (file === null) {
        notFound(res, path, match.method);
        return;
      }
      sendText(res, 200, file, 'application/json; charset=utf-8');
      return;
    }
    if (path === '/rest/files/copy' && match.method === 'POST') {
      const body = parseBodyObject(await readBody(req));
      if (typeof body?.source !== 'string' || typeof body?.destination !== 'string') {
        sendJson(res, 400, { error: 'source and destination are required' });
        return;
      }
      sendJson(res, 201, { success: true });
      return;
    }
    if (path.startsWith('/rest/files/') && match.method === 'DELETE') {
      sendJson(res, 204, {});
      return;
    }
    if (path === '/rest/subsystem' && match.method === 'GET') {
      sendJson(res, 200, state.subsystems);
      return;
    }
    if (path === '/rest/subsystem' && match.method === 'PUT') {
      const body = parseBodyObject(await readBody(req));
      if (body === null) {
        sendJson(res, 400, { success: false, errors: [{ message: 'Body must be a JSON object' }] });
        return;
      }
      const errors: Array<Record<string, unknown>> = [];
      for (const [name, enabled] of Object.entries(body)) {
        if (typeof enabled !== 'boolean') {
          errors.push({ subsystem: name, message: 'enabled must be boolean' });
          continue;
        }
        if (!(name in state.subsystems)) {
          errors.push({ subsystem: name, message: 'unknown subsystem' });
          continue;
        }
        state.subsystems[name] = enabled;
      }
      sendJson(res, 200, { success: errors.length === 0, errors });
      return;
    }
    if (path === '/rest/systemStatus' && match.method === 'GET') {
      sendJson(res, 200, {
        esp_platform: 'ESP32',
        firmware_version: '0.2.9.2',
        cpu_freq_mhz: 240,
        cpu_type: 'ESP32-D0WD',
        cpu_rev: 3,
        cpu_cores: 2,
        free_heap: 214_000,
        total_heap: 340_000,
        min_free_heap: 180_000,
        max_alloc_heap: 110_000,
        fs_total: 1_048_576,
        fs_used: 262_144,
        sdk_version: 'v4.4.7',
        arduino_version: '2.0.17',
        flash_chip_size: 4_194_304,
        uptime: Math.floor(uptime() / 1000),
        core_temp: 41.5,
      });
      return;
    }
    if (path === '/rest/restart' && match.method === 'POST') {
      sendJson(res, 200, { ok: true });
      return;
    }

    // --- Журналы ----------------------------------------------------------
    if (path === '/rest/logs' && match.method === 'GET') {
      sendJson(res, 200, {
        status: 'list',
        total: 2,
        received: 0,
        files: ['12.CSV', '13.CSV'],
      });
      return;
    }
    if (path === '/rest/logs' && match.method === 'DELETE') {
      state.logsPolls = 0;
      sendJson(res, 200, { status: 'idle' });
      return;
    }
    const logMatch = /^\/rest\/logs\/(\d+)$/.exec(path);
    if (logMatch !== null && match.method === 'GET') {
      state.logsPolls += 1;
      // Первый ответ — 202 (передача идёт), второй — готовый CSV: так проверяется поллинг.
      if (state.logsPolls === 1) {
        sendJson(res, 202, { status: 'receiving', total: 3, received: 1 });
        return;
      }
      sendText(res, 200, LOGS_CSV, 'text/csv; charset=utf-8', {
        'content-disposition': `attachment; filename="${logMatch[1]}.CSV"`,
      });
      return;
    }

    // --- Профили ----------------------------------------------------------
    if (path === '/rest/profiles' && match.method === 'GET') {
      sendJson(res, 200, state.profileList);
      return;
    }
    if (path === '/rest/profiles/active' && match.method === 'GET') {
      sendJson(res, 200, { id: state.activeProfileId });
      return;
    }
    if (path === '/rest/profiles/content' && match.method === 'GET') {
      const id = url.searchParams.get('id') ?? '';
      const content = PROFILE_CONTENT[id];
      if (content === undefined) {
        notFound(res, path, match.method);
        return;
      }
      sendJson(res, 200, content);
      return;
    }

    notFound(res, path, match.method);
  };

  const server = createServer((req, res) => {
    void (async (): Promise<void> => {
      const url = new URL(req.url ?? '/', `http://${req.headers.host ?? '127.0.0.1'}`);
      const match: RouteMatch = {
        method: (req.method ?? 'GET').toUpperCase(),
        path: url.pathname,
        url,
        req,
        res,
        params: [],
      };
      try {
        await handle(match);
      } catch (error) {
        if (res.headersSent) {
          res.destroy();
          return;
        }
        const message = error instanceof Error ? error.message : String(error);
        sendJson(res, 500, { error: 'mock_internal_error', message });
      }
    })();
  });

  return {
    server,
    state,
    listen(port = opts.port ?? 0, host = opts.host ?? '127.0.0.1'): Promise<{ host: string; port: number }> {
      return new Promise((resolve, reject) => {
        const onError = (error: Error): void => {
          server.removeListener('listening', onListening);
          reject(error);
        };
        const onListening = (): void => {
          server.removeListener('error', onError);
          const address = server.address();
          const actualPort = typeof address === 'object' && address !== null ? address.port : port;
          resolve({ host, port: actualPort });
        };
        server.once('error', onError);
        server.once('listening', onListening);
        server.listen(port, host);
      });
    },
    close(): Promise<void> {
      return new Promise((resolve, reject) => {
        server.close((error) => (error === undefined ? resolve() : reject(error)));
      });
    },
  };
}

/**
 * Поднимает мок на свободном порту (или на указанном) и возвращает готовый `MockDevice`.
 * Тесты обязаны звать `close()`: иначе `node --test` не завершится.
 */
export async function startMockDevice(opts: MockDeviceOptions = {}): Promise<MockDevice> {
  const device = createMockDevice(opts);
  const address = await device.listen(opts.port ?? 0, opts.host ?? '127.0.0.1');
  const host = address.host === '0.0.0.0' ? '127.0.0.1' : address.host;
  return {
    server: device.server,
    state: device.state,
    address,
    url: `http://${host}:${address.port}`,
    close: device.close,
  };
}

/** Разбирает `MOCK_*` окружение для CLI. */
function optionsFromEnv(env: NodeJS.ProcessEnv): MockDeviceOptions {
  const portRaw = env.MOCK_PORT;
  const port = portRaw === undefined || portRaw.trim() === '' ? 8099 : Number.parseInt(portRaw, 10);
  const switchRaw = env.MOCK_STAGE_SWITCH_MS;
  const stageSwitchMs =
    switchRaw === undefined || switchRaw.trim() === '' ? 1500 : Number.parseInt(switchRaw, 10);
  return {
    port: Number.isFinite(port) ? port : 8099,
    host: env.MOCK_HOST ?? '127.0.0.1',
    security: (env.MOCK_SECURITY ?? 'on').toLowerCase() !== 'off',
    user: env.MOCK_USER,
    password: env.MOCK_PASSWORD,
    stageSwitchMs: Number.isFinite(stageSwitchMs) ? stageSwitchMs : 1500,
  };
}

async function main(): Promise<void> {
  const opts = optionsFromEnv(process.env);
  const device = await startMockDevice(opts);
  process.stdout.write(
    `[mock-openconnect] мок-устройство слушает ${device.url}\n` +
      `[mock-openconnect] security: ${opts.security === false ? 'off (анонимно)' : 'on'}; ` +
      `стадия сменится через ${opts.stageSwitchMs} мс\n` +
      `[mock-openconnect] проверка: curl http://${device.address.host}:${device.address.port}/rest/oc/info\n`,
  );
  const shutdown = (): void => {
    void device.close().then(() => process.exit(0));
  };
  process.on('SIGINT', shutdown);
  process.on('SIGTERM', shutdown);
}

const invokedDirectly =
  process.argv[1] !== undefined && import.meta.url === pathToFileURL(process.argv[1]).href;
if (invokedDirectly) {
  void main();
}
