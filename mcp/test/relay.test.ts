/**
 * Тесты WP5: реле на PCF8574 (mcp-server-agents.md §9).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети. Проверяются ловушки
 * контракта прошивки (mcp-contract-notes.md §B.13, §C.6, §D.5):
 *  - `supported:false` (GPIO-сборка) — это `feature_unsupported`, а не сбой устройства;
 *  - metadata (index 1-based, targetBit 0-based) и state (активный низкий уровень);
 *  - override передаёт `enable` и `energized` ЯВНО и в правильных типах, тело без обёртки;
 *  - `403 Relay is reserved for AlarmManager` → `validation_failed`, а не `forbidden_admin_required`;
 *  - `400 bit out of range` → `validation_failed`;
 *  - `bit >= relay_line_count` отсекается локально (POST не отправляется);
 *  - без `confirm` риск-2 инструмент не делает ни одного сетевого вызова;
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
import { registerRelayTools, relayTools } from '../src/tools/relay.tools.js';
import {
  makeTestContext,
  signInRoute,
  type FakeCall,
  type FakeRoute,
} from './fakeHttp.js';

/** Фикстура §F.10: один чип 0x24, 8 линий, биты 0 и 1 заняты AlarmManager. */
const METADATA_PAYLOAD = {
  supported: true,
  relayChipCount: 1,
  relayLineCount: 8,
  relays: [
    {
      index: 1,
      label: 'Реле 1',
      editable: false,
      reason: 'Системное реле AlarmManager (только просмотр)',
      targetBit: 0,
    },
    {
      index: 2,
      label: 'Реле 2',
      editable: false,
      reason: 'Системное реле AlarmManager (только просмотр)',
      targetBit: 1,
    },
    { index: 3, label: 'Реле 3', editable: true, targetBit: 2 },
    { index: 8, label: 'Реле 8', editable: true, targetBit: 7 },
  ],
  sensorNames: ['Куб'],
  hardwareRoles: ['i2c_bus', 'pcf8574_port0'],
};

const STATE_PAYLOAD = {
  supported: true,
  userShadow: 0,
  userShadows: [0],
};

function tool(name: string): ToolDef {
  const found = relayTools().find((candidate) => candidate.name === name);
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

test('oc_get_relay_metadata / state: supported:false (GPIO) → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/relay/metadata', { supported: false }),
    getRoute('/rest/oc/relay/state', { supported: false }),
  ]);
  t.after(() => testCtx.cleanup());

  for (const name of ['oc_get_relay_metadata', 'oc_get_relay_state']) {
    const result = await runTool(tool(name), testCtx.ctx, {});
    assert.equal(result.isError, true, `${name}: GPIO-сборка — не успешный ответ`);
    const payload = payloadOf(result);
    assert.equal(payload.error.code, 'feature_unsupported');
    assert.equal(payload.error.message, 'Реле на PCF8574 недоступны в этой сборке прошивки');
    assert.equal(payload.error.retryable, false, 'повтор не поможет: это ограничение сборки');
    assert.equal(payload.error.details.supported, false);
  }
});

test('oc_relay_override: supported:false → feature_unsupported и никакого POST', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/relay/metadata', { supported: false }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 3,
    enable: true,
    energized: false,
    confirm: true,
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.equal(payload.error.message, 'Реле на PCF8574 недоступны в этой сборке прошивки');
  assert.equal(callsTo(testCtx.calls, 'POST', '/rest/oc/relay/override').length, 0);
});

