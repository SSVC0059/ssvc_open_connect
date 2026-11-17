/**
 * Тесты WP4: датчики, зоны и пороги тревог (mcp-server-agents.md §8).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети. Проверяются ловушки
 * контракта прошивки (mcp-contract-notes.md §B.2–§B.4, §B.7, §D.4):
 *  - `GET /rest/sensor` — карта зон без обёртки, пустой ответ = «датчиков нет»;
 *  - зоны в `/rest/zones` приходят числовыми id, а запись принимает имя;
 *  - `PUT /rest/sensors/zone` может ответить 404 и на идемпотентную запись — решает
 *    перечитанная карта, а наружу уходит фактическая зона, а не эхо аргумента;
 *  - адрес валидируется локально: невалидный не доходит до сети;
 *  - 403 → `forbidden_admin_required`, 400 порогов → `validation_failed` с полями;
 *  - SPA-заглушка (`200 text/html`) — не «пустые датчики», а `feature_unsupported`.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { createMcpServer } from '../src/mcp/server.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { registerSensorsTools, sensorsTools } from '../src/tools/sensors.tools.js';
import {
  makeTestContext,
  signInRoute,
  type FakeCall,
  type FakeRoute,
} from './fakeHttp.js';

/** Адреса — 16 hex-символов, как их печатает прошивка (§B.2). */
const ADDR_A = '28FF64A1B2C3D4E5';
const ADDR_B = '28FF64A1B2C3D4E6';
const ADDR_C = '3C01A816D4E5F601';
const ADDR_BMP = '4700000000000000';

/** Фикстура §F.7: три зоны, давление строкой и уровень без единиц. */
const SENSOR_PAYLOAD = {
  tank: { [ADDR_A]: { v: 78.62, u: '°C', type: 'thermal' } },
  act: { [ADDR_BMP]: { v: '745.2', u: 'мм рт. ст.', type: 'pressure' } },
  receiver_tank_1: { [ADDR_C]: { v: 1, u: '', type: 'level' } },
};

/** Фикстура §F.8: пороги с полным и частичным набором полей. */
const ALARM_PAYLOAD = {
  thresholds: {
    [ADDR_A]: { enabled: true, min: 0, dangerous: 80, critical: 100 },
    [ADDR_B]: { enabled: false, min: 5 },
  },
};

function tool(name: string): ToolDef {
  const found = sensorsTools().find((candidate) => candidate.name === name);
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

test('oc_get_sensors: плоский маппинг зон и нормализация значений', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), getRoute('/rest/sensor', SENSOR_PAYLOAD)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_sensors'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'чтение не должно быть ошибкой');
  const payload = payloadOf(result);

  assert.equal(payload.ok, true);
  assert.deepEqual(
    payload.zones.map((zone: any) => zone.zone),
    ['act', 'receiver_tank_1', 'tank'],
    'ответ — массив зон, отсортированный по имени',
  );

  const act = payload.zones.find((zone: any) => zone.zone === 'act');
  assert.deepEqual(act.readings, [
    { address: ADDR_BMP, value: 745.2, unit: 'мм рт. ст.', type: 'pressure' },
  ], 'числовая строка нормализуется в число без потери точности');

  const tank = payload.zones.find((zone: any) => zone.zone === 'tank');
  assert.deepEqual(tank.readings, [
    { address: ADDR_A, value: 78.62, unit: '°C', type: 'thermal' },
  ]);

  const receiver = payload.zones.find((zone: any) => zone.zone === 'receiver_tank_1');
  assert.equal(receiver.readings[0].unit, '', 'пустые единицы сохраняются как пустая строка');

  assert.equal(typeof payload.observed_at, 'string');
  assert.equal('tank' in payload, false, 'наружу уходит плоский zones[], а не карта прошивки');
});

test('oc_get_sensors: фильтр zone и неизвестная зона дают пустой массив', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), getRoute('/rest/sensor', SENSOR_PAYLOAD)]);
  t.after(() => testCtx.cleanup());

  const filtered = await runTool(tool('oc_get_sensors'), testCtx.ctx, { zone: 'ACT' });
  assert.equal(filtered.isError, undefined);
  const payload = payloadOf(filtered);
  assert.deepEqual(
    payload.zones.map((zone: any) => zone.zone),
    ['act'],
    'фильтр по имени зоны не зависит от регистра',
  );

  const missing = await runTool(tool('oc_get_sensors'), testCtx.ctx, { zone: 'outdoor' });
  assert.equal(missing.isError, undefined);
  assert.deepEqual(payloadOf(missing).zones, [], 'нет датчиков в зоне — пустой массив, не ошибка');
});

