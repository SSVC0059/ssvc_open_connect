/**
 * Тестовый двойник устройства (контракт §2.7).
 *
 * Полностью в памяти: ни одного реального сокета, диск — только временный каталог
 * (сырые логи и аудит), который удаляется в `cleanup()`.
 *
 * `fakeHttp` подменяет `HttpLike` из `src/device/http.ts`: `DeviceClient` и `AuthSession`
 * работают ровно тем же кодом, что и в бою, но отвечает им таблица маршрутов.
 * Маршрут `POST /rest/signIn` удобно добавлять через `signInRoute()`.
 */

import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { AuditLog } from '../src/audit.js';
import { DEFAULTS, type Config } from '../src/config.js';
import { CapabilityCache } from '../src/device/CapabilityCache.js';
import { DeviceClient } from '../src/device/DeviceClient.js';
import type { HttpLike, HttpRequestOptions, HttpResult } from '../src/device/http.js';
import { RawLogger } from '../src/device/RawLogger.js';
import { silentLogger, type Logger } from '../src/logging.js';
import type { ToolContext } from '../src/mcp/types.js';

export interface FakeCall {
  method: string;
  path: string;
  query: Record<string, string>;
  body: unknown;
  headers: Record<string, string>;
}

export interface FakeReply {
  status?: number;
  body?: unknown;
  headers?: Record<string, string>;
}

export type FakeRoute = {
  method: 'GET' | 'POST' | 'PUT' | 'DELETE';
  path: string | RegExp;
  reply: (call: FakeCall) => FakeReply | Promise<FakeReply>;
};

export interface FakeHttp {
  http: HttpLike;
  calls: FakeCall[];
  addRoute(route: FakeRoute): void;
}

/** Токен MCP-клиента, который используют тесты (не секрет). */
export const TEST_MCP_TOKEN = 'test-mcp-token';
/** JWT, который «выдаёт» контроллер в тестах. */
export const TEST_JWT = 'test-jwt-token';

function normalizeHeaders(headers: Record<string, string> | undefined): Record<string, string> {
  const out: Record<string, string> = {};
  for (const [key, value] of Object.entries(headers ?? {})) out[key.toLowerCase()] = value;
  return out;
}

/** Тело запроса: JSON разбираем, остальное отдаём строкой (как есть). */
function parseRequestBody(body: string | undefined): unknown {
  if (body === undefined) return undefined;
  const trimmed = body.trim();
  if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
    try {
      return JSON.parse(trimmed) as unknown;
    } catch {
      return body;
    }
  }
  return body;
}

function shapeReply(reply: FakeReply): HttpResult {
  const headers = normalizeHeaders(reply.headers);
  let body: string;
  if (reply.body === undefined) {
    body = '';
  } else if (typeof reply.body === 'string') {
    body = reply.body;
  } else {
    body = JSON.stringify(reply.body);
  }
  if (headers['content-type'] === undefined) {
    headers['content-type'] =
      typeof reply.body === 'string' ? 'text/plain; charset=utf-8' : 'application/json; charset=utf-8';
  }
  return { status: reply.status ?? 200, headers, body, ms: 0 };
}

/**
 * Создаёт поддельный `HttpLike` по таблице маршрутов.
 * Нет совпадения — `404` с JSON-телом (как у прошивки на неизвестном пути).
 */
export function fakeHttp(routes: FakeRoute[] = []): FakeHttp {
  const calls: FakeCall[] = [];
  const registered = [...routes];

  const http: HttpLike = {
    async request(input: HttpRequestOptions): Promise<HttpResult> {
      const url = new URL(input.url);
      const headers = normalizeHeaders(input.headers);
      const call: FakeCall = {
        method: input.method.toUpperCase(),
        path: url.pathname,
        query: Object.fromEntries(url.searchParams.entries()),
        body: parseRequestBody(input.body),
        headers,
      };
      calls.push(call);

      const route = registered.find((candidate) => {
        if (candidate.method !== call.method) return false;
        return typeof candidate.path === 'string'
          ? candidate.path === call.path
          : candidate.path.test(call.path);
      });
      if (route === undefined) {
        return shapeReply({
          status: 404,
          body: { error: 'not_found', path: call.path },
        });
      }
      return shapeReply(await route.reply(call));
    },
  };

  return {
    http,
    calls,
    addRoute(route: FakeRoute): void {
      registered.push(route);
    },
  };
}

