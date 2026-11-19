/**
 * Конфигурация сервиса: единственный источник — переменные окружения (или `.env`).
 *
 * Правила:
 *  - обязательные переменные проверяются на старте и валят процесс с русским
 *    сообщением: «открытый порт без токена» недопустим;
 *  - секреты (пароль контроллера, токен клиентов) живут только в памяти;
 *  - значения по умолчанию соответствуют docs/develop/mcp-server.md §3.9,
 *    а `.env.example` содержит безопасные (релизные) значения.
 */

import { existsSync, readFileSync } from 'node:fs';
import { parseLogLevel, type LogLevel } from './logging.js';

export interface Config {
  /** Транспорт, под который собрана конфигурация (см. `LoadConfigOptions.transport`). */
  transport: 'http' | 'stdio';
  device: { url: string; user: string; password: string; timeoutMs: number };
  mcp: {
    host: string;
    port: number;
    token: string;
    allowedHosts: string[];
    allowedCidrs: string[];
    tls: { cert: string; key: string } | null;
    maxSessions: number;
    idleMs: number;
  };
  raw: { mode: 'off' | 'errors' | 'all'; dir: string; maxMb: number };
  allowDestructive: boolean;
  auditPath: string;
  logLevel: LogLevel;
}

/** Ошибка конфигурации: сообщение предназначено человеку, а не программе. */
export class ConfigError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'ConfigError';
  }
}

export const DEFAULTS = {
  deviceTimeoutMs: 10_000,
  // Безопасное умолчание: сервис слушает только петлевой адрес. Для доступа из
  // сети (или из контейнера) адрес задаётся явно — SSVC_MCP_BIND, Dockerfile и
  // compose выставляют 0.0.0.0, потому что иначе проброс порта не работает.
  host: '127.0.0.1',
  port: 8080,
  maxSessions: 32,
  idleMs: 30 * 60 * 1000,
  // Безопасные умолчания: сырые обмены не пишутся, разрушительные операции и debug
  // выключены. Включаются явно в .env — так «минимальный» конфиг не открывает
  // опасные инструменты по недосмотру (см. .env.example и README).
  rawMode: 'off' as const,
  rawDir: './logs/raw',
  rawMaxMb: 50,
  auditPath: './logs/audit.jsonl',
  logLevel: 'info' as LogLevel,
} as const;

/** Секунды/миллисекунды в человекочитаемом виде — для сообщений об ошибках. */
function describe(name: string): string {
  return `Переменная окружения ${name}`;
}

function readString(env: NodeJS.ProcessEnv, name: string): string | undefined {
  const raw = env[name];
  if (raw === undefined) return undefined;
  const value = raw.trim();
  return value === '' ? undefined : value;
}

function required(env: NodeJS.ProcessEnv, name: string, why: string): string {
  const value = readString(env, name);
  if (value === undefined) {
    throw new ConfigError(`${describe(name)} не задана. ${why}`);
  }
  return value;
}

function readInt(
  env: NodeJS.ProcessEnv,
  name: string,
  fallback: number,
  range: { min: number; max: number },
): number {
  const value = readString(env, name);
  if (value === undefined) return fallback;
  if (!/^-?\d+$/.test(value)) {
    throw new ConfigError(`${describe(name)} должна быть целым числом, получено "${value}".`);
  }
  const parsed = Number.parseInt(value, 10);
  if (parsed < range.min || parsed > range.max) {
    throw new ConfigError(
      `${describe(name)} должна быть в диапазоне ${range.min}..${range.max}, получено ${parsed}.`,
    );
  }
  return parsed;
}

function readFloat(
  env: NodeJS.ProcessEnv,
  name: string,
  fallback: number,
  range: { min: number; max: number },
): number {
  const value = readString(env, name);
  if (value === undefined) return fallback;
  const parsed = Number(value.replace(',', '.'));
  if (!Number.isFinite(parsed) || parsed < range.min || parsed > range.max) {
    throw new ConfigError(
      `${describe(name)} должна быть числом в диапазоне ${range.min}..${range.max}, получено "${value}".`,
    );
  }
  return parsed;
}

function readBool(env: NodeJS.ProcessEnv, name: string, fallback: boolean): boolean {
  const value = readString(env, name);
  if (value === undefined) return fallback;
  const normalized = value.toLowerCase();
  if (['1', 'true', 'yes', 'on', 'да'].includes(normalized)) return true;
  if (['0', 'false', 'no', 'off', 'нет'].includes(normalized)) return false;
  throw new ConfigError(
    `${describe(name)} должна быть 0 или 1 (допустимо true/false), получено "${value}".`,
  );
}