test('metadata → state: индексы, target_bit, line_count и теневые маски', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/relay/metadata', METADATA_PAYLOAD),
    getRoute('/rest/oc/relay/state', STATE_PAYLOAD),
  ]);
  t.after(() => testCtx.cleanup());

  const metadata = await runTool(tool('oc_get_relay_metadata'), testCtx.ctx, {});
  assert.equal(metadata.isError, undefined);
  const meta = payloadOf(metadata);
  assert.equal(meta.supported, true);
  assert.equal(meta.relay_chip_count, 1);
  assert.equal(meta.relay_line_count, 8);
  assert.deepEqual(
    meta.relays.map((relay: any) => relay.target_bit),
    [0, 1, 2, 7],
    'target_bit — 0-based глобальный бит, отсортирован',
  );
  assert.equal(meta.relays[0].editable, false);
  assert.match(String(meta.relays[0].reason), /AlarmManager/);
  assert.equal('targetBit' in meta.relays[0], false, 'наружу — snake_case target_bit');
  assert.equal(typeof meta.observed_at, 'string');

  const state = await runTool(tool('oc_get_relay_state'), testCtx.ctx, {});
  assert.equal(state.isError, undefined);
  const statePayload = payloadOf(state);
  assert.equal(statePayload.supported, true);
  assert.equal(statePayload.user_shadow, 0, 'активный низкий: 0 = все реле включены');
  assert.deepEqual(statePayload.user_shadows, [0]);
});

test('oc_relay_override: тело с bit/enable/energized, типы и результат', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/relay/metadata', METADATA_PAYLOAD),
    {
      method: 'POST',
      path: '/rest/oc/relay/override',
      reply: () => ({ body: { ok: true } }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 3,
    enable: true,
    energized: false,
    confirm: true,
  });
  assert.equal(result.isError, undefined, 'успешное переключение — не ошибка');
  const payload = payloadOf(result);
  assert.deepEqual(payload, { ok: true, bit: 3, enable: true, energized: false });

  const posts = callsTo(testCtx.calls, 'POST', '/rest/oc/relay/override');
  assert.equal(posts.length, 1, 'одно переключение — один POST, без ретраев');
  assert.deepEqual(posts[0].body, { bit: 3, enable: true, energized: false });
  const body = posts[0].body as Record<string, unknown>;
  assert.equal(typeof body.bit, 'number');
  assert.equal(typeof body.enable, 'boolean');
  assert.equal(typeof body.energized, 'boolean');
  assert.equal('confirm' in body, false, 'confirm — аргумент MCP, на устройство не уходит');
});

test('oc_relay_override: metadata кэшируется на 60 с (чтение инструментом — force)', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/relay/metadata', METADATA_PAYLOAD),
    { method: 'POST', path: '/rest/oc/relay/override', reply: () => ({ body: { ok: true } }) },
  ]);
  t.after(() => testCtx.cleanup());

  // Явное чтение справочника заполняет кэш, override им пользуется — лишнего GET нет.
  await runTool(tool('oc_get_relay_metadata'), testCtx.ctx, {});
  await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 4,
    enable: false,
    energized: false,
    confirm: true,
  });
  assert.equal(
    callsTo(testCtx.calls, 'GET', '/rest/oc/relay/metadata').length,
    1,
    'line_count берётся из кэша metadata, а не отдельным запросом на каждый override',
  );
});

test('403 резерва AlarmManager → validation_failed, а не forbidden_admin_required', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/relay/metadata', METADATA_PAYLOAD),
    {
      method: 'POST',
      path: '/rest/oc/relay/override',
      reply: () => ({
        status: 403,
        body: { error: 'Relay is reserved for AlarmManager' },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 1,
    enable: true,
    energized: true,
    confirm: true,
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.message, 'Реле зарезервировано под аварийную сигнализацию');
  assert.equal(payload.error.details.bit, 1);
  assert.equal(payload.error.details.status, 403);
  assert.equal(payload.error.retryable, false);
  assert.notEqual(
    payload.error.code,
    'forbidden_admin_required',
    'запрет системного реле — не отказ в правах admin',
  );
});

test('400 прошивки (bit out of range) → validation_failed с текстом устройства', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/relay/metadata', METADATA_PAYLOAD),
    {
      method: 'POST',
      path: '/rest/oc/relay/override',
      reply: () => ({ status: 400, body: { error: 'bit out of range' } }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  // bit 2 проходит локальную проверку (line_count 8), решение принимает прошивка.
  const result = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 2,
    enable: true,
    energized: false,
    confirm: true,
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.status, 400);
  assert.equal(payload.error.details.device_error, 'bit out of range');
  assert.match(String(payload.error.message), /bit out of range/);
});

test('bit вне relay_line_count → validation_failed БЕЗ POST', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/relay/metadata', METADATA_PAYLOAD),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 8,
    enable: true,
    energized: false,
    confirm: true,
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.reason, 'bit_out_of_range');
  assert.equal(payload.error.details.relay_line_count, 8);
  assert.equal(
    callsTo(testCtx.calls, 'POST', '/rest/oc/relay/override').length,
    0,
    'заведомо неверный бит не должен уходить POST-ом',
  );
});

