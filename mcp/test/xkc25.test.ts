/**
 * Тесты WP6: датчики уровня XKC-25 (mcp-server-agents.md §10).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети и без живого устройства:
 * калибровка на стенде перезаписывает сырые уровни датчика, поэтому её путь
 * проверяется только здесь (mcp-contract-notes.md §B.12, §C.7, §D.5, §F.9).
 *
 * Проверяются ловушки контракта:
 *  - статус отдаёт слоты, режим предохранителя и причины блокировки;
 *  - шаги `without_liquid`/`with_liquid` — ровно один POST, без повторов и без timeoutMs;
 *  - `verify_return` — 202, затем опрос; результат на третьем опросе и таймаут с `pending`;
 *  - 404 `sensor_not_available`/`sensor_not_configured` → `not_found`;
 *  - 409 `unstable_signal` → `validation_failed` с кодом устройства в details;
 *  - неверный index/step/timeout отсекаются до сети;
 *  - SPA-заглушка (`200 text/html`) → `feature_unsupported`.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { createMcpServer } from '../src/mcp/server.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { registerXkc25Tools, xkc25Tools } from '../src/tools/xkc25.tools.js';
import {
  makeTestContext,
  signInRoute,
  type FakeCall,
  type FakeRoute,
} from './fakeHttp.js';

/** Один датчик на стенде: GPIO 4, зона receiver_tank_1, без калибровки (§F.9). */
const SENSOR = {
  index: 0,
  enabled: true,
  pinSource: 0,
  gpio: 4,
  pcfAddress: 0,
  pcfBit: 0,
  pinLabel: 'GPIO 4',
  zoneIndex: 7,
  zone: 'receiver_tank_1',
  zoneLabel: 'Приёмная ёмкость 1',
  sharedAcrossTanks: false,
  activeLow: true,
  calibrated: false,
  calibratedAt: 0,
  levelWithoutLiquid: 1,
  levelWithLiquid: 0,
  live: true,
  rawLevel: 1,
  filled: false,
};

const STATUS_PAYLOAD = {
  sensors: [SENSOR],
  triggerAction: 1,
  sharedTankIndex: 0,
  startGuardMode: 2,
  blockStart: false,
  blockReasons: [],
  warnings: ['Датчик уровня #1 не откалиброван — работа датчика не проверена'],
};

const BLOCKED_STATUS_PAYLOAD = {
  ...STATUS_PAYLOAD,
  blockStart: true,
  blockReasons: [
    'Датчик уровня #1 (GPIO 4, Приёмная ёмкость 1) не откалиброван — старт запрещён',
  ],
};

const CALIBRATED_SENSOR = {
  ...SENSOR,
  calibrated: true,
  calibratedAt: 1763300000,
  levelWithoutLiquid: 1,
  levelWithLiquid: 0,
};

function tool(name: string): ToolDef {
  const found = xkc25Tools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

function callsTo(calls: FakeCall[], method: string, path: string): FakeCall[] {
  return calls.filter((call) => call.method === method && call.path === path);
}

function getRoute(path: string, body: unknown): FakeRoute {
  return { method: 'GET', path, reply: () => ({ body }) };
}

function calibrateRoute(reply: FakeRoute['reply']): FakeRoute {
  return { method: 'POST', path: '/rest/oc/xkc25/calibrate', reply };
}

function calibrationBody(step: string): Record<string, unknown> {
  return {
    ok: true,
    completed: step === 'with_liquid',
    code: 'ok',
    rawLevel: 1,
    message:
      step === 'without_liquid'
        ? 'Уровень без жидкости записан'
        : 'Уровень с жидкостью записан, калибровка завершена',
    checklist: [],
    sensor: step === 'with_liquid' ? CALIBRATED_SENSOR : SENSOR,
  };
}

test('oc_get_xkc25_status: датчики, режим предохранителя и blockReasons при блокировке старта', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/xkc25/status', BLOCKED_STATUS_PAYLOAD),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_xkc25_status'), testCtx.ctx, {});
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);

  assert.equal(payload.ok, true);
  assert.equal(payload.sensors.length, 1);
  assert.equal(payload.sensors[0].index, 0);
  assert.equal(payload.sensors[0].zone, 'receiver_tank_1');
  assert.equal(payload.sensors[0].gpio, 4);
  assert.equal(payload.sensors[0].calibrated, false);
  assert.equal(payload.trigger_action, 1);
  assert.equal(payload.start_guard_mode, 2);
  assert.equal(payload.block_start, true);
  assert.deepEqual(payload.block_reasons, BLOCKED_STATUS_PAYLOAD.blockReasons);
  assert.deepEqual(payload.warnings, BLOCKED_STATUS_PAYLOAD.warnings);
  assert.equal(typeof payload.observed_at, 'string');

  assert.equal(callsTo(testCtx.calls, 'GET', '/rest/oc/xkc25/status').length, 1);
});

