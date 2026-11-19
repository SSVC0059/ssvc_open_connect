/**
 * Тесты WP1: состояние, телеметрия и статус процесса (mcp-server-agents.md §5).
 *
 * Проверяем:
 *  - `oc_get_state` читает `/rest/oc/snapshot` ОДНИМ запросом и доносит `partial[]`;
 *  - `stale` считается по неизменности `lastUpdate` (монотонное время, инъекция часов);
 *  - отсутствие `telemetry` — `device_error`, а SPA-заглушка вместо snapshot —
 *    `feature_unsupported` (старая прошивка), а не падение;
 *  - `oc_get_telemetry` читает `/rest/telemetry` (и `/rest/sensor` по include_sensors),
 *    а не snapshot;
 *  - числа-строки нормализуются без потери точности.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import {
  bindStalenessTracker,
  normalizeScalar,
  StalenessTracker,
} from '../src/domain/telemetry.js';
import { TELEMETRY_STALE_MS } from '../src/device/schemas/telemetry.js';
import { registerTelemetryResource, TELEMETRY_RESOURCE_URI } from '../src/resources/telemetry.resource.js';
import { createMcpServer } from '../src/mcp/server.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { telemetryTools } from '../src/tools/telemetry.tools.js';
import { makeTestContext, signInRoute, type FakeRoute } from './fakeHttp.js';

const INFO = {
  ssvc: { version: '2.3.30', api: '1.10', api_code: 110, mode: 'late_heads' },
  oc: {
    version: '0.2.9.2',
    is_support_api: true,
    is_support_release: true,
    api_min: '1.7',
    api_target: '1.10',
    api_compatibility: 'supported',
    api_features: [{ name: 'base', min_api: '1.7', available: true }],
  },
};

/** Фикстура, снятая с живого контроллера (значения сокращены). */
const SNAPSHOT = {
  observed_at: 827618,
  info: INFO,
  telemetry: {
    type: 'waiting',
    stop: false,
    stops: 0,
    // Прошивка отдаёт число строкой — домен обязан нормализовать.
    common: { mmhg: '765.7', tp1: 27.19, relay: false },
    hysteresis: 0,
    volume: { heads: 0, hearts: 0, late_heads: 0 },
  },
  status: {
    stage: 'waiting',
    status: 'idle',
    startTime: '',
    endTime: '',
    stages: { waiting: 'running', error: 'finished' },
    uartConnectionError: false,
  },
  lastUpdate: 825908,
  sensor: {
    tank: { '4700000000000000': { v: 766.6454, u: 'мм рт. ст.', type: 'pressure' } },
    receiver_tank_1: { '0400000000000000': { v: '1', u: '', type: 'level' } },
  },
  faults: { faults: [], count: 0, i2cBusActive: true, i2cScan: [71] },
  partial: [] as string[],
};

const TELEMETRY = {
  telemetry: SNAPSHOT.telemetry,
  status: SNAPSHOT.status,
  lastUpdate: 835908,
};

function snapshotRoute(reply: () => unknown): FakeRoute {
  return { method: 'GET', path: '/rest/oc/snapshot', reply: () => ({ body: reply() }) };
}

function telemetryRoute(reply: () => unknown): FakeRoute {
  return { method: 'GET', path: '/rest/telemetry', reply: () => ({ body: reply() }) };
}

function sensorRoute(reply: () => unknown): FakeRoute {
  return { method: 'GET', path: '/rest/sensor', reply: () => ({ body: reply() }) };
}

function tool(name: string): ToolDef {
  const found = telemetryTools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

test('oc_get_state: один запрос snapshot, данные, partial и нормализация чисел', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), snapshotRoute(() => SNAPSHOT)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_state'), testCtx.ctx, {});
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);

  assert.equal(payload.observed_at, 827618, 'observed_at — millis() устройства');
  assert.equal(payload.last_update_ms, 825908);
  assert.equal(payload.stale, false, 'первое наблюдение не может быть устаревшим');
  assert.equal(payload.status.stage, 'waiting');
  assert.equal(payload.status.status, 'idle');
  assert.equal(payload.status.uart_error, false);
  assert.deepEqual(payload.status.stages, { waiting: 'running', error: 'finished' });
  assert.equal(payload.info.device.firmware, '0.2.9.2');
  assert.equal(payload.telemetry.common.mmhg, 765.7, 'строка «765.7» → число');
  assert.equal(payload.telemetry.type, 'waiting', 'нечисловые строки не трогаем');
  assert.equal(payload.telemetry.volume.heads, 0);
  assert.equal(payload.sensor.tank['4700000000000000'].v, 766.6454);
  assert.equal(payload.sensor.receiver_tank_1['0400000000000000'].v, 1, 'строка «1» → число');
  assert.equal(payload.sensor.tank['4700000000000000'].u, 'мм рт. ст.');
  assert.equal(payload.faults.count, 0);
  assert.deepEqual(payload.faults.i2c_scan, [71]);
  assert.deepEqual(payload.partial, []);
  assert.equal(
    testCtx.calls.filter((call) => call.path === '/rest/oc/snapshot').length,
    1,
    'oc_get_state — ровно один запрос снимка',
  );
});

