/**
 * Структурный лог: одна строка — один JSON-объект в stdout.
 *
 * Почему так: сервис живёт в Docker/systemd, где сбор логов — штатный, а
 * `console.log('...')` с интерполяцией секретов — источник утечек. Поэтому
 * редакция секретов живёт на уровне логгера, а не в вызывающем коде.
 */

export type LogLevel = 'debug' | 'info' | 'warn' | 'error' | 'silent';

export const LOG_LEVELS: readonly LogLevel[] = ['debug', 'info', 'warn', 'error', 'silent'];

const LEVEL_WEIGHT: Record<LogLevel, number> = {
  debug: 10,
  info: 20,
  warn: 30,
  error: 40,
  silent: 100,
};

/** Ключи, значение которых никогда не попадает в лог. */
export const SECRET_KEY_RE = /authorization|passw|secret|token|api[-_]?key|cookie/i;

export const REDACTED = '***';

export function parseLogLevel(raw: string | undefined, fallback: LogLevel = 'info'): LogLevel {
  if (raw === undefined || raw.trim() === '') return fallback;
  const value = raw.trim().toLowerCase();
  if ((LOG_LEVELS as readonly string[]).includes(value)) return value as LogLevel;
  throw new Error(
    `Некорректный уровень логов "${raw}". Допустимо: ${LOG_LEVELS.join(', ')}. ` +
      'Проверьте переменную окружения SSVC_LOG_LEVEL.',
  );
}

export interface LogFields {
  [key: string]: unknown;
}

export interface Logger {
  readonly level: LogLevel;
  debug(msg: string, fields?: LogFields): void;
  info(msg: string, fields?: LogFields): void;
  warn(msg: string, fields?: LogFields): void;
  error(msg: string, fields?: LogFields): void;
  child(fields: LogFields): Logger;
}

export interface LoggerOptions {
  level?: LogLevel;
  /** Куда писать строку. По умолчанию — stdout. */
  sink?: (line: string) => void;
  base?: LogFields;
  now?: () => number;
}

/**
 * Глубоко заменяет значения секретных ключей. Работает с массивами и объектами,
 * ограничивает глубину, чтобы не упасть на циклических ссылках.
 */
export function redactValue(value: unknown, depth = 0): unknown {
  if (depth > 6) return '[truncated]';
  if (value === null || value === undefined) return value;
  if (value instanceof Error) {
    return { name: value.name, message: value.message, stack: value.stack };
  }
  if (Array.isArray(value)) return value.map((item) => redactValue(item, depth + 1));
  if (typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [key, item] of Object.entries(value as Record<string, unknown>)) {
      out[key] = SECRET_KEY_RE.test(key) ? REDACTED : redactValue(item, depth + 1);
    }
    return out;
  }
  return value;
}

/** Редакция секретов в произвольном тексте (например, в теле ошибки). */
export function redactText(text: string): string {
  return text
    .replace(
      /("?(?:authorization|password|passwd|secret|token|access_token|api[_-]?key)"?\s*[:=]\s*)("[^"]*"|'[^']*'|[^\s,&}]+)/gi,
      (_match, prefix: string) => `${prefix}${REDACTED}`,
    )
    .replace(/(Bearer\s+)[A-Za-z0-9._~+/-]{8,}/gi, `$1${REDACTED}`);
}

function safeStringify(value: unknown): string {
  const seen = new WeakSet<object>();
  try {
    return JSON.stringify(value, (_key, item: unknown) => {
      if (typeof item === 'object' && item !== null) {
        if (seen.has(item as object)) return '[circular]';
        seen.add(item as object);
      }
      return item;
    });
  } catch (error) {
    return JSON.stringify({ msg: 'log serialization failed', error: String(error) });
  }
}

class JsonLogger implements Logger {
  constructor(
    readonly level: LogLevel,
    private readonly sink: (line: string) => void,
    private readonly base: LogFields,
    private readonly now: () => number,
  ) {}

  private write(level: Exclude<LogLevel, 'silent'>, msg: string, fields?: LogFields): void {
    if (LEVEL_WEIGHT[level] < LEVEL_WEIGHT[this.level]) return;
    const record: Record<string, unknown> = {
      ts: new Date(this.now()).toISOString(),
      level,
      msg,
      ...(redactValue(this.base) as Record<string, unknown>),
      ...(fields ? (redactValue(fields) as Record<string, unknown>) : {}),
    };
    this.sink(safeStringify(record));
  }

  debug(msg: string, fields?: LogFields): void {
    this.write('debug', msg, fields);
  }
  info(msg: string, fields?: LogFields): void {
    this.write('info', msg, fields);
  }
  warn(msg: string, fields?: LogFields): void {
    this.write('warn', msg, fields);
  }
  error(msg: string, fields?: LogFields): void {
    this.write('error', msg, fields);
  }
  child(fields: LogFields): Logger {
    return new JsonLogger(this.level, this.sink, { ...this.base, ...fields }, this.now);
  }
}

export function createLogger(opts: LoggerOptions = {}): Logger {
  const level = opts.level ?? 'info';
  const sink = opts.sink ?? ((line: string) => process.stdout.write(`${line}\n`));
  return new JsonLogger(level, sink, opts.base ?? {}, opts.now ?? Date.now);
}

/** Логгер, который ничего не пишет: для тестов и «тихих» контекстов. */
export function silentLogger(): Logger {
  return createLogger({ level: 'silent', sink: () => {} });
}
