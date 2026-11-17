/**
 * Тесты WP5: аппаратная конфигурация и сбои (mcp-server-agents.md §9).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети. Проверяются ловушки
 * контракта прошивки (mcp-contract-notes.md §B.10, §B.11, §D.5):
 *  - GET отдаёт конфигурацию целиком + capabilities + needs_reboot + пробы, а состав
 *    проб зависит от сборки (их может не быть вовсе);
 *  - PUT принимает ЧАСТИЧНЫЙ объект без обёртки; тело уходит ровно как аргумент config;
 *  - `needsReboot:true` → `hint` про oc_restart, при false — никакого hint;
 *  - 400 прошивки → `validation_failed` с текстом устройства;
 *  - `faults` может отсутствовать (сбоев нет), `i2cScan` может быть пустым — это не ошибка;
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
import { registerHardwareTools, hardwareTools } from '../src/tools/hardware.tools.js';
import {
  makeTestContext,
  signInRoute,
  type FakeCall,
  type FakeRoute,
} from './fakeHttp.js';

/** Фикстура §B.10: включённые датчик давления, RTC и LCD, реле на PCF8574. */
const CONFIG_PAYLOAD = {
  schemaVersion: 2,
  pressureSensorEnabled: true,
  bmp581I2cAddress: 71,
  userRelayPcfEnabled: true,
  relayPcf8574Addresses: [36],
  pendingReboot: false,
  rtcEnabled: true,
  ds3231I2cAddress: 104,
  oledEnabled: false,
  lcd1602Enabled: true,
  lcd1602I2cAddress: 39,
  xkc25Sensors: [],
  xkc25AutoPause: true,
  xkc25AbortRectification: false,
  xkc25TriggerAction: 1,
  xkc25SharedTankIndex: 0,
  xkc25StartGuardMode: 2,
  bmp581ProbeOk: true,
  ds3231ProbeOk: true,
  lcd1602ProbeOk: false,
  relayPcf8574ProbeOk: [true],
  relayHardwareOk: true,
  capabilities: { pinoutUseGpio: false, relayOptionsAvailable: true },
  needsReboot: false,
};

/** Фикстура §B.11: NACK LCD1602, шина жива, ответили 0x27 и 0x68. */
const FAULTS_PAYLOAD = {
  faults: [
    {
      code: 'i2c_nack',
      codeLabel: 'I2C: нет подтверждения (NACK)',
      address: 39,
      role: 'lcd1602',
    },
  ],
  count: 1,
  i2cBusActive: true,
  i2cScan: [39, 104],
};

function tool(name: string): ToolDef {
  const found = hardwareTools().find((candidate) => candidate.name === name);
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

test('oc_get_hardware_config: конфигурация, capabilities, пробы и needs_reboot:false', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/hardware-config', CONFIG_PAYLOAD),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_hardware_config'), testCtx.ctx, {});
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);

  assert.equal(payload.ok, true);
  assert.equal(payload.config.pressureSensorEnabled, true);
  assert.equal(payload.config.lcd1602Enabled, true);
  assert.deepEqual(payload.config.capabilities, {
    pinoutUseGpio: false,
    relayOptionsAvailable: true,
  });
  assert.deepEqual(payload.capabilities, {
    pinoutUseGpio: false,
    relayOptionsAvailable: true,
  });
  assert.equal(payload.needs_reboot, false);
  assert.deepEqual(payload.probes, {
    bmp581_probe_ok: true,
    ds3231_probe_ok: true,
    lcd1602_probe_ok: false,
    relay_pcf8574_probe_ok: [true],
    relay_hardware_ok: true,
  });
  assert.equal('hint' in payload, false, 'перезагрузка не нужна — hint не нужен');
  assert.equal(typeof payload.observed_at, 'string');
});

test('oc_get_hardware_config: минимум полей — пустые capabilities/пробы и needs_reboot:false', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/hardware-config', { pressureSensorEnabled: true }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_hardware_config'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'пробы зависят от сборки и могут отсутствовать');
  const payload = payloadOf(result);
  assert.deepEqual(payload.capabilities, {});
  assert.deepEqual(payload.probes, {});
  assert.equal(payload.needs_reboot, false);
});

test('oc_update_hardware_config: частичное тело БЕЗ обёртки и конфиг после записи', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'PUT',
      path: '/rest/oc/hardware-config',
      reply: () => ({
        body: {
          ...CONFIG_PAYLOAD,
          pressureSensorEnabled: false,
          needsReboot: false,
        },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_hardware_config'), testCtx.ctx, {
    config: { pressureSensorEnabled: false },
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.config.pressureSensorEnabled, false);
  assert.equal(payload.needs_reboot, false);
  assert.equal('hint' in payload, false);

  const puts = callsTo(testCtx.calls, 'PUT', '/rest/oc/hardware-config');
  assert.equal(puts.length, 1);
  assert.deepEqual(
    puts[0].body,
    { pressureSensorEnabled: false },
    'тело — ровно частичный объект, без обёртки config',
  );
});

