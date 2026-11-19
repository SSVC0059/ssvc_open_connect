/**
 * Менеджер MCP-сессий (контракт §3.4): `Map<sessionId, Session>`, лимит, idle-cleanup.
 *
 * Зачем состояние: подписки на ресурсы и уведомления привязаны к сессии, поэтому
 * транспорт работает в stateful-режиме и сессии нужно учитывать явно.
 *
 * Правила:
 *  - `MAX_SESSIONS` (по умолчанию 32) — при переполнении новый `initialize` получает `503`;
 *  - сессия без запросов дольше `IDLE_MS` (30 мин) закрывается фоновой уборкой;
 *  - `onsessioninitialized`/`onsessionclosed` транспорта вызывают `register`/`close`.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import type { StreamableHTTPServerTransport } from '@modelcontextprotocol/sdk/server/streamableHttp.js';
import type { Logger } from '../logging.js';

export interface SessionEntry {
  id: string;
  server: McpServer;
  transport: StreamableHTTPServerTransport;
  createdAt: number;
  lastSeenAt: number;
  /** Сколько HTTP-запросов пришло в сессию (наблюдаемость, без тел). */
  requests: number;
  /** Освобождение ресурсов сессии (например, закрытие её журнала аудита). */
  dispose?: () => Promise<void>;
}

export interface SessionManagerOptions {
  maxSessions: number;
  idleMs: number;
  log?: Logger;
  now?: () => number;
}

/** Период фоновой уборки: чаще, чем idle, но не чаще раза в секунду. */
export function cleanupIntervalMs(idleMs: number): number {
  return Math.max(1_000, Math.min(idleMs, 60_000));
}

export class SessionManager {
  readonly maxSessions: number;
  readonly idleMs: number;
  private readonly log?: Logger;
  private readonly now: () => number;
  private readonly sessions = new Map<string, SessionEntry>();
  private timer: NodeJS.Timeout | null = null;
  private closing = false;

  constructor(opts: SessionManagerOptions) {
    this.maxSessions = Math.max(1, opts.maxSessions);
    this.idleMs = Math.max(1, opts.idleMs);
    this.log = opts.log;
    this.now = opts.now ?? Date.now;
  }

  get size(): number {
    return this.sessions.size;
  }

  /** Есть ли место для новой сессии. */
  hasCapacity(): boolean {
    return this.sessions.size < this.maxSessions;
  }

  /**
   * Учитывает новую сессию. Возвращает `false`, если места уже нет (гонка двух
   * одновременных initialize): вызывающий обязан закрыть транспорт.
   */
  register(
    id: string,
    session: {
      server: McpServer;
      transport: StreamableHTTPServerTransport;
      dispose?: () => Promise<void>;
    },
  ): boolean {
    if (this.sessions.has(id)) {
      this.log?.warn('Сессия с таким идентификатором уже существует', { session: id });
      return true;
    }
    if (!this.hasCapacity()) {
      this.log?.warn('Лимит сессий исчерпан', { max_sessions: this.maxSessions });
      return false;
    }
    const timestamp = this.now();
    this.sessions.set(id, {
      id,
      server: session.server,
      transport: session.transport,
      createdAt: timestamp,
      lastSeenAt: timestamp,
      // Регистрация происходит внутри обработки initialize: это уже первый запрос сессии.
      requests: 1,
      ...(session.dispose === undefined ? {} : { dispose: session.dispose }),
    });
    this.log?.info('Сессия MCP открыта', { session: id, sessions: this.sessions.size });
    return true;
  }

  get(id: string): SessionEntry | undefined {
    return this.sessions.get(id);
  }

  /** Отмечает активность сессии (idle-таймер считается от неё). */
  touch(id: string): SessionEntry | undefined {
    const entry = this.sessions.get(id);
    if (entry === undefined) return undefined;
    entry.lastSeenAt = this.now();
    entry.requests += 1;
    return entry;
  }

  /** Закрывает сессию: транспорт закрывается, запись удаляется. Идемпотентно. */
  async close(id: string): Promise<void> {
    const entry = this.sessions.get(id);
    if (entry === undefined) return;
    this.sessions.delete(id);
    try {
      await entry.transport.close();
    } catch (error) {
      this.log?.warn('Не удалось закрыть транспорт сессии', { session: id, error });
    }
    try {
      await entry.server.close();
    } catch (error) {
      this.log?.debug('Сервер сессии уже закрыт', { session: id, error });
    }
    if (entry.dispose !== undefined) {
      try {
        await entry.dispose();
      } catch (error) {
        this.log?.warn('Не удалось освободить ресурсы сессии', { session: id, error });
      }
    }
    this.log?.info('Сессия MCP закрыта', { session: id, sessions: this.sessions.size });
  }

  /** Закрывает все сессии (остановка сервиса, `DELETE` всех сразу). */
  async closeAll(): Promise<void> {
    this.closing = true;
    const ids = [...this.sessions.keys()];
    for (const id of ids) await this.close(id);
  }

  /** Закрывает простаивающие сессии; возвращает их идентификаторы (для лога и тестов). */
  async pruneIdle(): Promise<string[]> {
    const limit = this.now() - this.idleMs;
    const expired = [...this.sessions.values()]
      .filter((entry) => entry.lastSeenAt <= limit)
      .map((entry) => entry.id);
    for (const id of expired) {
      this.log?.info('Закрытие простаивающей сессии', { session: id, idle_ms: this.idleMs });
      await this.close(id);
    }
    return expired;
  }

  /** Запускает фоновую уборку. Таймер `unref`, чтобы не держать процесс при тестах. */
  startCleanup(intervalMs = cleanupIntervalMs(this.idleMs)): void {
    if (this.timer !== null) return;
    this.timer = setInterval(() => {
      if (this.closing) return;
      void this.pruneIdle().catch((error: unknown) => {
        this.log?.warn('Уборка сессий завершилась ошибкой', { error });
      });
    }, intervalMs);
    this.timer.unref?.();
  }

  stopCleanup(): void {
    if (this.timer === null) return;
    clearInterval(this.timer);
    this.timer = null;
  }

  /** Снимок без секретов — для `/healthz`, `/readyz` и `oc_diagnostics`. */
  list(): Array<{ id: string; created_at: string; last_seen_at: string; requests: number }> {
    return [...this.sessions.values()].map((entry) => ({
      id: entry.id,
      created_at: new Date(entry.createdAt).toISOString(),
      last_seen_at: new Date(entry.lastSeenAt).toISOString(),
      requests: entry.requests,
    }));
  }
}
