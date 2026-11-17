/**
 * Тесты WP1: информация, возможности, диагностика (mcp-server-agents.md §5).
 *
 * Всё — на тестовом двойнике `fakeHttp` (никакой реальной сети): проверяем маппинг
 * `/rest/oc/info`, терпимый разбор `/rest/features`, отсутствие сетевых вызовов у
 * `oc_diagnostics`, отсутствие секретов в его ответе и работу ресурса `openconnect://info`.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { registerInfoResource, INFO_RESOURCE_URI } from '../src/resources/info.resource.js';
import { createMcpServer } from '../src/mcp/server.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { infoTools, registerInfoTools } from '../src/tools/info.tools.js';
import { registerTelemetryTools } from '../src/tools/telemetry.tools.js';
import { makeTestContext, signInRoute, type FakeRoute } from './fakeHttp.js';

/** Фикстура §F.2: API 1.10, все возможности доступны, кроме predec. */
const INFO_110 = {
  ssvc: { version: '2.5.1', api: '1.10', api_code: 110, mode: 'late_heads' },
  oc: {
    version: '0.2.9.1',
    is_support_api: true,
    is_support_release: true,
    api_min: '1.7',
    api_target: '1.10',
    api_compatibility: 'supported',
    api_features: [
      { name: 'base', min_api: '1.7', available: true },
      { name: 'get_log', min_api: '1.10', available: true },
      { name: 'predec', min_api: '1.9', available: false },
    ],
  },
};

/** Живой `/rest/features` содержит и bool, и строки — схема должна это пережить. */
const FEATURES = {
  security: false,
  mqtt: true,
  ntp: true,
  firmware_version: '0.2.9.2',
  firmware_name: 'SSVC-Open-Connect',
};

function infoRoute(info: unknown = INFO_110): FakeRoute {
  return { method: 'GET', path: '/rest/oc/info', reply: () => ({ body: info }) };
}

function featuresRoute(features: unknown = FEATURES): FakeRoute {
  return { method: 'GET', path: '/rest/features', reply: () => ({ body: features }) };
}

function tool(name: string): ToolDef {
  const found = infoTools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

test('oc_get_info: /rest/oc/info → плоская модель device', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), infoRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_info'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'успех не должен быть isError');
  const payload = payloadOf(result);

  assert.deepEqual(payload.device, {
    firmware: '0.2.9.1',
    ssvc_version: '2.5.1',
    api: '1.10',
    api_code: 110,
    mode: 'late_heads',
    compatibility: 'supported',
    is_support_release: true,
    api_min: '1.7',
    api_target: '1.10',
  });
  assert.deepEqual(payload.api_features, [
    { name: 'base', min_api: '1.7', available: true },
    { name: 'get_log', min_api: '1.10', available: true },
    { name: 'predec', min_api: '1.9', available: false },
  ]);
  assert.deepEqual(payload.unavailable, ['predec'], 'available:false должен попасть в unavailable');
  assert.equal(
    testCtx.calls.filter((call) => call.path === '/rest/oc/info').length,
    1,
    'кэш возможностей: один запрос на вызов',
  );
});

test('oc_get_info: api_code выводится из строки api, отсутствующие поля — null', async (t) => {
  // Старая прошивка: api_code и части полей oc нет вовсе.
  const legacy = {
    ssvc: { version: '2.3.1', api: '1.7', mode: 'late_heads' },
    oc: { version: '0.2.9.1' },
  };
  const testCtx = await makeTestContext([signInRoute(), infoRoute(legacy)]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_info'), testCtx.ctx, {}));
  assert.equal(payload.device.api_code, 107, '«1.7» → 107, а не сравнение строк');
  assert.equal(payload.device.compatibility, 'unknown');
  assert.equal(payload.device.is_support_release, null);
  assert.equal(payload.device.api_min, null);
  assert.equal(payload.device.api_target, null);
  assert.deepEqual(payload.api_features, []);
  assert.deepEqual(payload.unavailable, []);
});

test('oc_get_capabilities: bool-флаги отделены от строковых метаданных', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), infoRoute(), featuresRoute()]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_capabilities'), testCtx.ctx, {}));
  assert.equal(payload.include_runtime, true, 'include_runtime по умолчанию true');
  assert.deepEqual(payload.features, { security: false, mqtt: true, ntp: true });
  assert.deepEqual(payload.features_meta, {
    firmware_version: '0.2.9.2',
    firmware_name: 'SSVC-Open-Connect',
  });
  assert.deepEqual(payload.unavailable, ['predec']);
});

