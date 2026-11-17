/**
 * HTTP-тесты сервиса (таблица §4 WP0).
 *
 * Поднимаем настоящее приложение на `127.0.0.1:0` и ходим `node:http` — проверяются
 * реальные маршруты, коды и заголовки. Устройство подменено `fakeHttp`: сети наружу нет.
 */

import assert from 'node:assert/strict';
import { request as httpRequest } from 'node:http';
import { test } from 'node:test';
import { defineTool, type ToolDef } from '../src/mcp/types.js';
import { createApp, type AppHandle } from '../src/http/app.js';
import { signInRoute, makeTestContext, TEST_MCP_TOKEN, type FakeRoute, type TestConfigPatch } from './fakeHttp.js';

const PROTOCOL_VERSION = '2025-06-18';

const INITIALIZE_REQUEST = {
  jsonrpc: '2.0',
  id: 1,
  method: 'initialize',
  params: {
    protocolVersion: PROTOCOL_VERSION,
    capabilities: {},
    clientInfo: { name: 'wp0-http-test', version: '1.0.0' },
  },
};

const PING_TOOL = defineTool({
  name: 'oc_test_ping',
  title: 'Тестовый ping',
  description: 'Отвечает pong. Ничего не меняет на контроллере.',
  inputSchema: {},
  annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  handler: async () => ({ ok: true, reply: 'pong', observed_at: '2026-01-01T00:00:00.000Z' }),
});

const INFO_FIXTURE = {
  ssvc: { version: '2.5.1', api: '1.10', api_code: 110 },
  oc: { version: '0.2.9.1', is_support_api: true, api_compatibility: 'supported' },
};

interface RawResponse {
  status: number;
  headers: Record<string, string>;
  body: string;
  json(): unknown;
}

interface Harness {
  port: number;
  app: AppHandle;
  close(): Promise<void>;
}

function normalizeHeaders(raw: NodeJS.Dict<string | string[]>): Record<string, string> {
  const out: Record<string, string> = {};
  for (const [key, value] of Object.entries(raw)) {
    if (value === undefined) continue;
    out[key.toLowerCase()] = Array.isArray(value) ? value.join(', ') : value;
  }
  return out;
}

function request(
  port: number,
  options: { method: string; path: string; headers?: Record<string, string>; body?: string },
): Promise<RawResponse> {
  return new Promise((resolve, reject) => {
    const req = httpRequest(
      {
        host: '127.0.0.1',
        port,
        method: options.method,
        path: options.path,
        headers: { connection: 'close', ...(options.headers ?? {}) },
        agent: false,
      },
      (res) => {
        const chunks: Buffer[] = [];
        res.on('data', (chunk: Buffer) => chunks.push(chunk));
        res.on('end', () => {
          const body = Buffer.concat(chunks).toString('utf8');
          const headers = normalizeHeaders(res.headers);
          resolve({
            status: res.statusCode ?? 0,
            headers,
            body,
            json(): unknown {
              return body.trim() === '' ? undefined : (JSON.parse(body) as unknown);
            },
          });
        });
      },
    );
    req.on('error', reject);
    if (options.body !== undefined) req.write(options.body);
    req.end();
  });
}

async function startApp(
  cfgPatch: TestConfigPatch = {},
  options: { tools?: ToolDef[]; routes?: FakeRoute[] } = {},
): Promise<Harness> {
  const testContext = await makeTestContext(options.routes ?? [], { cfg: cfgPatch });
  const app = await createApp({
    cfg: testContext.ctx.cfg,
    ctx: testContext.ctx,
    tools: options.tools ?? [PING_TOOL],
  });
  const { port } = await app.listen(0, '127.0.0.1');
  return {
    port,
    app,
    async close(): Promise<void> {
      await app.close();
      await testContext.cleanup();
    },
  };
}

function mcpHeaders(extra: Record<string, string> = {}): Record<string, string> {
  return {
    'content-type': 'application/json',
    accept: 'application/json, text/event-stream',
    authorization: `Bearer ${TEST_MCP_TOKEN}`,
    ...extra,
  };
}

function postMcp(
  harness: Harness,
  payload: unknown,
  extraHeaders: Record<string, string> = {},
): Promise<RawResponse> {
  return request(harness.port, {
    method: 'POST',
    path: '/mcp',
    headers: mcpHeaders(extraHeaders),
    body: JSON.stringify(payload),
  });
}

/** Первый `initialize`: возвращает идентификатор сессии. */
async function initialize(harness: Harness): Promise<string> {
  const response = await postMcp(harness, INITIALIZE_REQUEST);
  assert.equal(response.status, 200, `initialize должен вернуть 200, тело: ${response.body}`);
  const sessionId = response.headers['mcp-session-id'];
  assert.ok(sessionId !== undefined && sessionId !== '', 'initialize обязан вернуть Mcp-Session-Id');
  return sessionId;
}

