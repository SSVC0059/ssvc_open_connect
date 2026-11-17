/**
 * Запись сырых обменов с контроллером в JSONL (контракт §3.12).
 *
 * Одна строка — один обмен: метод, путь, статус, длительность, тела (усечённые до
 * 64 КБ, с пометкой truncated) и вырезанные секреты.
 * Файлы: `<dir>/YYYY-MM-DD.jsonl` (UTC), предел каталога — `SSVC_RAW_LOG_MAX_MB`,
 * при превышении удаляется самый старый файл (текущий не трогаем).
 */

import { appendFile, mkdir, readdir, stat, unlink } from 'node:fs/promises';
import { join } from 'node:path';
import type { Logger } from '../logging.js';
import { buildRawRecord, shouldRecord } from '../rawlog.js';

export type RawLogMode = 'off' | 'errors' | 'all';

export interface RawLogInput {
  method: string;
  path: string;
  status?: number;
  ms?: number;
  request?: unknown;
  response?: unknown;
  error?: unknown;
  session?: string;
}

/** Минимальный интерфейс приёмника: DeviceClient и AuthSession пишут через него. */
export interface RawSink {
  record(input: RawLogInput): void | Promise<void>;
}

export interface RawLoggerOptions {
  mode: RawLogMode;
  dir: string;
  maxMb: number;
  log?: Logger;
  now?: () => number;
}

export interface RawLoggerStats {
  written: number;
  skipped: number;
  bytes: number;
  pruned: number;
}

export class RawLogger implements RawSink {
  readonly mode: RawLogMode;
  readonly dir: string;
  readonly maxMb: number;
  private readonly log?: Logger;
  private readonly now: () => number;
  private queue: Promise<void> = Promise.resolve();
  private writtenBytes = 0;
  private sincePrune = 0;
  private closed = false;
  private stats: RawLoggerStats = { written: 0, skipped: 0, bytes: 0, pruned: 0 };

  constructor(opts: RawLoggerOptions) {
    this.mode = opts.mode;
    this.dir = opts.dir;
    this.maxMb = opts.maxMb;
    this.log = opts.log;
    this.now = opts.now ?? Date.now;
  }

  get enabled(): boolean {
    return this.mode !== 'off';
  }

  getStats(): RawLoggerStats {
    return { ...this.stats, bytes: this.writtenBytes };
  }

  record(input: RawLogInput): void {
    if (!this.enabled) {
      this.stats.skipped += 1;
      return;
    }
    if (!shouldRecord(this.mode, { status: input.status, error: input.error })) {
      this.stats.skipped += 1;
      return;
    }
    if (this.closed) return;

    const record = buildRawRecord({
      ...input,
      now: this.now,
    });
    const line = `${JSON.stringify(record)}\n`;
    this.queue = this.queue
      .then(async () => {
        await mkdir(this.dir, { recursive: true });
        const file = join(this.dir, `${this.dayStamp()}.jsonl`);
        await appendFile(file, line, 'utf8');
        this.writtenBytes += Buffer.byteLength(line, 'utf8');
        this.stats.written += 1;
        this.sincePrune += 1;
        if (this.sincePrune >= 10) await this.prune();
      })
      .catch((error: unknown) => {
        this.log?.warn('Не удалось записать сырой лог', { dir: this.dir, error });
      });
  }

  private dayStamp(): string {
    return new Date(this.now()).toISOString().slice(0, 10);
  }

  /** Удаляет самые старые файлы, пока каталог не впишется в предел. */
  async prune(): Promise<void> {
    this.sincePrune = 0;
    const limitBytes = Math.max(1, Math.round(this.maxMb * 1024 * 1024));
    let entries: string[];
    try {
      entries = (await readdir(this.dir)).filter((name) => name.endsWith('.jsonl')).sort();
    } catch {
      return;
    }
    if (entries.length <= 1) return;

    const sizes = new Map<string, number>();
    let total = 0;
    for (const name of entries) {
      try {
        const info = await stat(join(this.dir, name));
        sizes.set(name, info.size);
        total += info.size;
      } catch {
        // файл могли удалить параллельно
      }
    }
    // Самый новый файл (= текущий день) не удаляем никогда.
    const removable = entries.slice(0, -1);
    for (const name of removable) {
      if (total <= limitBytes) break;
      try {
        await unlink(join(this.dir, name));
        total -= sizes.get(name) ?? 0;
        this.stats.pruned += 1;
      } catch (error) {
        this.log?.warn('Не удалось удалить старый сырой лог', { file: name, error });
      }
    }
    this.writtenBytes = total;
  }

  async flush(): Promise<void> {
    await this.queue;
  }

  async close(): Promise<void> {
    await this.flush();
    if (this.enabled) await this.prune();
    this.closed = true;
  }
}
