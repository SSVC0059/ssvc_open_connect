/**
 * Сырые обмены с контроллером: общие правила редакции и усечения.
 *
 * Здесь нет ввода-вывода — только чистые функции, чтобы их можно было проверить
 * без диска. Запись файлов и ротация — в `device/RawLogger.ts`.
 */

import { redactText } from './logging.js';

/** Предел одного поля (тело запроса/ответа) — 64 КБ. */
export const MAX_FIELD_BYTES = 64 * 1024;

export const REDACTED = '***';

/** Ключи, значения которых заменяются на `***` до записи. */
export const SECRET_KEY_RE = /^(authorization|password|passwd|secret|token|access_token|api[_-]?key|cookie)$/i;

export interface RawExchange {
  ts: string;
  method: string;
  path: string;
  status?: number;
  ms?: number;
  request?: string;
  response?: string;
  error?: string;
  truncated?: boolean;
  session?: string;
}

export interface RawExchangeInput {
  method: string;
  path: string;
  status?: number;
  ms?: number;
  request?: unknown;
  response?: unknown;
  error?: unknown;
  session?: string;
  now?: () => number;
}

export interface TruncatedField {
  value: string;
  truncated: boolean;
}

/** Усекает строку до лимита в байтах UTF-8, не разрывая суррогатные пары. */
export function truncateField(text: string, maxBytes = MAX_FIELD_BYTES): TruncatedField {
  const bytes = Buffer.byteLength(text, 'utf8');
  if (bytes <= maxBytes) return { value: text, truncated: false };
  // Buffer.subarray по границе байт, затем «мягкая» обрезка незавершённого символа.
  const buf = Buffer.from(text, 'utf8').subarray(0, maxBytes);
  const value = buf.toString('utf8').replace(/\uFFFD+$/u, '');
  return { value, truncated: true };
}

function redactDeep(value: unknown, depth = 0): unknown {
  if (depth > 8) return '[truncated]';
  if (value === null || value === undefined) return value;
  if (Array.isArray(value)) return value.map((item) => redactDeep(item, depth + 1));
  if (typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [key, item] of Object.entries(value as Record<string, unknown>)) {
      out[key] = SECRET_KEY_RE.test(key) ? REDACTED : redactDeep(item, depth + 1);
    }
    return out;
  }
  if (typeof value === 'string') return redactText(value);
  return value;
}

/**
 * Сериализует тело обмена в строку с редакцией секретов.
 * JSON разбирается и вычищается по ключам; остальное (text/plain, CSV, form) — регуляркой.
 */
export function serializeBody(body: unknown): string | undefined {
  if (body === undefined) return undefined;
  if (typeof body === 'string') {
    const trimmed = body.trim();
    if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
      try {
        return JSON.stringify(redactDeep(JSON.parse(trimmed)));
      } catch {
        // не JSON — обработаем как текст
      }
    }
    return redactText(body);
  }
  return JSON.stringify(redactDeep(body));
}

/** Собирает запись сырого лога: редакция + усечение полей. */
export function buildRawRecord(input: RawExchangeInput): RawExchange {
  const now = input.now ?? Date.now;
  const record: RawExchange = {
    ts: new Date(now()).toISOString(),
    method: input.method.toUpperCase(),
    path: input.path,
  };
  if (input.status !== undefined) record.status = input.status;
  if (input.ms !== undefined) record.ms = Math.round(input.ms);
  if (input.session !== undefined) record.session = input.session;

  let truncated = false;
  const request = serializeBody(input.request);
  if (request !== undefined) {
    const cut = truncateField(request);
    record.request = cut.value;
    truncated = truncated || cut.truncated;
  }
  const response = serializeBody(input.response);
  if (response !== undefined) {
    const cut = truncateField(response);
    record.response = cut.value;
    truncated = truncated || cut.truncated;
  }
  if (input.error !== undefined) {
    record.error = redactText(input.error instanceof Error ? input.error.message : String(input.error));
  }
  if (truncated) record.truncated = true;
  return record;
}

/** Нужно ли писать обмен при заданном режиме. */
export function shouldRecord(
  mode: 'off' | 'errors' | 'all',
  outcome: { status?: number; error?: unknown },
): boolean {
  if (mode === 'off') return false;
  if (mode === 'all') return true;
  return outcome.error !== undefined || (outcome.status !== undefined && outcome.status >= 400);
}