test('oc_get_state: stale после 10 с без изменения lastUpdate, свежий после обновления', async (t) => {
  let clock = 1_000_000;
  let lastUpdate = 5_000;
  const testCtx = await makeTestContext([
    signInRoute(),
    snapshotRoute(() => ({ ...SNAPSHOT, lastUpdate })),
  ]);
  t.after(() => testCtx.cleanup());

  // Инъекция «текущего времени»: проверяем именно логику неизменности значения.
  bindStalenessTracker(
    testCtx.ctx.client,
    new StalenessTracker({ now: () => clock, staleMs: TELEMETRY_STALE_MS }),
  );

  const first = payloadOf(await runTool(tool('oc_get_state'), testCtx.ctx, {}));
  assert.equal(first.stale, false);
  assert.equal(first.freshness_age_ms, 0);

  clock += 11_000;
  const second = payloadOf(await runTool(tool('oc_get_state'), testCtx.ctx, {}));
  assert.equal(second.stale, true, 'lastUpdate не менялся 11 с — телеметрия встала');
  assert.equal(second.freshness_age_ms, 11_000);
  assert.equal(second.last_update_ms, 5_000);

  // Устройство обновило телеметрию — снова свежо.
  lastUpdate = 7_123;
  clock += 1;
  const third = payloadOf(await runTool(tool('oc_get_state'), testCtx.ctx, {}));
  assert.equal(third.stale, false);
  assert.equal(third.freshness_age_ms, 0);
  assert.equal(third.last_update_ms, 7_123);
});

test('StalenessTracker: порог строго больше 10 с, новое значение сбрасывает возраст', () => {
  let now = 100_000;
  const tracker = new StalenessTracker({ now: () => now, staleMs: 10_000 });

  assert.deepEqual(tracker.observe(100), { stale: false, age_ms: 0, last_update_ms: 100 });
  now += 10_000;
  assert.equal(tracker.observe(100).stale, false, 'ровно 10 с — ещё не «дольше 10 с»');
  now += 1;
  assert.equal(tracker.observe(100).stale, true);
  assert.equal(tracker.observe(101).stale, false, 'значение изменилось — данные свежие');
  assert.deepEqual(tracker.observe(undefined), {
    stale: null,
    age_ms: null,
    last_update_ms: null,
  });
});

test('oc_get_state: partial[] доносится, отсутствующие компоненты попадают в него', async (t) => {
  const snapshot = {
    ...SNAPSHOT,
    sensor: undefined,
    faults: undefined,
    partial: ['sensor'],
  };
  const testCtx = await makeTestContext([signInRoute(), snapshotRoute(() => snapshot)]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_state'), testCtx.ctx, {}));
  assert.equal(payload.sensor, undefined, 'нет данных — поля нет, а не нули');
  assert.equal(payload.faults, undefined);
  assert.deepEqual(payload.partial.sort(), ['faults', 'sensor']);
});

test('oc_get_state: include ограничивает компоненты, статус остаётся', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), snapshotRoute(() => SNAPSHOT)]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(tool('oc_get_state'), testCtx.ctx, { include: ['telemetry'] }),
  );
  assert.equal(payload.telemetry.type, 'waiting');
  assert.equal(payload.status.stage, 'waiting', 'статус — всегда часть состояния');
  assert.equal(payload.info, undefined);
  assert.equal(payload.sensor, undefined);
  assert.equal(payload.faults, undefined);
});