test('отрицательный bit и отсутствие enable → validation_failed без сети', async (t) => {
  // Маршрутов нет вовсе: любой запрос к устройству был бы виден.
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const negative = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: -1,
    enable: true,
    energized: false,
    confirm: true,
  });
  assert.equal(negative.isError, true);
  assert.equal(payloadOf(negative).error.code, 'validation_failed');
  assert.equal(payloadOf(negative).error.details.field, 'bit');

  const missingEnable = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 2,
    energized: false,
    confirm: true,
  });
  assert.equal(missingEnable.isError, true);
  const payload = payloadOf(missingEnable);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.field, 'enable');
  assert.match(String(payload.error.hint), /по умолчанию true/);

  const missingEnergized = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 2,
    enable: true,
    confirm: true,
  });
  assert.equal(payloadOf(missingEnergized).error.details.field, 'energized');

  assert.equal(testCtx.calls.length, 0, 'валидация аргументов локальная: сеть не трогается');
});

test('oc_relay_override без confirm → confirmation_required и ни одного запроса', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_relay_override'), testCtx.ctx, {
    bit: 3,
    enable: true,
    energized: false,
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'confirmation_required');
  assert.equal(payload.error.details.required, 'confirm');
  assert.equal(testCtx.calls.length, 0, 'без confirm риск-политика не пускает к устройству');
});

test('SPA-заглушка 200 text/html на metadata → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/oc/relay/metadata',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SSVC OpenConnect</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_relay_metadata'), testCtx.ctx, {});
  assert.equal(result.isError, true, 'SPA-заглушка не должна выглядеть успехом');
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.match(String(payload.error.message), /SPA|JSON/);
  assert.equal('relays' in payload, false);
});

test('registerRelayTools: три инструмента видны в tools/list с аннотациями риска', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerRelayTools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp5-relay-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(
    listed.tools.map((entry) => entry.name).sort(),
    ['oc_get_relay_metadata', 'oc_get_relay_state', 'oc_relay_override'],
  );

  for (const name of ['oc_get_relay_metadata', 'oc_get_relay_state']) {
    const read = listed.tools.find((entry) => entry.name === name);
    assert.equal(read?.annotations?.readOnlyHint, true, `${name}: чтение`);
    assert.equal(read?.annotations?.destructiveHint, false);
  }

  const override = listed.tools.find((entry) => entry.name === 'oc_relay_override');
  assert.equal(override?.annotations?.readOnlyHint, false);
  assert.equal(override?.annotations?.destructiveHint, true, 'риск 2 влияет на процесс');
  assert.equal(override?.title, 'Ручное управление реле');
  const schema = override?.inputSchema as
    | { properties?: Record<string, unknown>; required?: string[] }
    | undefined;
  assert.ok(schema?.properties?.bit !== undefined);
  assert.ok(schema?.properties?.confirm !== undefined);
  assert.deepEqual(
    [...(schema?.required ?? [])].sort(),
    ['bit', 'enable', 'energized'],
    'enable и energized обязательны в схеме, confirm — нет',
  );
});
