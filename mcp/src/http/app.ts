/**
 * HTTP-приложение сервиса (контракт §3.4, §4 WP0): `node:http`, три маршрута.
 *
 * Почему без веб-фреймворка: маршрутов три, а любая прослойка добавляет зависимость
 * и второй слой разбора тела запроса (см. mcp-server.md §3.4).
 *
 * Маршруты:
 *  - `POST|GET|DELETE /mcp` — Streamable HTTP транспорт, stateful;
 *  - `GET /healthz` — «процесс жив», без токена (HEALTHCHECK контейнера);
 *  - `GET /readyz` — с токеном, проверяет `GET /rest/oc/info` на контроллере;
 *  - `GET /` и всё остальное — `404` JSON.
 *
 * Коды протокола: неизвестная сессия — `404`; не-init запрос без `Mcp-Session-Id` — `400`;
 * `initialize` при исчерпанном лимите — `503`.
 */

import { randomUUID } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { createServer as createHttpServer, type IncomingMessage, type Server, type ServerResponse } from 'node:http';
import { createServer as createHttpsServer } from 'node:https';
import { StreamableHTTPServerTransport } from '@modelcontextprotocol/sdk/server/streamableHttp.js';
import { isInitializeRequest } from '@modelcontextprotocol/sdk/types.js';
import { AuditLog } from '../audit.js';
import type { Config } from '../config.js';
import { ocInfoSchema } from '../device/schemas/info.js';
import { errorEnvelope, MAX_DETAILS_BODY, truncateBody } from '../errors.js';
import { createMcpServer } from '../mcp/server.js';
import type { ToolContext, ToolDef } from '../mcp/types.js';
import { registerSessionCatalog } from '../tools/index.js';
import { authorize, checkNetwork, createAccessPolicy, type AccessFailure, type AccessPolicy } from './auth.js';
import { normalizeIp } from './net.js';
import { SessionManager } from './sessions.js';

export const MCP_PATH = '/mcp';
export const HEALTH_PATH = '/healthz';
export const READY_PATH = '/readyz';

/** Предел тела JSON-RPC: MCP-запросы маленькие, 4 МБ — с большим запасом. */
export const MAX_REQUEST_BODY_BYTES = 4 * 1024 * 1024;

export interface AppOptions {
  cfg: Config;
  ctx: ToolContext;
  /** Инструменты для регистрации в каждой сессии. WP0 передаёт пустой список. */
  tools?: ToolDef[];
  /** Готовая политика доступа (тесты могут подменить); по умолчанию строится из конфига. */
  policy?: AccessPolicy;
  sessions?: SessionManager;
}

export interface AppHandle {
  server: Server;
  sessions: SessionManager;
  policy: AccessPolicy;
  /** Поднимает сервер и возвращает фактический адрес (порт 0 → выбранный ОС). */
  listen(port?: number, host?: string): Promise<{ host: string; port: number }>;
  close(): Promise<void>;
}

const JSON_HEADERS: Record<string, string> = {
  'content-type': 'application/json; charset=utf-8',
  'cache-control': 'no-store',
  'x-content-type-options': 'nosniff',
};

function sendJson(
  res: ServerResponse,
  status: number,
  body: unknown,
  extraHeaders: Record<string, string> = {},
): void {
  const text = JSON.stringify(body);
  res.writeHead(status, {
    ...JSON_HEADERS,
    'content-length': Buffer.byteLength(text, 'utf8').toString(),
    ...extraHeaders,
  });
  res.end(text);
}

/** Ответ уровня протокола: JSON-RPC-ошибка, чтобы MCP-клиент понял причину. */
function sendProtocolError(
  res: ServerResponse,
  status: number,
  message: string,
  data?: Record<string, unknown>,
): void {
  sendJson(
    res,
    status,
    {
      jsonrpc: '2.0',
      error: {
        code: status === 400 ? -32600 : -32000,
        message,
        ...(data === undefined ? {} : { data }),
      },
      id: null,
    },
    status === 503 ? { 'retry-after': '5' } : {},
  );
}