test('oc_update_hardware_config: needsReboot:true → hint про oc_restart', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'PUT',
      path: '/rest/oc/hardware-config',
      reply: () => ({
        body: {
          ...CONFIG_PAYLOAD,
          lcd1602Enabled: false,
          pendingReboot: true,
          needsReboot: true,
        },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_hardware_config'), testCtx.ctx, {
    config: { lcd1602Enabled: false },
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.needs_reboot, true);
  assert.equal(payload.hint, 'изменения вступят в силу после oc_restart');
  assert.match(String(payload.hint), /oc_restart/);
});

test('oc_update_hardware_config: 400 прошивки → validation_failed с полями', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'PUT',
      path: '/rest/oc/hardware-config',
      reply: () => ({
        status: 400,
        body: {
          success: false,
          error: 'validation_failed',
          message: 'Настройки не прошли проверку: адреса, пины или привязка зон',
        },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_hardware_config'), testCtx.ctx, {
    config: { bmp581I2cAddress: 7 },
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.status, 400);
  assert.equal(payload.error.details.device_error, 'validation_failed');
  assert.match(String(payload.error.message), /Настройки не прошли проверку/);
  assert.equal(
    callsTo(testCtx.calls, 'GET', '/rest/oc/hardware-config').length,
    0,
    'при 400 перечитывать конфигурацию не нужно',
  );
});

test('oc_update_hardware_config: пустой config → validation_failed без сети', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_hardware_config'), testCtx.ctx, {
    config: {},
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.reason, 'empty_config');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_update_hardware_config: 403 → forbidden_admin_required', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'PUT',
      path: '/rest/oc/hardware-config',
      reply: () => ({ status: 403, body: {} }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_hardware_config'), testCtx.ctx, {
    config: { pressureSensorEnabled: true },
  });
  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'forbidden_admin_required');
});

test('oc_get_hardware_faults: сбои, count и адреса I²C', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/hardware-faults', FAULTS_PAYLOAD),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_hardware_faults'), testCtx.ctx, {});
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.deepEqual(payload.faults, [
    {
      code: 'i2c_nack',
      code_label: 'I2C: нет подтверждения (NACK)',
      address: 39,
      role: 'lcd1602',
    },
  ]);
  assert.equal(payload.count, 1);
  assert.equal(payload.i2c_bus_active, true);
  assert.deepEqual(payload.i2c_scan, [39, 104]);
  assert.equal(typeof payload.observed_at, 'string');
});

test('oc_get_hardware_faults: отсутствие faults и пустой i2c_scan — не ошибка', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute('/rest/oc/hardware-faults', { i2cBusActive: true, i2cScan: [] }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_hardware_faults'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'сбоев нет — пустой массив, а не ошибка');
  const payload = payloadOf(result);
  assert.deepEqual(payload.faults, []);
  assert.equal(payload.count, 0);
  assert.equal(payload.i2c_bus_active, true);
  assert.deepEqual(payload.i2c_scan, []);
});

test('SPA-заглушка 200 text/html на hardware-config → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/oc/hardware-config',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SSVC OpenConnect</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_hardware_config'), testCtx.ctx, {});
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.match(String(payload.error.message), /SPA|JSON/);
  assert.equal('config' in payload, false);
});

test('SPA-заглушка 200 text/html на hardware-faults → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/oc/hardware-faults',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SSVC OpenConnect</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_hardware_faults'), testCtx.ctx, {});
  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'feature_unsupported');
  assert.equal('faults' in payloadOf(result), false);
});

test('registerHardwareTools: три инструмента видны в tools/list с аннотациями риска', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerHardwareTools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp5-hardware-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(
    listed.tools.map((entry) => entry.name).sort(),
    ['oc_get_hardware_config', 'oc_get_hardware_faults', 'oc_update_hardware_config'],
  );

  for (const name of ['oc_get_hardware_config', 'oc_get_hardware_faults']) {
    const read = listed.tools.find((entry) => entry.name === name);
    assert.equal(read?.annotations?.readOnlyHint, true, `${name}: чтение`);
    assert.equal(read?.annotations?.destructiveHint, false);
  }

  const update = listed.tools.find((entry) => entry.name === 'oc_update_hardware_config');
  assert.equal(update?.annotations?.readOnlyHint, false);
  assert.equal(update?.annotations?.destructiveHint, false, 'риск 1 — обратимая запись');
  assert.equal(update?.title, 'Изменить аппаратную конфигурацию');
  const schema = update?.inputSchema as
    | { properties?: Record<string, unknown>; required?: string[] }
    | undefined;
  assert.ok(schema?.properties?.config !== undefined);
  assert.deepEqual(schema?.required, ['config']);
});