function readList(env: NodeJS.ProcessEnv, name: string): string[] {
  const value = readString(env, name);
  if (value === undefined) return [];
  return value
    .split(',')
    .map((item) => item.trim())
    .filter((item) => item.length > 0);
}

function normalizeBaseUrl(raw: string): string {
  let url: URL;
  try {
    url = new URL(raw);
  } catch {
    throw new ConfigError(
      `${describe('SSVC_DEVICE_URL')} содержит некорректный адрес "${raw}". ` +
        'Ожидается вид http://192.168.1.42 (прошивка слушает только HTTP, порт 80).',
    );
  }
  if (url.protocol !== 'http:' && url.protocol !== 'https:') {
    throw new ConfigError(
      `${describe('SSVC_DEVICE_URL')} должна начинаться с http:// или https://, получено "${raw}".`,
    );
  }
  if (url.username !== '' || url.password !== '') {
    throw new ConfigError(
      `${describe('SSVC_DEVICE_URL')} не должна содержать логин и пароль: ` +
        'для этого есть SSVC_USER и SSVC_PASSWORD.',
    );
  }
  url.search = '';
  url.hash = '';
  return url.toString().replace(/\/+$/, '');
}

function parseRawMode(env: NodeJS.ProcessEnv): 'off' | 'errors' | 'all' {
  const value = readString(env, 'SSVC_RAW_LOG');
  if (value === undefined) return DEFAULTS.rawMode;
  const normalized = value.toLowerCase();
  if (normalized === 'off' || normalized === 'errors' || normalized === 'all') {
    return normalized;
  }
  throw new ConfigError(
    `${describe('SSVC_RAW_LOG')} должна быть off, errors или all, получено "${value}".`,
  );
}

/** Расширение значения `private` в списке CIDR (RFC1918 + петлевые адреса). */
export const PRIVATE_CIDRS: readonly string[] = [
  '10.0.0.0/8',
  '172.16.0.0/12',
  '192.168.0.0/16',
  '127.0.0.0/8',
  '::1/128',
  'fc00::/7',
  'fe80::/10',
];

/** Значение по умолчанию для allowlist: «вся частная сеть». */
export const DEFAULT_ALLOWED_CIDRS: readonly string[] = ['private'];

export interface LoadConfigOptions {
  /**
   * Транспорт. В `stdio` нет ни порта, ни Bearer-токена: процесс запускает сам
   * клиент, поэтому `SSVC_MCP_TOKEN` не обязателен. Пароль контроллера тоже
   * можно не задавать: при выключенной на прошивке авторизации запросы идут
   * анонимно (DeviceClient.AuthSession → security off).
   */
  transport?: 'http' | 'stdio';
}

/**
 * Загружает конфигурацию из окружения.
 * @throws ConfigError с русским сообщением, если обязательные значения отсутствуют/некорректны.
 */