function sendAccessFailure(res: ServerResponse, failure: AccessFailure): void {
  sendJson(
    res,
    failure.status,
    {
      ok: false,
      error: {
        code: failure.code,
        message: failure.message,
        ...(failure.hint === undefined ? {} : { hint: failure.hint }),
        ...(failure.details === undefined ? {} : { details: failure.details }),
      },
    },
    failure.headers ?? {},
  );
}

function headerOf(req: IncomingMessage, name: string): string | undefined {
  const value = req.headers[name.toLowerCase()];
  if (value === undefined) return undefined;
  return Array.isArray(value) ? value.join(', ') : value;
}

/** Читает тело запроса с пределом; при превышении — `413`. */
function readBody(req: IncomingMessage, limit = MAX_REQUEST_BODY_BYTES): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let size = 0;
    req.on('data', (chunk: Buffer) => {
      size += chunk.length;
      if (size > limit) {
        reject(new Error(`Тело запроса больше ${limit} байт`));
        req.destroy();
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    req.on('error', reject);
  });
}

function originOf(req: IncomingMessage): string {
  const protocol = (req.socket as { encrypted?: boolean }).encrypted === true ? 'https' : 'http';
  return `${protocol}://${headerOf(req, 'host') ?? 'localhost'}`;
}

/**
 * Создаёт HTTP(S)-сервер. Слушание — отдельным методом `listen`, чтобы тесты
 * могли занять порт 0 и не зависеть от занятых портов на машине.
 */
