/**
 * Аудит опасных операций: JSONL-файл, по одной строке на действие.
 *
 * Состав записи зафиксирован контрактом (§3.11):
 *   ts, session, tool, risk, confirm, reason, args_hash, result, duration_ms.
 *
 * Чего здесь принципиально нет: тел файлов, журналов, паролей и токенов.
 * `details` вычищаются от секретов, «тяжёлые» ключи (body/response/content/raw)
 * выбрасываются с пометкой `details_omitted`.
 */

import { createHash } from 'node:crypto';
import { appendFile, mkdir, readFile } from 'node:fs/promises';
import { dirname } from 'node:path';
import { redactValue, type Logger } from './logging.js';
import type { RiskClass } from './policy/risk.js';

export interface AuditEntry {
  tool: string;
  risk: RiskClass | number;
  confirm?: boolean;
  reason?: string;
  result: string;
  details?: Record<string, unknown>;
  /** Длительность операции; если не передана — считается из details.duration_ms. */
  durationMs?: number;
}

export interface AuditRecord {
  ts: string;
  session: string;
  tool: string;
  risk: number;
  confirm: boolean | null;
  reason: string | null;
  args_hash: string;
  result: string;
  duration_ms: number | null;
  details?: Record<string, unknown>;
  details_omitted?: boolean;
}

export interface AuditLogOptions {
  path: string;
  session?: string;
  log?: Logger;
  now?: () => number;
  /** Максимальная длина строковых значений в details (защита от «простыней»). */
  maxStringLength?: number;
}

/** Ключи, которые в аудит не попадают никогда. */
const HEAVY_KEYS = /^(body|request|response|content|csv|file|file_content|raw|payload|data)$/i;
const MAX_STRING = 512;

/** Устойчивый хеш аргументов: одинаковый вызов → одинаковый args_hash. */
export function hashArgs(args: unknown): string {
  const normalized = stableStringify(args ?? null);
  return createHash('sha256').update(normalized).digest('hex');
}

/** JSON с отсортированными ключами — чтобы хеш не зависел от порядка полей. */
export function stableStringify(value: unknown): string {
  return JSON.stringify(sortDeep(value));
}

function sortDeep(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(sortDeep);
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const key of Object.keys(value as Record<string, unknown>).sort()) {
      out[key] = sortDeep((value as Record<string, unknown>)[key]);
    }
    return out;
  }
  return value;
}

function sanitizeDetails(
  details: Record<string, unknown> | undefined,
  maxStringLength: number,
): { details?: Record<string, unknown>; omitted: boolean } {
  if (details === undefined) return { omitted: false };
  let omitted = false;
  const redacted = redactValue(details) as Record<string, unknown>;
  const cleaned: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(redacted)) {
    if (HEAVY_KEYS.test(key)) {
      omitted = true;
      continue;
    }
    cleaned[key] = truncateStrings(value, maxStringLength);
  }
  return { details: cleaned, omitted };
}

function truncateStrings(value: unknown, max: number): unknown {
  if (typeof value === 'string') {
    return value.length > max ? `${value.slice(0, max)}…` : value;
  }
  if (Array.isArray(value)) return value.map((item) => truncateStrings(item, max));
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [key, item] of Object.entries(value as Record<string, unknown>)) {
      out[key] = truncateStrings(item, max);
    }
    return out;
  }
  return value;
}

/**
 * Журнал аудита одного процесса. Экземпляр на сессию (поле `session`),
 * запись — последовательная очередь, чтобы строки не перемешивались.
 */
export class AuditLog {
  session: string;
  private readonly path: string;
  private readonly log?: Logger;
  private readonly now: () => number;
  private readonly maxStringLength: number;
  private queue: Promise<void> = Promise.resolve();
  private closed = false;
  private written = 0;

  constructor(opts: AuditLogOptions) {
    this.path = opts.path;
    this.session = opts.session ?? 'system';
    this.log = opts.log;
    this.now = opts.now ?? Date.now;
    this.maxStringLength = opts.maxStringLength ?? MAX_STRING;
  }

  get filePath(): string {
    return this.path;
  }

  get writtenCount(): number {
    return this.written;
  }

  /** Строит запись, не выполняя ввода-вывода — удобно для проверок. */
  build(entry: AuditEntry): AuditRecord {
    const details = entry.details ?? {};
    // args_hash считаем по details.args, если он передан: это «отпечаток» вызова.
    const argsHash = hashArgs('args' in details ? details.args : details);
    const rest: Record<string, unknown> = { ...details };
    delete rest.args;
    const durationFromDetails = rest.duration_ms;
    delete rest.duration_ms;

    const sanitized = sanitizeDetails(rest, this.maxStringLength);
    const duration =
      entry.durationMs ??
      (typeof durationFromDetails === 'number' ? durationFromDetails : undefined);

    const record: AuditRecord = {
      ts: new Date(this.now()).toISOString(),
      session: this.session,
      tool: entry.tool,
      risk: entry.risk,
      confirm: entry.confirm ?? null,
      reason: entry.reason ?? null,
      args_hash: argsHash,
      result: entry.result,
      duration_ms: duration ?? null,
    };
    if (sanitized.details !== undefined && Object.keys(sanitized.details).length > 0) {
      record.details = sanitized.details;
    }
    if (sanitized.omitted) record.details_omitted = true;
    return record;
  }

  /** Записывает событие. Ошибка диска не должна ломать операцию — только лог. */
  write(entry: AuditEntry): void {
    const record = this.build(entry);
    this.enqueue(record);
  }

  private enqueue(record: AuditRecord): void {
    if (this.closed) {
      this.log?.warn('Аудит закрыт: запись пропущена', { tool: record.tool });
      return;
    }
    const line = `${JSON.stringify(record)}\n`;
    this.queue = this.queue
      .then(async () => {
        await mkdir(dirname(this.path), { recursive: true });
        await appendFile(this.path, line, 'utf8');
        this.written += 1;
      })
      .catch((error: unknown) => {
        this.log?.error('Не удалось записать аудит', { path: this.path, error });
      });
  }

  /** Дожидается записи всех поставленных в очередь строк. */
  async flush(): Promise<void> {
    await this.queue;
  }

  async close(): Promise<void> {
    await this.flush();
    this.closed = true;
  }

  /** Читает журнал (для диагностики и тестов). */
  async readAll(): Promise<AuditRecord[]> {
    await this.flush();
    try {
      const text = await readFile(this.path, 'utf8');
      return text
        .split(/\r?\n/)
        .filter((line) => line.trim() !== '')
        .map((line) => JSON.parse(line) as AuditRecord);
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code === 'ENOENT') return [];
      throw error;
    }
  }

  /** Последние `n` записей — для `oc_diagnostics`. */
  async last(n: number): Promise<AuditRecord[]> {
    const all = await this.readAll();
    return all.slice(Math.max(0, all.length - n));
  }
}