test('oc_get_sensors: пустой /rest/sensor — пустой массив зон, а не ошибка', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), getRoute('/rest/sensor', {})]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_sensors'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'пустой ответ — валидное «датчиков нет»');
  assert.deepEqual(payloadOf(result).zones, []);
});

test('403 на чтении → forbidden_admin_required с подсказкой про admin', async (t) => {
  const forbidden = (method: FakeRoute['method'], path: string): FakeRoute => ({
    method,
    path,
    reply: () => ({ status: 403, body: {} }),
  });
  const testCtx = await makeTestContext([
    signInRoute(),
    forbidden('GET', '/rest/sensor'),
    forbidden('GET', '/rest/zones'),
    forbidden('GET', '/rest/alarms'),
  ]);
  t.after(() => testCtx.cleanup());

  for (const name of ['oc_get_sensors', 'oc_get_zones', 'oc_get_alarms']) {
    const result = await runTool(tool(name), testCtx.ctx, {});
    assert.equal(result.isError, true, `${name}: 403 должен быть ошибкой`);
    const payload = payloadOf(result);
    assert.equal(payload.ok, false);
    assert.equal(payload.error.code, 'forbidden_admin_required');
    assert.match(String(payload.error.hint), /admin/);
    assert.equal(payload.error.retryable, false);
  }
});

test('403 на записи → forbidden_admin_required и никаких перечитываний', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'PUT',
      path: '/rest/sensors/zone',
      reply: () => ({ status: 403, body: { status: 'error', message: 'Forbidden' } }),
    },
    {
      method: 'POST',
      path: '/rest/alarms',
      reply: () => ({ status: 403, body: { error: 'Forbidden' } }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const zone = await runTool(tool('oc_set_zone'), testCtx.ctx, {
    address: ADDR_A,
    zone: 'tank',
  });
  assert.equal(zone.isError, true);
  assert.equal(payloadOf(zone).error.code, 'forbidden_admin_required');

  const alarms = await runTool(tool('oc_set_alarm_thresholds'), testCtx.ctx, {
    thresholds: { [ADDR_A]: { min: 0 } },
  });
  assert.equal(alarms.isError, true);
  assert.equal(payloadOf(alarms).error.code, 'forbidden_admin_required');

  assert.equal(callsTo(testCtx.calls, 'GET', '/rest/zones').length, 0);
  assert.equal(callsTo(testCtx.calls, 'GET', '/rest/alarms').length, 0);
});

test('oc_set_zone: невалидный адрес → validation_failed БЕЗ сетевого вызова', async (t) => {
  // Маршрутов нет вовсе: любой запрос к устройству (включая signIn) был бы виден.
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  for (const address of ['28FF64A1B2C3D4E', 'ZZFF64A1B2C3D4E5', '28FF64A1B2C3D4E55']) {
    const result = await runTool(tool('oc_set_zone'), testCtx.ctx, { address, zone: 'tank' });
    assert.equal(result.isError, true, `адрес «${address}» должен быть отклонён`);
    const payload = payloadOf(result);
    assert.equal(payload.error.code, 'validation_failed');
    assert.equal(payload.error.details.field, 'address');
    assert.equal(payload.error.details.reason, 'invalid_address_format');
  }
  assert.equal(testCtx.calls.length, 0, 'валидация адреса локальная: сеть не трогается');
});

test('oc_set_zone: возвращает фактическую зону из перечитанной карты, а не эхо', async (t) => {
  const zones: Record<string, string | number> = { [ADDR_A]: 3 }; // act
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'PUT',
      path: '/rest/sensors/zone',
      reply: () => ({
        // Прошивка отвечает успехом и эхом аргумента, но состояние не меняет.
        body: { status: 'success', zone: 'tank', address: ADDR_A },
      }),
    },
    {
      method: 'GET',
      path: '/rest/zones',
      reply: () => ({ body: { zones } }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_set_zone'), testCtx.ctx, {
    address: ADDR_A.toLowerCase(), // регистр адреса нормализуется к верхнему
    zone: 'TANK',
  });
  assert.equal(result.isError, undefined, 'запись принята — не ошибка инструмента');
  const payload = payloadOf(result);

  assert.equal(payload.address, ADDR_A);
  assert.equal(payload.zone, 'act', 'наружу — фактическая зона, а не аргумент tank');
  assert.equal(payload.applied, false);
  assert.equal(payload.already_set, false);
  assert.equal(typeof payload.warning, 'string');
  assert.match(payload.warning, /перечитанная карта/);

  const puts = callsTo(testCtx.calls, 'PUT', '/rest/sensors/zone');
  assert.equal(puts.length, 1, 'одна привязка — один PUT');
  assert.equal(puts[0].query.address, ADDR_A, 'адрес уходит в query в верхнем регистре');
  assert.equal(puts[0].query.zone, 'tank', 'имя зоны уходит в query в нижнем регистре');
  assert.equal(puts[0].body, undefined, 'тело не отправляется: параметры только в query');
  assert.equal(callsTo(testCtx.calls, 'GET', '/rest/zones').length, 1, 'после записи карта перечитана');
});