export async function createApp(opts: AppOptions): Promise<AppHandle> {
  const { cfg, ctx } = opts;
  const log = ctx.log;
  const policy = opts.policy ?? createAccessPolicy(cfg);
  const sessions =
    opts.sessions ??
    new SessionManager({
      maxSessions: cfg.mcp.maxSessions,
      idleMs: cfg.mcp.idleMs,
      log,
    });
  const startedAt = Date.now();

  if (policy.allowedHosts.length === 0) {
    log.warn('SSVC_MCP_ALLOWED_HOSTS пуст: проверка Host/Origin отключена');
  }
  if (cfg.mcp.host === '0.0.0.0' && cfg.mcp.allowedHosts.length === 0) {
    log.warn(
      'Сервис слушает 0.0.0.0 без SSVC_MCP_ALLOWED_HOSTS: клиенты по IP получат 403 ' +
        'из-за проверки Host. Укажите имя хоста/IP в SSVC_MCP_ALLOWED_HOSTS.',
    );
  }

  /** Создаёт MCP-сервер и транспорт для новой сессии. */
  const newTransport = async (): Promise<StreamableHTTPServerTransport> => {
    // Контекст инструментов — на сессию (§2.2): свой журнал аудита, чтобы поле `session`
    // в записи указывало, чей это вызов. Клиент устройства и кэш — общие: контроллер один.
    const sessionAudit = new AuditLog({ path: cfg.auditPath, session: 'pending', log });
    const sessionCtx: ToolContext = { ...ctx, audit: sessionAudit };
    const server = createMcpServer({ ctx: sessionCtx, tools: opts.tools ?? [] });
    // Промпты-сценарии (WP10) и ресурсы openconnect://… (WP1): одна точка входа —
    // `registerSessionCatalog` из каталога инструментов. Промпты здесь больше не
    // регистрируются отдельно: иначе двойная регистрация при подключении ресурсов.
    registerSessionCatalog(server, sessionCtx);
    let transport!: StreamableHTTPServerTransport;
    transport = new StreamableHTTPServerTransport({
      sessionIdGenerator: () => randomUUID(),
      // POST отвечает JSON: SSE нужен только для GET-потока уведомлений.
      enableJsonResponse: true,
      onsessioninitialized: (sessionId: string) => {
        sessionAudit.session = sessionId;
        const accepted = sessions.register(sessionId, {
          server,
          transport,
          dispose: () => sessionAudit.close(),
        });
        if (!accepted) {
          // Гонка двух одновременных initialize: место кончилось уже после проверки.
          void transport.close();
          void sessionAudit.close();
        }
      },
      onsessionclosed: (sessionId: string) => {
        // `close` освободит журнал аудита через `dispose`.
        void sessions.close(sessionId);
      },
    });
    transport.onerror = (error: Error) => {
      log.warn('Ошибка транспорта MCP', { error: error.message });
    };
    await server.connect(transport);
    return transport;
  };

  const handleMcp = async (req: IncomingMessage, res: ServerResponse): Promise<void> => {
    const method = (req.method ?? 'GET').toUpperCase();
    if (method !== 'POST' && method !== 'GET' && method !== 'DELETE') {
      sendJson(
        res,
        405,
        { ok: false, error: { code: 'not_found', message: 'Метод не поддерживается на /mcp' } },
        { allow: 'GET, POST, DELETE' },
      );
      return;
    }

    const sessionId = headerOf(req, 'mcp-session-id');

    if (method === 'POST') {
      let raw: string;
      try {
        raw = await readBody(req);
      } catch (error) {
        sendProtocolError(res, 413, 'Тело запроса слишком большое', {
          limit_bytes: MAX_REQUEST_BODY_BYTES,
        });
        void error;
        return;
      }

      let parsed: unknown;
      try {
        parsed = raw.trim() === '' ? undefined : (JSON.parse(raw) as unknown);
      } catch {
        sendProtocolError(res, 400, 'Тело запроса не является корректным JSON');
        return;
      }

      if (sessionId !== undefined && sessionId !== '') {
        const entry = sessions.touch(sessionId);
        if (entry === undefined) {
          sendProtocolError(res, 404, 'Сессия MCP не найдена: пересоздайте её через initialize');
          return;
        }
        await entry.transport.handleRequest(req, res, parsed);
        return;
      }

      if (Array.isArray(parsed) || !isInitializeRequest(parsed)) {
        sendProtocolError(
          res,
          400,
          'Заголовок Mcp-Session-Id обязателен для всех запросов, кроме initialize',
        );
        return;
      }

      if (!sessions.hasCapacity()) {
        log.warn('Отказ в новой сессии: исчерпан лимит', { max_sessions: sessions.maxSessions });
        sendProtocolError(res, 503, 'Слишком много активных сессий MCP: повторите позже');
        return;
      }

      const transport = await newTransport();
      try {
        await transport.handleRequest(req, res, parsed);
      } finally {
        // initialize мог быть отклонён (битые параметры) — тогда сессия не родилась.
        if (transport.sessionId === undefined) {
          await transport.close().catch(() => undefined);
        }
      }
      return;
    }

    // GET (SSE) и DELETE требуют существующую сессию.
    if (sessionId === undefined || sessionId === '') {
      sendProtocolError(res, 400, 'Заголовок Mcp-Session-Id обязателен');
      return;
    }
    const entry = sessions.touch(sessionId);
    if (entry === undefined) {
      sendProtocolError(res, 404, 'Сессия MCP не найдена: пересоздайте её через initialize');
      return;
    }
    await entry.transport.handleRequest(req, res);
  };

  const handleHealthz = (req: IncomingMessage, res: ServerResponse): void => {
    const failure = checkNetwork(policy, { remoteAddress: req.socket.remoteAddress, headers: req.headers });
    if (failure !== null) {
      sendAccessFailure(res, failure);
      return;
    }
    const body = {
      ok: true,
      status: 'alive',
      service: 'ssvc-openconnect-mcp',
      uptime_s: Math.round((Date.now() - startedAt) / 1000),
      sessions: sessions.size,
    };
    if ((req.method ?? 'GET').toUpperCase() === 'HEAD') {
      res.writeHead(200, JSON_HEADERS);
      res.end();
      return;
    }
    sendJson(res, 200, body);
  };

  const handleReadyz = async (req: IncomingMessage, res: ServerResponse): Promise<void> => {
    const failure = authorize(policy, { remoteAddress: req.socket.remoteAddress, headers: req.headers });
    if (failure !== null) {
      sendAccessFailure(res, failure);
      return;
    }
    try {
      const info = await ctx.client.get('/rest/oc/info', ocInfoSchema);
      sendJson(res, 200, {
        ok: true,
        ready: true,
        device: {
          url: ctx.client.deviceUrl,
          ssvc: info.ssvc.version,
          api: info.ssvc.api,
          firmware: info.oc.version,
          compatibility: info.oc.api_compatibility ?? 'unknown',
        },
        sessions: sessions.size,
      });
    } catch (error) {
      const envelope = errorEnvelope(error);
      log.warn('Проверка готовности не прошла', { error: envelope.error.code });
      sendJson(res, 503, {
        ok: false,
        ready: false,
        error: envelope.error,
        device: { url: ctx.client.deviceUrl },
      });
    }
  };

  const route = async (req: IncomingMessage, res: ServerResponse): Promise<void> => {
    const url = new URL(req.url ?? '/', originOf(req));
    const pathname = url.pathname;
    const method = (req.method ?? 'GET').toUpperCase();

    if (pathname === MCP_PATH) {
      const failure = authorize(policy, {
        remoteAddress: req.socket.remoteAddress,
        headers: req.headers,
      });
      if (failure !== null) {
        log.warn('Отказ в доступе к /mcp', {
          status: failure.status,
          remote: req.socket.remoteAddress === undefined ? null : normalizeIp(req.socket.remoteAddress),
        });
        sendAccessFailure(res, failure);
        return;
      }
      await handleMcp(req, res);
      return;
    }

    if (pathname === HEALTH_PATH) {
      if (method !== 'GET' && method !== 'HEAD') {
        sendJson(res, 405, { ok: false, error: { code: 'not_found', message: 'Метод не поддерживается' } }, {
          allow: 'GET, HEAD',
        });
        return;
      }
      handleHealthz(req, res);
      return;
    }

    if (pathname === READY_PATH) {
      if (method !== 'GET') {
        sendJson(res, 405, { ok: false, error: { code: 'not_found', message: 'Метод не поддерживается' } }, {
          allow: 'GET',
        });
        return;
      }
      await handleReadyz(req, res);
      return;
    }

    // `GET /` и любой неизвестный путь — 404 JSON (а не HTML-заглушка).
    sendJson(res, 404, {
      ok: false,
      error: {
        code: 'not_found',
        message: 'Маршрут не найден',
        hint: `Доступны: POST/GET/DELETE ${MCP_PATH} (MCP), GET ${HEALTH_PATH}, GET ${READY_PATH}.`,
        details: { method, path: pathname },
      },
    });
  };

  const listener = (req: IncomingMessage, res: ServerResponse): void => {
    void route(req, res).catch((error: unknown) => {
      log.error('Необработанная ошибка HTTP-запроса', { error });
      if (res.headersSent) {
        res.destroy();
        return;
      }
      sendJson(res, 500, {
        ok: false,
        error: {
          code: 'internal_error',
          message: 'Внутренняя ошибка MCP-сервера при обработке запроса',
          hint: 'Это дефект сервера, а не отказ контроллера. Смотрите лог сервиса.',
          details: { body: truncateBody(String(error), MAX_DETAILS_BODY) },
        },
      });
    });
  };

  let server: Server;
  if (cfg.mcp.tls === null) {
    server = createHttpServer(listener);
  } else {
    server = createHttpsServer(
      {
        cert: readFileSync(cfg.mcp.tls.cert),
        key: readFileSync(cfg.mcp.tls.key),
      },
      listener,
    );
  }
  server.requestTimeout = 0; // SSE-поток живёт долго, таймаут запроса задаёт транспорт
  server.headersTimeout = 60_000;

  sessions.startCleanup();

  return {
    server,
    sessions,
    policy,
    listen(port = cfg.mcp.port, host = cfg.mcp.host): Promise<{ host: string; port: number }> {
      return new Promise((resolve, reject) => {
        const onError = (error: Error): void => {
          server.removeListener('listening', onListening);
          reject(error);
        };
        const onListening = (): void => {
          server.removeListener('error', onError);
          const address = server.address();
          const actualPort = typeof address === 'object' && address !== null ? address.port : port;
          log.info('MCP-сервер слушает', { host, port: actualPort, tls: cfg.mcp.tls !== null });
          resolve({ host, port: actualPort });
        };
        server.once('error', onError);
        server.once('listening', onListening);
        server.listen(port, host);
      });
    },
    async close(): Promise<void> {
      sessions.stopCleanup();
      await sessions.closeAll();
      await new Promise<void>((resolve, reject) => {
        server.close((error) => (error === undefined ? resolve() : reject(error)));
      });
    },
  };
}