test('oc_get_state: отсутствие telemetry в снимке → device_error (§2.1)', async (t) => {
  const snapshot = { ...SNAPSHOT, telemetry: undefined };
  const testCtx = await makeTestContext([signInRoute(), snapshotRoute(() => snapshot)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_state'), testCtx.ctx, {});
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.ok, false);
  assert.equal(payload.error.code, 'device_error');
  assert.equal(payload.error.retryable, false);
  assert.equal(typeof payload.error.message, 'string');
  assert.match(payload.error.message, /телеметри/i);
});

test('oc_get_state: include без telemetry не требует этот блок в снимке', async (t) => {
  const snapshot = { ...SNAPSHOT, telemetry: undefined };
  const testCtx = await makeTestContext([signInRoute(), snapshotRoute(() => snapshot)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_state'), testCtx.ctx, { include: ['info', 'faults'] });
  assert.equal(result.isError, undefined, 'info/faults доступны и без блока telemetry');
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.telemetry, undefined);
  assert.ok(payload.info !== undefined || payload.partial.includes('info'));
});

test('oc_get_state: старая прошивка (SPA 200 text/html) → feature_unsupported с подсказкой', async (t) => {
  const spa: FakeRoute = {
    method: 'GET',
    path: '/rest/oc/snapshot',
    reply: () => ({
      status: 200,
      body: '<!DOCTYPE html><html><body>SPA</body></html>',
      headers: { 'content-type': 'text/html; charset=utf-8' },
    }),
  };
  const testCtx = await makeTestContext([signInRoute(), spa]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_state'), testCtx.ctx, {});
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.equal(payload.error.retryable, false);
  assert.match(String(payload.error.hint), /\/rest\/oc\/snapshot/);
  assert.match(String(payload.error.hint), /oc_get_telemetry/);
});

test('oc_get_telemetry: читает /rest/telemetry, snapshot не трогает', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    telemetryRoute(() => TELEMETRY),
    snapshotRoute(() => SNAPSHOT),
  ]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_telemetry'), testCtx.ctx, {}));
  assert.equal(payload.last_update_ms, 835908);
  assert.equal(payload.observed_at, null, 'у /rest/telemetry нет observed_at');
  assert.equal(payload.telemetry.common.mmhg, 765.7);
  assert.equal(payload.status.stage, 'waiting');
  assert.equal(payload.stale, false);
  assert.equal('sensor' in payload, false, 'include_sensors:false — датчиков нет');
  assert.equal(
    testCtx.calls.filter((call) => call.path === '/rest/oc/snapshot').length,
    0,
    'oc_get_telemetry не должен читать snapshot',
  );
  assert.equal(
    testCtx.calls.filter((call) => call.path === '/rest/sensor').length,
    0,
  );
});

test('oc_get_telemetry: include_sensors:true добавляет датчики', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    telemetryRoute(() => TELEMETRY),
    sensorRoute(() => SNAPSHOT.sensor),
  ]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(tool('oc_get_telemetry'), testCtx.ctx, { include_sensors: true }),
  );
  assert.equal(payload.sensor.tank['4700000000000000'].v, 766.6454);
  assert.equal(payload.sensor.receiver_tank_1['0400000000000000'].v, 1);
  assert.equal(
    testCtx.calls.filter((call) => call.path === '/rest/sensor').length,
    1,
  );
});

test('oc_get_telemetry: /rest/telemetry без telemetry → device_error', async (t) => {
  const broken = { status: SNAPSHOT.status, lastUpdate: 1000 };
  const testCtx = await makeTestContext([signInRoute(), telemetryRoute(() => broken)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_telemetry'), testCtx.ctx, {});
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'device_error');
});

test('oc_get_process_status: только stage/status/uart_error/stages и никакого sensor', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    telemetryRoute(() => TELEMETRY),
    sensorRoute(() => SNAPSHOT.sensor),
  ]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_process_status'), testCtx.ctx, {}));
  assert.deepEqual(payload, {
    ok: true,
    stage: 'waiting',
    status: 'idle',
    uart_error: false,
    started_at: '',
    ended_at: '',
    stages: { waiting: 'running', error: 'finished' },
  });
  assert.equal(
    testCtx.calls.filter((call) => call.path === '/rest/sensor').length,
    0,
  );
  assert.equal(
    testCtx.calls.filter((call) => call.path === '/rest/oc/snapshot').length,
    0,
  );
});

test('нормализация чисел/строк не теряет точность', () => {
  assert.equal(normalizeScalar('766.6454'), 766.6454);
  assert.equal(normalizeScalar(' 12.5 '), 12.5);
  assert.equal(normalizeScalar('1'), 1);
  assert.equal(normalizeScalar(''), '');
  assert.equal(normalizeScalar('waiting'), 'waiting');
  assert.equal(normalizeScalar('12,5'), '12,5', 'запятая — не число для прошивки');
  const longInteger = '12345678901234567890';
  assert.equal(normalizeScalar(longInteger), longInteger, '>15 цифр: Number потерял бы точность');
  assert.equal(normalizeScalar(27.19), 27.19);
  assert.equal(normalizeScalar(true), true);
});

test('ресурс openconnect://telemetry отдаёт снимок со stale и partial', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), snapshotRoute(() => SNAPSHOT)]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerTelemetryResource(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp1-telemetry-resource-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const read = await client.readResource({ uri: TELEMETRY_RESOURCE_URI });
  assert.equal(read.contents.length, 1);
  const text = (read.contents[0] as { text?: string }).text;
  assert.ok(typeof text === 'string');
  const payload = JSON.parse(text) as Record<string, any>;
  assert.equal(payload.observed_at, 827618);
  assert.equal(payload.stale, false);
  assert.deepEqual(payload.partial, []);
  assert.equal(payload.status.stage, 'waiting');
});