test('oc_set_zone: идемпотентная запись (404 прошивки) подтверждается картой зон', async (t) => {
  // SensorConfigService возвращает false, когда зона не изменилась → обработчик отдаёт 404.
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'PUT',
      path: '/rest/sensors/zone',
      reply: () => ({
        status: 404,
        body: {
          status: 'error',
          message: 'Failed to update zone. Sensor not found or invalid parameters',
          address: ADDR_A,
          zone: 'act',
        },
      }),
    },
    {
      method: 'GET',
      path: '/rest/zones',
      reply: () => ({ body: { zones: { [ADDR_A]: 'act' } } }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_set_zone'), testCtx.ctx, { address: ADDR_A, zone: 'act' });
  assert.equal(result.isError, undefined, '404 при совпавшей зоне — это идемпотентный успех');
  const payload = payloadOf(result);
  assert.equal(payload.zone, 'act');
  assert.equal(payload.applied, true);
  assert.equal(payload.already_set, true);
  assert.equal('warning' in payload, false);
});

test('oc_set_zone: 404 и фактическая зона другая → not_found с деталями', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'PUT',
      path: '/rest/sensors/zone',
      reply: () => ({
        status: 404,
        body: { status: 'error', message: 'Failed to update zone. Sensor not found or invalid parameters' },
      }),
    },
    getRoute('/rest/zones', { zones: {} }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_set_zone'), testCtx.ctx, { address: ADDR_A, zone: 'tank' });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'not_found');
  assert.equal(payload.error.details.status, 404);
  assert.equal(payload.error.details.actual_zone, null);
  assert.equal(payload.error.details.requested_zone, 'tank');
});

test('oc_get_zones: числовые id зон переводятся в имена, неизвестный id остаётся числом', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/zones', {
      zones: { [ADDR_A]: 4, [ADDR_B]: 'receiver_tank_1', [ADDR_C]: 42 },
    }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_zones'), testCtx.ctx, {});
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.deepEqual(payload.zones, {
    [ADDR_A]: 'tank',
    [ADDR_B]: 'receiver_tank_1',
    [ADDR_C]: 42,
  });
  assert.equal(typeof payload.observed_at, 'string');
});

test('oc_get_alarms: пороги отдаются как есть, включая частичный набор полей', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), getRoute('/rest/alarms', ALARM_PAYLOAD)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_alarms'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'частичный набор полей — валидный ответ');
  const payload = payloadOf(result);
  assert.deepEqual(payload.thresholds, ALARM_PAYLOAD.thresholds);
  assert.equal(typeof payload.observed_at, 'string');
});

test('oc_set_alarm_thresholds: частичный набор полей, тело запроса и перечитывание', async (t) => {
  const current: Record<string, Record<string, unknown>> = {
    [ADDR_A]: { enabled: true, min: 0, dangerous: 80, critical: 100 },
  };
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'POST',
      path: '/rest/alarms',
      reply: (call) => {
        const body = call.body as { thresholds: Record<string, Record<string, unknown>> };
        for (const [address, entry] of Object.entries(body.thresholds)) {
          // Как прошивка: недостающие поля берутся из значений по умолчанию/текущего состояния.
          current[address] = { ...(current[address] ?? { enabled: true, min: 0, dangerous: 80, critical: 100 }), ...entry };
        }
        return { body: { thresholds: current } };
      },
    },
    { method: 'GET', path: '/rest/alarms', reply: () => ({ body: { thresholds: current } }) },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_set_alarm_thresholds'), testCtx.ctx, {
    thresholds: { [ADDR_A]: { min: 10 } },
  });
  assert.equal(result.isError, undefined, 'частичные поля не должны ронять запись');
  const payload = payloadOf(result);

  assert.deepEqual(payload.thresholds[ADDR_A], {
    enabled: true,
    min: 10,
    dangerous: 80,
    critical: 100,
  }, 'наружу — перечитанные пороги целиком, включая недостающие в запросе поля');
  assert.equal(payload.applied, true);
  assert.deepEqual(payload.requested_addresses, [ADDR_A]);
  assert.deepEqual(payload.not_applied, []);
  assert.deepEqual(payload.mismatched, []);

  const posts = callsTo(testCtx.calls, 'POST', '/rest/alarms');
  assert.equal(posts.length, 1);
  assert.deepEqual(posts[0].body, { thresholds: { [ADDR_A]: { min: 10 } } }, 'тело ровно {thresholds}');
  assert.equal(callsTo(testCtx.calls, 'GET', '/rest/alarms').length, 1, 'пороги перечитываются');
});