test('oc_get_xkc25_status: без блокировки — block_start:false и пустые blockReasons', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/xkc25/status', STATUS_PAYLOAD),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_xkc25_status'), testCtx.ctx, {});
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);

  assert.equal(payload.block_start, false);
  assert.deepEqual(payload.block_reasons, []);
  assert.equal(payload.sensors[0].zone, 'receiver_tank_1');
  assert.deepEqual(payload.warnings, STATUS_PAYLOAD.warnings);
});

test('oc_xkc25_calibrate: without_liquid и with_liquid — 200, ровно один POST на шаг', async (t) => {
  for (const step of ['without_liquid', 'with_liquid'] as const) {
    const testCtx = await makeTestContext([
      signInRoute(),
      calibrateRoute((call) => {
        const body = call.body as { step?: string };
        return { body: calibrationBody(body.step ?? step) };
      }),
    ]);
    t.after(() => testCtx.cleanup());

    const result = await runTool(tool('oc_xkc25_calibrate'), testCtx.ctx, {
      index: 0,
      step,
    });
    assert.equal(result.isError, undefined, `шаг ${step} должен вернуть ok`);
    const payload = payloadOf(result);

    assert.equal(payload.ok, true);
    assert.equal(payload.device_ok, true);
    assert.equal(payload.completed, step === 'with_liquid');
    assert.equal(payload.code, 'ok');
    assert.equal(typeof payload.message, 'string');
    assert.deepEqual(payload.checklist, []);
    assert.equal(payload.sensor.index, 0);
    assert.equal(
      payload.sensor.calibrated,
      step === 'with_liquid',
      'калибровка завершается только на шаге с жидкостью',
    );

    const posts = callsTo(testCtx.calls, 'POST', '/rest/oc/xkc25/calibrate');
    assert.equal(posts.length, 1, `шаг ${step}: POST должен быть ровно один`);
    assert.deepEqual(
      posts[0].body,
      { index: 0, step },
      'для шага без verify_return тело — только index и step, без timeoutMs',
    );
  }
});