async function waitFor(condition: () => boolean, timeoutMs = 2_000): Promise<void> {
  const started = Date.now();
  while (!condition()) {
    if (Date.now() - started > timeoutMs) throw new Error('Условие не выполнилось за отведённое время');
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
}

test('initialize создаёт сессию, повтор с Mcp-Session-Id идёт в ту же сессию', async (t) => {
  const harness = await startApp();
  t.after(() => harness.close());

  const initResponse = await postMcp(harness, INITIALIZE_REQUEST);
  assert.equal(initResponse.status, 200, initResponse.body);
  const sessionId = initResponse.headers['mcp-session-id'];
  assert.ok(sessionId !== undefined && sessionId !== '', 'initialize обязан вернуть Mcp-Session-Id');
  assert.equal(harness.app.sessions.size, 1);

  const initBody = initResponse.json() as { result?: { serverInfo?: { name?: string } } };
  assert.equal(initBody.result?.serverInfo?.name, 'ssvc-openconnect-mcp');

  const first = await postMcp(
    harness,
    { jsonrpc: '2.0', id: 2, method: 'tools/list', params: {} },
    { 'mcp-session-id': sessionId, 'mcp-protocol-version': PROTOCOL_VERSION },
  );
  assert.equal(first.status, 200, first.body);
  const listed = first.json() as { result?: { tools?: Array<{ name?: string }> } };
  assert.deepEqual(
    listed.result?.tools?.map((tool) => tool.name),
    ['oc_test_ping'],
  );

  // Тот же Mcp-Session-Id — та же сессия: новая не создаётся.
  const second = await postMcp(
    harness,
    { jsonrpc: '2.0', id: 3, method: 'tools/list', params: {} },
    { 'mcp-session-id': sessionId },
  );
  assert.equal(second.status, 200, second.body);
  assert.equal(harness.app.sessions.size, 1, 'повторный запрос не должен создавать сессию');
  const entries = harness.app.sessions.list();
  assert.equal(entries.length, 1);
  assert.equal(entries[0]?.id, sessionId);
  assert.ok((entries[0]?.requests ?? 0) >= 3, 'initialize + два запроса учтены в счётчике');
});

test('без токена и с неверным токеном — 401 и WWW-Authenticate', async (t) => {
  const harness = await startApp();
  t.after(() => harness.close());

  const noToken = await request(harness.port, {
    method: 'POST',
    path: '/mcp',
    headers: {
      'content-type': 'application/json',
      accept: 'application/json, text/event-stream',
    },
    body: JSON.stringify(INITIALIZE_REQUEST),
  });
  assert.equal(noToken.status, 401);
  assert.match(noToken.headers['www-authenticate'] ?? '', /Bearer/);
  const body = noToken.json() as { ok?: boolean; error?: { code?: string } };
  assert.equal(body.ok, false);
  assert.equal(body.error?.code, 'unauthorized');

  const wrongToken = await postMcp(
    harness,
    INITIALIZE_REQUEST,
    { authorization: 'Bearer not-the-token' },
  );
  assert.equal(wrongToken.status, 401);
  assert.equal(harness.app.sessions.size, 0, 'отказ не должен создавать сессию');
});

test('чужой Host и чужой Origin — 403', async (t) => {
  const harness = await startApp();
  t.after(() => harness.close());

  const foreignHost = await postMcp(harness, INITIALIZE_REQUEST, { host: 'attacker.example.com' });
  assert.equal(foreignHost.status, 403, foreignHost.body);
  const hostBody = foreignHost.json() as { error?: { code?: string; details?: { host?: string } } };
  assert.equal(hostBody.error?.code, 'forbidden');
  assert.equal(hostBody.error?.details?.host, 'attacker.example.com');

  // Origin проверяется только у браузерных запросов; чужой Origin — отказ.
  const foreignOrigin = await postMcp(harness, INITIALIZE_REQUEST, {
    origin: 'http://attacker.example.com',
  });
  assert.equal(foreignOrigin.status, 403, foreignOrigin.body);
  assert.equal((foreignOrigin.json() as { error?: { code?: string } }).error?.code, 'forbidden');

  assert.equal(harness.app.sessions.size, 0);
});

test('неизвестная сессия — 404, запрос без сессии на не-init — 400', async (t) => {
  const harness = await startApp();
  t.after(() => harness.close());

  const unknownSession = await postMcp(
    harness,
    { jsonrpc: '2.0', id: 2, method: 'tools/list', params: {} },
    { 'mcp-session-id': '00000000-0000-0000-0000-000000000000' },
  );
  assert.equal(unknownSession.status, 404, unknownSession.body);
  assert.match(String((unknownSession.json() as { error?: { message?: string } }).error?.message), /Сессия/);

  const noSession = await postMcp(harness, {
    jsonrpc: '2.0',
    id: 3,
    method: 'tools/list',
    params: {},
  });
  assert.equal(noSession.status, 400, noSession.body);

  const deleteWithoutSession = await request(harness.port, {
    method: 'DELETE',
    path: '/mcp',
    headers: mcpHeaders(),
  });
  assert.equal(deleteWithoutSession.status, 400);
});

test('DELETE закрывает сессию: повтор с тем же идентификатором — 404', async (t) => {
  const harness = await startApp();
  t.after(() => harness.close());

  const sessionId = await initialize(harness);
  assert.equal(harness.app.sessions.size, 1);

  const closed = await request(harness.port, {
    method: 'DELETE',
    path: '/mcp',
    headers: mcpHeaders({ 'mcp-session-id': sessionId }),
  });
  assert.equal(closed.status, 200, closed.body);
  await waitFor(() => harness.app.sessions.size === 0);

  const afterClose = await postMcp(
    harness,
    { jsonrpc: '2.0', id: 4, method: 'tools/list', params: {} },
    { 'mcp-session-id': sessionId },
  );
  assert.equal(afterClose.status, 404, afterClose.body);
});

test('лимит сессий исчерпан — новый initialize получает 503', async (t) => {
  const harness = await startApp({ mcp: { maxSessions: 1 } });
  t.after(() => harness.close());

  const sessionId = await initialize(harness);
  assert.ok(sessionId !== '');
  assert.equal(harness.app.sessions.size, 1);

  const second = await postMcp(harness, { ...INITIALIZE_REQUEST, id: 2 });
  assert.equal(second.status, 503, second.body);
  assert.match(String((second.json() as { error?: { message?: string } }).error?.message), /сессий/i);
  assert.equal(harness.app.sessions.size, 1, 'отказ по лимиту не должен трогать существующую сессию');
});

test('GET /healthz отвечает 200 без токена и без Origin', async (t) => {
  const harness = await startApp();
  t.after(() => harness.close());

  const health = await request(harness.port, { method: 'GET', path: '/healthz' });
  assert.equal(health.status, 200, health.body);
  const body = health.json() as { ok?: boolean; status?: string; sessions?: number };
  assert.equal(body.ok, true);
  assert.equal(body.status, 'alive');
  assert.equal(body.sessions, 0);
});

test('GET /readyz: с токеном проверяет /rest/oc/info, без токена — 401', async (t) => {
  const harness = await startApp(
    {},
    {
      routes: [
        signInRoute(),
        { method: 'GET', path: '/rest/oc/info', reply: () => ({ status: 200, body: INFO_FIXTURE }) },
      ],
    },
  );
  t.after(() => harness.close());

  const unauthorized = await request(harness.port, { method: 'GET', path: '/readyz' });
  assert.equal(unauthorized.status, 401);

  const ready = await request(harness.port, {
    method: 'GET',
    path: '/readyz',
    headers: { authorization: `Bearer ${TEST_MCP_TOKEN}` },
  });
  assert.equal(ready.status, 200, ready.body);
  const body = ready.json() as {
    ok?: boolean;
    ready?: boolean;
    device?: { ssvc?: string; api?: string; firmware?: string };
  };
  assert.equal(body.ok, true);
  assert.equal(body.ready, true);
  assert.equal(body.device?.ssvc, '2.5.1');
  assert.equal(body.device?.api, '1.10');
  assert.equal(body.device?.firmware, '0.2.9.1');
});

test('GET /readyz при недоступном устройстве — 503 с конвертом ошибки', async (t) => {
  const harness = await startApp(
    {},
    {
      routes: [
        signInRoute(),
        { method: 'GET', path: '/rest/oc/info', reply: () => ({ status: 500, body: { error: 'boom' } }) },
      ],
    },
  );
  t.after(() => harness.close());

  const ready = await request(harness.port, {
    method: 'GET',
    path: '/readyz',
    headers: { authorization: `Bearer ${TEST_MCP_TOKEN}` },
  });
  assert.equal(ready.status, 503, ready.body);
  const body = ready.json() as { ok?: boolean; ready?: boolean; error?: { code?: string } };
  assert.equal(body.ok, false);
  assert.equal(body.ready, false);
  assert.equal(body.error?.code, 'device_error');
});

test('GET / и неизвестный путь — 404 JSON с подсказкой', async (t) => {
  const harness = await startApp();
  t.after(() => harness.close());

  const root = await request(harness.port, { method: 'GET', path: '/' });
  assert.equal(root.status, 404);
  const body = root.json() as { ok?: boolean; error?: { code?: string; hint?: string } };
  assert.equal(body.ok, false);
  assert.equal(body.error?.code, 'not_found');
  assert.match(String(body.error?.hint), /\/mcp/);

  const unknown = await request(harness.port, { method: 'GET', path: '/admin' });
  assert.equal(unknown.status, 404);
  assert.equal((unknown.json() as { error?: { code?: string } }).error?.code, 'not_found');
});