test('oc_set_alarm_thresholds: 400 → validation_failed с полями и без перечитывания', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'POST',
      path: '/rest/alarms',
      reply: () => ({
        status: 400,
        body: { min: 'Invalid value', dangerous: 'Invalid value' },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_set_alarm_thresholds'), testCtx.ctx, {
    thresholds: { [ADDR_A]: { min: 'oops', dangerous: 999 } },
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.status, 400);
  assert.deepEqual(payload.error.details.fields, {
    min: 'Invalid value',
    dangerous: 'Invalid value',
  });
  assert.deepEqual(payload.error.details.invalid_fields, ['min', 'dangerous']);
  assert.match(payload.error.message, /min/);
  assert.equal(callsTo(testCtx.calls, 'GET', '/rest/alarms').length, 0);
});

test('SPA-заглушка 200 text/html → feature_unsupported, а не пустые датчики', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/sensor',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SSVC OpenConnect</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_sensors'), testCtx.ctx, {});
  assert.equal(result.isError, true, 'SPA-заглушка не должна выглядеть успехом');
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.match(payload.error.message, /SPA|JSON/);
  assert.equal('zones' in payload, false);
});

test('registerSensorsTools: все пять инструментов видны в tools/list с нужными аннотациями', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerSensorsTools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp4-sensors-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(
    listed.tools.map((entry) => entry.name).sort(),
    ['oc_get_alarms', 'oc_get_sensors', 'oc_get_zones', 'oc_set_alarm_thresholds', 'oc_set_zone'],
  );

  for (const name of ['oc_get_sensors', 'oc_get_zones', 'oc_get_alarms']) {
    const read = listed.tools.find((entry) => entry.name === name);
    assert.equal(read?.annotations?.readOnlyHint, true, `${name}: чтение`);
    assert.equal(read?.annotations?.destructiveHint, false);
  }

  const setZoneTool = listed.tools.find((entry) => entry.name === 'oc_set_zone');
  assert.equal(setZoneTool?.annotations?.readOnlyHint, false);
  assert.equal(setZoneTool?.annotations?.destructiveHint, false, 'риск 1 — обратимая запись');
  assert.equal(setZoneTool?.title, 'Привязать датчик к зоне');
  const zoneProps = (setZoneTool?.inputSchema as { properties?: Record<string, unknown> } | undefined)
    ?.properties;
  assert.ok(zoneProps?.address !== undefined, 'address обязателен в схеме');
  assert.ok(zoneProps?.zone !== undefined, 'zone обязателен в схеме');

  const setAlarms = listed.tools.find((entry) => entry.name === 'oc_set_alarm_thresholds');
  assert.equal(setAlarms?.annotations?.readOnlyHint, false);
  assert.equal(setAlarms?.annotations?.destructiveHint, false);
  assert.equal(setAlarms?.title, 'Изменить пороги тревог');
  const alarmProps = (setAlarms?.inputSchema as { properties?: Record<string, unknown> } | undefined)
    ?.properties;
  assert.ok(alarmProps?.thresholds !== undefined, 'thresholds обязателен в схеме');
});

test('oc_set_alarm_thresholds: тело не объект → validation_failed без записи', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  // runTool вызывается напрямую, минуя zod-схему SDK, — домен обязан проверить сам.
  const result = await runTool(tool('oc_set_alarm_thresholds'), testCtx.ctx, {
    thresholds: { [ADDR_A]: 'not-an-object' } as unknown as Record<string, unknown>,
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.reason, 'entry_not_object');
  assert.equal(callsTo(testCtx.calls, 'POST', '/rest/alarms').length, 0);
});