test('oc_xkc25_calibrate: verify_return 202 → done с третьей попытки опроса', async (t) => {
  let polls = 0;
  const testCtx = await makeTestContext([
    signInRoute(),
    calibrateRoute(() => ({
      status: 202,
      body: {
        status: 'pending',
        index: 0,
        code: 'ok',
        message: 'Проверка возврата запущена',
      },
    })),
    {
      method: 'GET',
      path: '/rest/oc/xkc25/verify-status',
      reply: () => {
        polls += 1;
        if (polls < 3) return { body: { status: 'pending', index: 0 } };
        return {
          body: {
            status: 'done',
            index: 0,
            ok: true,
            completed: true,
            code: 'ok',
            rawLevel: 0,
            message: 'Проверка возврата пройдена',
          },
        };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_xkc25_calibrate'), testCtx.ctx, {
    index: 0,
    step: 'verify_return',
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);

  assert.equal(payload.ok, true);
  assert.equal(payload.device_ok, true);
  assert.equal(payload.completed, true);
  assert.equal(payload.code, 'ok');
  assert.equal(payload.message, 'Проверка возврата пройдена');
  assert.equal(payload.verify.status, 'done');
  assert.equal(payload.verify.ok, true);
  assert.equal(payload.verify.completed, true);
  assert.equal(payload.verify.raw_level, 0);
  assert.equal('hint' in payload, false, 'результат получен — подсказка не нужна');

  assert.equal(polls, 3, 'опрос должен прекратиться на done');
  const posts = callsTo(testCtx.calls, 'POST', '/rest/oc/xkc25/calibrate');
  assert.equal(posts.length, 1, 'POST verify_return не повторяется');
  assert.deepEqual(posts[0].body, {
    index: 0,
    step: 'verify_return',
    timeoutMs: 5_000,
  });
});

test('oc_xkc25_calibrate: verify_return таймаут → ok:true, pending и hint, POST один', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    calibrateRoute(() => ({
      status: 202,
      body: {
        status: 'pending',
        index: 0,
        code: 'ok',
        message: 'Проверка возврата запущена',
      },
    })),
    getRoute('/rest/oc/xkc25/verify-status', { status: 'pending', index: 0 }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_xkc25_calibrate'), testCtx.ctx, {
    index: 0,
    step: 'verify_return',
    timeout_ms: 1_000,
  });
  assert.equal(result.isError, undefined, 'таймаут опроса — не ошибка');
  const payload = payloadOf(result);

  assert.equal(payload.ok, true);
  assert.equal(payload.completed, false);
  assert.equal(payload.verify.status, 'pending');
  assert.equal(
    payload.hint,
    'проверка продолжается, повторите oc_xkc25_verify_status',
  );

  const posts = callsTo(testCtx.calls, 'POST', '/rest/oc/xkc25/calibrate');
  assert.equal(posts.length, 1, 'POST verify_return не повторяется и при таймауте');
  assert.ok(
    callsTo(testCtx.calls, 'GET', '/rest/oc/xkc25/verify-status').length >= 2,
    'до таймаута должен быть хотя бы один повторный опрос',
  );
});

test('oc_xkc25_calibrate: 404 sensor_not_available → not_found с русской подсказкой', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    calibrateRoute(() => ({
      status: 404,
      body: {
        error: 'XKC-25 subsystem is not active',
        code: 'sensor_not_available',
      },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_xkc25_calibrate'), testCtx.ctx, {
    index: 0,
    step: 'without_liquid',
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'not_found');
  assert.equal(payload.error.details.device_code, 'sensor_not_available');
  assert.match(String(payload.error.message), /недоступен/);
  assert.match(String(payload.error.hint), /xkc25_sensor|подсистема/i);
  assert.equal(callsTo(testCtx.calls, 'POST', '/rest/oc/xkc25/calibrate').length, 1);
});

test('oc_xkc25_calibrate: 404 sensor_not_configured → not_found', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    calibrateRoute(() => ({
      status: 404,
      body: { error: 'Sensor not configured', code: 'sensor_not_configured' },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_xkc25_calibrate'), testCtx.ctx, {
    index: 2,
    step: 'with_liquid',
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'not_found');
  assert.equal(payload.error.details.device_code, 'sensor_not_configured');
  assert.match(String(payload.error.message), /не настроен/);
});

test('oc_xkc25_calibrate: 409 unstable_signal → validation_failed с кодом устройства', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    calibrateRoute(() => ({
      status: 409,
      body: {
        ok: false,
        completed: false,
        code: 'unstable_signal',
        rawLevel: 1,
        message: 'Уровень менялся во время замера',
        checklist: ['Проверьте крепление датчика', 'Убедитесь, что жидкость не плещется'],
        sensor: SENSOR,
      },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_xkc25_calibrate'), testCtx.ctx, {
    index: 0,
    step: 'without_liquid',
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);

  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.device_code, 'unstable_signal');
  assert.deepEqual(payload.error.details.checklist, [
    'Проверьте крепление датчика',
    'Убедитесь, что жидкость не плещется',
  ]);
  assert.match(String(payload.error.message), /нестабилен/);
  assert.equal(payload.error.details.status, 409);
});

test('oc_xkc25_calibrate: неверный index или step → validation_failed без сетевых вызовов', async (t) => {
  for (const args of [
    { index: 7, step: 'without_liquid' },
    { index: 0, step: 'bogus_step' },
    { index: 0, step: 'verify_return', timeout_ms: 500 },
  ]) {
    // Маршрутов нет вовсе: любой сетевой вызов дал бы 404, а не validation_failed.
    const testCtx = await makeTestContext([]);
    t.after(() => testCtx.cleanup());

    const result = await runTool(tool('oc_xkc25_calibrate'), testCtx.ctx, args);
    assert.equal(result.isError, true, `args=${JSON.stringify(args)}`);
    assert.equal(payloadOf(result).error.code, 'validation_failed');
    assert.equal(testCtx.calls.length, 0, 'до сети дело доходить не должно');
  }
});

test('SPA-заглушка 200 text/html на xkc25/status → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/oc/xkc25/status',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SSVC OpenConnect</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_xkc25_status'), testCtx.ctx, {});
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.match(String(payload.error.message), /SPA|JSON/);
  assert.equal('sensors' in payload, false);
});

test('oc_xkc25_verify_status: done и none отдаются как данные', async (t) => {
  let reply: Record<string, unknown> = {
    status: 'done',
    index: 0,
    ok: true,
    completed: true,
    code: 'ok',
    rawLevel: 0,
    message: 'Проверка возврата пройдена',
  };
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/oc/xkc25/verify-status',
      reply: () => ({ body: reply }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const done = await runTool(tool('oc_xkc25_verify_status'), testCtx.ctx, {});
  assert.equal(done.isError, undefined);
  const donePayload = payloadOf(done);
  assert.equal(donePayload.ok, true);
  assert.equal(donePayload.status, 'done');
  assert.equal(donePayload.device_ok, true);
  assert.equal(donePayload.completed, true);
  assert.equal(donePayload.code, 'ok');
  assert.equal(donePayload.message, 'Проверка возврата пройдена');
  assert.equal(donePayload.raw_level, 0);

  reply = { status: 'none' };
  const none = await runTool(tool('oc_xkc25_verify_status'), testCtx.ctx, {});
  assert.equal(none.isError, undefined);
  const nonePayload = payloadOf(none);
  assert.equal(nonePayload.status, 'none');
  assert.equal(nonePayload.device_ok, null);
  assert.equal(nonePayload.completed, null);
});

test('registerXkc25Tools: три инструмента видны в tools/list с аннотациями риска', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerXkc25Tools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp6-xkc25-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(
    listed.tools.map((entry) => entry.name).sort(),
    ['oc_get_xkc25_status', 'oc_xkc25_calibrate', 'oc_xkc25_verify_status'],
  );

  for (const name of ['oc_get_xkc25_status', 'oc_xkc25_verify_status']) {
    const read = listed.tools.find((entry) => entry.name === name);
    assert.equal(read?.annotations?.readOnlyHint, true, `${name}: чтение`);
    assert.equal(read?.annotations?.destructiveHint, false);
  }

  const calibrate = listed.tools.find((entry) => entry.name === 'oc_xkc25_calibrate');
  assert.equal(calibrate?.annotations?.readOnlyHint, false);
  assert.equal(calibrate?.annotations?.destructiveHint, false, 'риск 1 — обратимая запись');
  assert.equal(calibrate?.title, 'Откалибровать датчик уровня');

  const schema = calibrate?.inputSchema as
    | { properties?: Record<string, any>; required?: string[] }
    | undefined;
  assert.ok(schema?.properties?.index !== undefined);
  assert.ok(schema?.properties?.step !== undefined);
  assert.deepEqual(schema?.required, ['index', 'step']);
  assert.deepEqual(schema?.properties?.step?.enum, [
    'without_liquid',
    'with_liquid',
    'verify_return',
  ]);
});