/** Готовый маршрут входа: контроллер выдаёт `accessToken`. */
export function signInRoute(accessToken = TEST_JWT): FakeRoute {
  return {
    method: 'POST',
    path: '/rest/signIn',
    reply: (call) => {
      const body = call.body as { username?: unknown; password?: unknown } | undefined;
      if (typeof body?.username !== 'string' || typeof body?.password !== 'string') {
        return { status: 400, body: { error: 'bad_request' } };
      }
      return { status: 200, body: { access_token: accessToken } };
    },
  };
}

/** Патч конфига для тестов: вложенные секции сливаются, а не заменяются целиком. */
export type TestConfigPatch = Partial<Omit<Config, 'device' | 'mcp' | 'raw'>> & {
  device?: Partial<Config['device']>;
  mcp?: Partial<Config['mcp']>;
  raw?: Partial<Config['raw']>;
};

/** Конфиг для тестов: обязательные поля заполнены, сеть и диск не используются. */
export function testConfig(patch: TestConfigPatch = {}): Config {
  const base: Config = {
    device: {
      url: 'http://device.test',
      user: 'admin',
      password: 'test-password',
      timeoutMs: 1_000,
      pollHz: 1,
    },
    mcp: {
      host: '127.0.0.1',
      port: 0,
      token: TEST_MCP_TOKEN,
      allowedHosts: [],
      allowedCidrs: ['private'],
      tls: null,
      maxSessions: 32,
      idleMs: 30 * 60 * 1000,
    },
    raw: { mode: 'off', dir: './logs/raw', maxMb: 1 },
    allowDestructive: false,
    allowDebug: false,
    auditPath: './logs/audit.jsonl',
    logLevel: 'silent',
  };
  return {
    ...base,
    ...patch,
    device: { ...base.device, ...(patch.device ?? {}) },
    mcp: { ...base.mcp, ...(patch.mcp ?? {}) },
    raw: { ...base.raw, ...(patch.raw ?? {}) },
  };
}

export interface TestContext {
  ctx: ToolContext;
  calls: FakeCall[];
  dir: string;
  http: HttpLike;
  cleanup(): Promise<void>;
}

export interface MakeTestContextOverrides {
  cfg?: TestConfigPatch;
  allowDestructive?: boolean;
  log?: Logger;
}

/**
 * Готовый `ToolContext` с фейковым устройством, выключенным сырым логом и временным каталогом.
 * Тесты обязаны вызывать `cleanup()` (например, в `t.after`).
 */
export async function makeTestContext(
  routes: FakeRoute[] = [],
  overrides: MakeTestContextOverrides = {},
): Promise<TestContext> {
  const dir = await mkdtemp(join(tmpdir(), 'ssvc-mcp-test-'));
  const cfg = testConfig(overrides.cfg);
  if (overrides.allowDestructive !== undefined) cfg.allowDestructive = overrides.allowDestructive;
  // Всё «дисковое» уводим во временный каталог, если тест не задал своё.
  cfg.raw = { ...cfg.raw, dir: cfg.raw.dir === DEFAULTS.rawDir ? join(dir, 'raw') : cfg.raw.dir };
  cfg.auditPath =
    cfg.auditPath === DEFAULTS.auditPath ? join(dir, 'audit.jsonl') : cfg.auditPath;

  const log = overrides.log ?? silentLogger();
  const fake = fakeHttp(routes);
  const raw = new RawLogger({ mode: cfg.raw.mode, dir: cfg.raw.dir, maxMb: cfg.raw.maxMb, log });
  const audit = new AuditLog({ path: cfg.auditPath, session: 'test', log });
  const client = new DeviceClient({
    cfg,
    http: fake.http,
    raw,
    log,
    retry: { attempts: 3, baseDelayMs: 0, maxDelayMs: 0 },
  });
  const caps = new CapabilityCache(client, { log });

  const ctx: ToolContext = { cfg, client, caps, log, audit, raw };

  return {
    ctx,
    calls: fake.calls,
    dir,
    http: fake.http,
    async cleanup(): Promise<void> {
      await client.close();
      await raw.close();
      await audit.close();
      await rm(dir, { recursive: true, force: true, maxRetries: 3, retryDelay: 50 });
    },
  };
}