test('oc_get_capabilities: include_runtime:false не читает /rest/features', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), infoRoute(), featuresRoute()]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(tool('oc_get_capabilities'), testCtx.ctx, { include_runtime: false }),
  );
  assert.equal(payload.include_runtime, false);
  assert.equal(payload.features, null);
  assert.equal(payload.features_meta, null);
  assert.deepEqual(payload.unavailable, ['predec'], 'реестр API доступен и без рантайм-флагов');
  assert.equal(
    testCtx.calls.filter((call) => call.path === '/rest/features').length,
    0,
    'при include_runtime:false запроса флагов быть не должно',
  );
});

test('oc_diagnostics не делает сетевых вызовов и не содержит секретов', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), infoRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_diagnostics'), testCtx.ctx, {});
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);

  assert.equal(testCtx.calls.length, 0, 'oc_diagnostics — только локальное состояние сервера');
  assert.equal(payload.config.device.url, 'http://device.test');
  assert.equal(payload.auth.status, 'unknown', 'до первого запроса авторизация неизвестна');
  assert.equal(payload.auth.authenticated, false);
  assert.equal(payload.capability_cache.has_data, false);
  assert.equal(payload.capability_cache.age_ms, null);
  assert.equal(payload.counters.requests, 0);
  assert.deepEqual(payload.last_audit, []);

  const text = JSON.stringify(payload);
  assert.equal(text.includes(testCtx.ctx.cfg.mcp.token), false, 'токен клиентов не должен утекать');
  assert.equal(
    text.includes(testCtx.ctx.cfg.device.password),
    false,
    'пароль контроллера не должен утекать',
  );
});

test('oc_diagnostics: после запроса видит кэш возможностей, авторизацию и аудит', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), infoRoute()]);
  t.after(() => testCtx.cleanup());

  // Один запрос прогревает кэш `/rest/oc/info` и выполняет signIn.
  await runTool(tool('oc_get_info'), testCtx.ctx, {});
  const callsAfterInfo = testCtx.calls.length;

  testCtx.ctx.audit.write({ tool: 'oc_test_probe', risk: 0, result: 'ok' });
  const payload = payloadOf(await runTool(tool('oc_diagnostics'), testCtx.ctx, {}));

  assert.equal(testCtx.calls.length, callsAfterInfo, 'диагностика не должна ходить в сеть');
  assert.equal(payload.auth.status, 'ok');
  assert.equal(payload.auth.authenticated, true);
  assert.equal(payload.capability_cache.has_data, true);
  assert.equal(payload.capability_cache.firmware, '0.2.9.1');
  assert.equal(payload.capability_cache.api, '1.10');
  assert.equal(typeof payload.capability_cache.age_ms, 'number');
  assert.equal(payload.counters.requests > 0, true);
  assert.equal(payload.last_audit.length, 1);
  assert.equal(payload.last_audit[0].tool, 'oc_test_probe');
  assert.equal(payload.sessions.current, 'test');
  assert.equal(payload.sessions.seen_in_audit, 1);
});

test('registerInfoTools/registerTelemetryTools регистрируют все шесть инструментов', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  // Именно эти функции вызывает WP11 при сборке сервера сессии; проверяем их напрямую,
  // а не только список `infoTools()`.
  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerInfoTools(server, testCtx.ctx);
  registerTelemetryTools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp1-register-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(listed.tools.map((entry) => entry.name).sort(), [
    'oc_diagnostics',
    'oc_get_capabilities',
    'oc_get_info',
    'oc_get_process_status',
    'oc_get_state',
    'oc_get_telemetry',
  ]);
});

test('ресурс openconnect://info отдаёт модель и рантайм-флаги', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), infoRoute(), featuresRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerInfoResource(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp1-info-resource-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const read = await client.readResource({ uri: INFO_RESOURCE_URI });
  assert.equal(read.contents.length, 1);
  const text = (read.contents[0] as { text?: string }).text;
  assert.ok(typeof text === 'string', 'ресурс должен отдавать текст JSON');
  const payload = JSON.parse(text) as Record<string, any>;
  assert.equal(payload.device.firmware, '0.2.9.1');
  assert.deepEqual(payload.features, { security: false, mqtt: true, ntp: true });
});
