/**
 * Тесты WP9: подсистемы и система (mcp-server-agents.md §13).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети. Проверяются:
 *  - `GET /rest/subsystem` — словарь bool с фактическим набором ключей;
 *  - `PUT /rest/subsystem` — ровно `{name: enabled}`, таймаут 120 000 мс (Telegram/VK)
 *    и обязательное перечитывание карты;
 *  - `success:false` + `errors[]` — это данные, а не исключение;
 *  - `GET /rest/systemStatus` — маппинг `free_heap` → `heap` и т. д.;
 *  - `POST /rest/restart` — 200 → `ok:true` и `hint` про 20–40 секунд и `/readyz`;
 *  - риск 3 перезагрузки: confirm → reason → SSVC_ALLOW_DESTRUCTIVE до сети.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { SUBSYSTEM_TIMEOUT_MS } from '../src/domain/system.js';
import { createMcpServer } from '../src/mcp/server.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { registerSystemTools, systemTools } from '../src/tools/system.tools.js';
import {
  makeTestContext,
  signInRoute,
  type FakeCall,
  type FakeRoute,
} from './fakeHttp.js';

function tool(name: string): ToolDef {
  const found = systemTools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

function callsFor(calls: FakeCall[], method: string, path: string | RegExp): FakeCall[] {
  return calls.filter(
    (call) =>
      call.method === method && (typeof path === 'string' ? call.path === path : path.test(call.path)),
  );
}

interface SubsystemState {
  map: Record<string, boolean>;
}

function subsystemState(): SubsystemState {
  return { map: { i2c_bus: true, thermal: true, atm_sensor: true, telegram_bot: false } };
}

function subsystemGetRoute(state: SubsystemState): FakeRoute {
  return { method: 'GET', path: '/rest/subsystem', reply: () => ({ body: state.map }) };
}

const SYSTEM_STATUS_FIXTURE = {
  esp_platform: 'ESP32-S3',
  firmware_version: '0.2.9.2',
  max_alloc_heap: 350000,
  free_psram: 4000000,
  used_psram: 4000000,
  psram_size: 8000000,
  cpu_freq_mhz: 240,
  cpu_type: 'ESP32-S3',
  cpu_rev: 1,
  cpu_cores: 2,
  free_heap: 123456,
  min_free_heap: 100000,
  used_heap: 77777,
  total_heap: 201233,
  sketch_size: 1500000,
  free_sketch_space: 5000000,
  flash_chip_size: 16777216,
  flash_chip_speed: 80000000,
  fs_total: 1500000,
  fs_used: 200000,
  core_temp: 45.5,
  uptime: 3600,
  cpu_reset_reason: 'SW_CPU_RESET',
};

test('systemTools: четыре инструмента WP9 с аннотациями риска', () => {
  const tools = systemTools();
  assert.deepEqual(
    tools.map((candidate) => candidate.name),
    ['oc_get_subsystems', 'oc_set_subsystem', 'oc_system_status', 'oc_restart'],
  );
  assert.equal(tool('oc_get_subsystems').annotations.readOnlyHint, true);
  assert.equal(tool('oc_set_subsystem').annotations.readOnlyHint, false);
  assert.equal(tool('oc_set_subsystem').annotations.destructiveHint, false);
  assert.equal(tool('oc_system_status').annotations.readOnlyHint, true);
  assert.equal(tool('oc_restart').annotations.readOnlyHint, false);
  assert.equal(tool('oc_restart').annotations.destructiveHint, true);
});

test('oc_get_subsystems: фактическая карта bool', async (t) => {
  const state = subsystemState();
  const testCtx = await makeTestContext([signInRoute(), subsystemGetRoute(state)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_subsystems'), testCtx.ctx, {});

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.deepEqual(payload.subsystems, state.map);
  assert.equal(payload.subsystems.telegram_bot, false);
  assert.equal(typeof payload.observed_at, 'string');
  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/subsystem').length, 1);
});

test('oc_set_subsystem: PUT {name:enabled}, таймаут 120 с и перечитывание карты', async (t) => {
  const state = subsystemState();
  let putBody: unknown;
  const testCtx = await makeTestContext([
    signInRoute(),
    subsystemGetRoute(state),
    {
      method: 'PUT',
      path: '/rest/subsystem',
      reply: (call) => {
        putBody = call.body;
        const body = call.body as Record<string, boolean>;
        state.map = { ...state.map, ...body };
        return { body: { success: true, errors: [] } };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_set_subsystem'), testCtx.ctx, {
    name: 'thermal',
    enabled: false,
  });

  assert.equal(result.isError, undefined, 'успешное переключение — не ошибка');
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.success, true);
  assert.deepEqual(payload.errors, []);
  assert.equal(payload.enabled, false);
  assert.deepEqual(putBody, { thermal: false }, 'тело — ровно {name: enabled}');

  const puts = callsFor(testCtx.calls, 'PUT', '/rest/subsystem');
  assert.equal(puts.length, 1);
  assert.equal(puts[0].timeoutMs, SUBSYSTEM_TIMEOUT_MS, 'таймаут именно 120 000 мс');
  assert.equal(SUBSYSTEM_TIMEOUT_MS, 120_000);

  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/subsystem').length, 1, 'карта перечитана');
  assert.equal(payload.subsystems.thermal, false, 'наружу — фактическое состояние');
});

test('oc_set_subsystem: success:false + errors[] → не исключение, карта перечитана', async (t) => {
  const state = subsystemState();
  const testCtx = await makeTestContext([
    signInRoute(),
    subsystemGetRoute(state),
    {
      method: 'PUT',
      path: '/rest/subsystem',
      reply: () => ({
        body: {
          success: false,
          errors: [{ subsystem: 'telegram_bot', message: 'Subsystem not found' }],
        },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_set_subsystem'), testCtx.ctx, {
    name: 'telegram_bot',
    enabled: true,
  });

  assert.equal(result.isError, undefined, 'отчёт устройства не должен быть исключением');
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.success, false);
  assert.deepEqual(payload.errors, [
    { subsystem: 'telegram_bot', message: 'Subsystem not found' },
  ]);
  assert.equal(payload.subsystems.telegram_bot, false, 'карта — фактическая, а не эхо enabled');
  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/subsystem').length, 1);
});

test('oc_set_subsystem: пустое имя и не-bool enabled → validation_failed без сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const emptyName = await runTool(tool('oc_set_subsystem'), testCtx.ctx, {
    name: '   ',
    enabled: true,
  });
  assert.equal(emptyName.isError, true);
  assert.equal(payloadOf(emptyName).error.code, 'validation_failed');

  const wrongEnabled = await runTool(tool('oc_set_subsystem'), testCtx.ctx, {
    name: 'thermal',
    enabled: 'false',
  });
  assert.equal(wrongEnabled.isError, true);
  const payload = payloadOf(wrongEnabled);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.name, 'thermal');

  assert.equal(testCtx.calls.length, 0);
});

test('oc_set_subsystem: GET /rest/subsystem отдаёт SPA → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/subsystem',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SPA</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_subsystems'), testCtx.ctx, {});

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'feature_unsupported');
});

test('oc_system_status: канонические поля прошивки и псевдонимы §13', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/systemStatus',
      reply: () => ({ body: SYSTEM_STATUS_FIXTURE }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_system_status'), testCtx.ctx, {});

  assert.equal(result.isError, undefined);
  const system = payloadOf(result).system;
  assert.equal(system.free_heap, 123456);
  assert.equal(system.heap, 123456, 'псевдоним heap из §13');
  assert.equal(system.min_free_heap, 100000);
  assert.equal(system.total_heap, 201233);
  assert.equal(system.free_psram, 4000000);
  assert.equal(system.psram_size, 8000000);
  assert.equal(system.uptime, 3600);
  assert.equal(system.firmware_version, '0.2.9.2');
  assert.equal(system.firmware, '0.2.9.2', 'псевдоним firmware из §13');
  assert.equal(system.cpu_type, 'ESP32-S3');
  assert.equal(system.chip, 'ESP32-S3', 'псевдоним chip из §13');
  assert.equal(system.esp_platform, 'ESP32-S3');
  assert.equal(system.cpu_freq_mhz, 240);
  assert.equal(system.flash_chip_size, 16777216);
  assert.equal(system.fs_total, 1500000);
  assert.equal(system.fs_used, 200000);
  assert.equal(system.core_temp, 45.5);
  assert.equal(typeof payloadOf(result).observed_at, 'string');
  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/systemStatus').length, 1);
});

test('oc_system_status: старые имена heap/firmware → free_heap/firmware_version', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/systemStatus',
      reply: () => ({ body: { heap: 555, firmware: '0.1.0', uptime: 10 } }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_system_status'), testCtx.ctx, {});

  assert.equal(result.isError, undefined);
  const system = payloadOf(result).system;
  assert.equal(system.free_heap, 555);
  assert.equal(system.heap, 555);
  assert.equal(system.firmware_version, '0.1.0');
  assert.equal(system.firmware, '0.1.0');
  assert.equal(system.chip, null);
  assert.equal(system.free_psram, null);
});

test('oc_restart: 200 → ok:true и hint про 20–40 секунд и /readyz', async (t) => {
  const testCtx = await makeTestContext(
    [
      signInRoute(),
      { method: 'POST', path: '/rest/restart', reply: () => ({ status: 200, body: '' }) },
    ],
    { allowDestructive: true },
  );
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_restart'), testCtx.ctx, {
    confirm: true,
    reason: 'wp9 self-test restart',
  });

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.match(payload.hint, /20–40 секунд/);
  assert.match(payload.hint, /\/readyz/);

  const posts = callsFor(testCtx.calls, 'POST', '/rest/restart');
  assert.equal(posts.length, 1);
  assert.equal(posts[0].body, undefined, 'тело перезагрузке не нужно');
});

test('oc_restart без confirm → confirmation_required без сетевых вызовов', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: true });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_restart'), testCtx.ctx, {
    reason: 'wp9 self-test restart',
  });

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'confirmation_required');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_restart без reason → validation_failed без сетевых вызовов', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: true });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_restart'), testCtx.ctx, { confirm: true });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.required, 'reason');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_restart при allowDestructive:false → disabled_by_policy', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: false });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_restart'), testCtx.ctx, {
    confirm: true,
    reason: 'wp9 self-test restart',
  });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'disabled_by_policy');
  assert.equal(payload.error.details.flag, 'SSVC_ALLOW_DESTRUCTIVE');
  assert.equal(testCtx.calls.length, 0);
});

test('registerSystemTools: четыре инструмента видны в tools/list', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerSystemTools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp9-system-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(
    listed.tools.map((entry) => entry.name).sort(),
    ['oc_get_subsystems', 'oc_restart', 'oc_set_subsystem', 'oc_system_status'],
  );

  const restart = listed.tools.find((entry) => entry.name === 'oc_restart');
  assert.equal(restart?.annotations?.destructiveHint, true);
  const restartSchema = restart?.inputSchema as { properties?: Record<string, unknown> } | undefined;
  assert.ok(restartSchema?.properties?.confirm !== undefined);
  assert.ok(restartSchema?.properties?.reason !== undefined);
});