export function loadConfig(
  env: NodeJS.ProcessEnv = process.env,
  opts: LoadConfigOptions = {},
): Config {
  const transport = opts.transport ?? 'http';
  const stdio = transport === 'stdio';

  const token = stdio
    ? (readString(env, 'SSVC_MCP_TOKEN') ?? '')
    : required(
        env,
        'SSVC_MCP_TOKEN',
        'Сервер не запускается без токена MCP-клиентов: открытый порт без аутентификации недопустим. ' +
          'Сгенерируйте токен командой «openssl rand -hex 32» и запишите его в .env как SSVC_MCP_TOKEN.',
      );

  const url = normalizeBaseUrl(
    required(
      env,
      'SSVC_DEVICE_URL',
      'Укажите базовый адрес контроллера, например SSVC_DEVICE_URL=http://192.168.1.42.',
    ),
  );

  const password = stdio
    ? (readString(env, 'SSVC_PASSWORD') ?? '')
    : required(
        env,
        'SSVC_PASSWORD',
        'Укажите пароль учётной записи контроллера (SSVC_PASSWORD). Обычно нужна учётка admin: ' +
          'телеметрия и датчики на прошивке доступны только администратору.',
      );

  const host = readString(env, 'SSVC_MCP_BIND') ?? DEFAULTS.host;
  const allowedHosts = readList(env, 'SSVC_MCP_ALLOWED_HOSTS');
  const allowedCidrsRaw = readList(env, 'SSVC_MCP_ALLOWED_CIDRS');
  const allowedCidrs =
    allowedCidrsRaw.length > 0 ? allowedCidrsRaw : [...DEFAULT_ALLOWED_CIDRS];

  const cert = readString(env, 'SSVC_MCP_TLS_CERT');
  const key = readString(env, 'SSVC_MCP_TLS_KEY');
  if ((cert === undefined) !== (key === undefined)) {
    throw new ConfigError(
      'TLS настраивается только парой: задайте и SSVC_MCP_TLS_CERT, и SSVC_MCP_TLS_KEY ' +
        '(либо не задавайте ни одной — тогда сервис работает за TLS-прокси).',
    );
  }

  return {
    transport,
    device: {
      url,
      user: readString(env, 'SSVC_USER') ?? 'admin',
      password,
      timeoutMs: readInt(env, 'SSVC_DEVICE_TIMEOUT_MS', DEFAULTS.deviceTimeoutMs, {
        min: 100,
        max: 600_000,
      }),
    },
    mcp: {
      host,
      port: readInt(env, 'SSVC_MCP_PORT', DEFAULTS.port, { min: 1, max: 65_535 }),
      token,
      allowedHosts,
      allowedCidrs,
      tls: cert !== undefined && key !== undefined ? { cert, key } : null,
      maxSessions: readInt(env, 'SSVC_MCP_MAX_SESSIONS', DEFAULTS.maxSessions, {
        min: 1,
        max: 256,
      }),
      idleMs: readInt(env, 'SSVC_MCP_IDLE_MS', DEFAULTS.idleMs, {
        min: 1_000,
        max: 7 * 24 * 60 * 60 * 1000,
      }),
    },
    raw: {
      mode: parseRawMode(env),
      dir: readString(env, 'SSVC_RAW_LOG_DIR') ?? DEFAULTS.rawDir,
      maxMb: readFloat(env, 'SSVC_RAW_LOG_MAX_MB', DEFAULTS.rawMaxMb, { min: 0.001, max: 100_000 }),
    },
    allowDestructive: readBool(env, 'SSVC_ALLOW_DESTRUCTIVE', false),
    auditPath: readString(env, 'SSVC_AUDIT_PATH') ?? DEFAULTS.auditPath,
    logLevel: parseLogLevel(readString(env, 'SSVC_LOG_LEVEL'), DEFAULTS.logLevel),
  };
}

/** Секретные значения конфига для редакции (лог, диагностика). */
export function configSecrets(cfg: Config): string[] {
  return [cfg.mcp.token, cfg.device.password].filter((secret) => secret.length > 0);
}

/**
 * Разбирает `.env`-файл: пары KEY=VALUE, комментарии `#`, кавычки, пустые строки.
 * Не бросает исключений: отсутствующий файл — это норма (конфигурация из окружения).
 */
export function readDotEnvFile(file: string): Record<string, string> {
  const result: Record<string, string> = {};
  if (!existsSync(file)) return result;
  const text = readFileSync(file, 'utf8');
  for (const line of text.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (trimmed === '' || trimmed.startsWith('#')) continue;
    const normalized = trimmed.startsWith('export ') ? trimmed.slice(7).trim() : trimmed;
    const eq = normalized.indexOf('=');
    if (eq <= 0) continue;
    const key = normalized.slice(0, eq).trim();
    let value = normalized.slice(eq + 1).trim();
    const quoted =
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"));
    if (quoted) {
      value = value.slice(1, -1);
    } else {
      // Комментарий после значения: VALUE   # пояснение
      const hash = value.indexOf(' #');
      if (hash >= 0) value = value.slice(0, hash).trim();
    }
    if (key !== '') result[key] = value;
  }
  return result;
}

/**
 * Дополняет окружение значениями из `.env`, не перетирая уже заданные переменные
 * (окружение контейнера приоритетнее файла).
 */
export function applyDotEnv(
  env: NodeJS.ProcessEnv,
  file = '.env',
): { applied: string[]; file: string } {
  const values = readDotEnvFile(file);
  const applied: string[] = [];
  for (const [key, value] of Object.entries(values)) {
    if (env[key] === undefined) {
      env[key] = value;
      applied.push(key);
    }
  }
  return { applied, file };
}
