/**
 * Тесты WP3: настройки SSVC (mcp-server-agents.md §7).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети. Проверяются именно
 * ловушки контракта (mcp-server.md §2.5 п.4–5):
 *  - GET отдаёт вложенную обёртку, PUT — плоское тело без `ssvcSettings`;
 *  - `ignored`/`skipped` доносятся до агента;
 *  - 503 `partial:true` — успех с partial, без исключения и без повторного PUT;
 *  - `strict:true` превращает неполноту в validation_failed;
 *  - 400 с полями — validation_failed с details.fields;
 *  - SPA-заглушка (200 text/html) не считается успехом.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { createMcpServer } from '../src/mcp/server.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { registerSettingsTools, settingsTools } from '../src/tools/settings.tools.js';
import { makeTestContext, signInRoute, type FakeCall, type FakeReply, type FakeRoute } from './fakeHttp.js';

/** Фикстура §F.4: набор ключей late_heads-ветки. */
const SETTINGS = {
  heads: [1.2, 3],
  hearts: [0.9, 2],
  late_heads: [0.7, 2],
  hyst: 0.5,
  decrement: 2,
  formula: 1,
  tank_mmhg: 10,
  heads_timer: 3600,
  release_timer: 0,
};

function tool(name: string): ToolDef {
  const found = settingsTools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

function settingsCalls(calls: FakeCall[], method: 'GET' | 'PUT'): FakeCall[] {
  return calls.filter((call) => call.method === method && call.path === '/rest/settings');
}

/** Маршрут GET: отдаёт обёртку с актуальным состоянием. */
function getRoute(current: () => Record<string, unknown>, extra: Record<string, unknown> = {}): FakeRoute {
  return {
    method: 'GET',
    path: '/rest/settings',
    reply: () => ({ body: { ssvcSettings: current(), ...extra } }),
  };
}

/** Маршрут PUT с настраиваемым ответом и (опционально) применением тела к состоянию. */
function putRoute(
  reply: (call: FakeCall) => FakeReply,
): FakeRoute {
  return { method: 'PUT', path: '/rest/settings', reply };
}

test('oc_get_settings: разворачивает ssvcSettings и не отдаёт обёртку наружу', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute(() => SETTINGS, { mode: 'late_heads' }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_settings'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'чтение не должно быть ошибкой');
  const payload = payloadOf(result);

  assert.deepEqual(payload.settings, SETTINGS, 'наружу — только содержимое ssvcSettings');
  assert.equal('ssvcSettings' in payload, false, 'обёртка не должна утекать в ответ');
  assert.equal('mode' in payload, false, 'лишние поля GET наружу не выносятся');
  assert.equal(typeof payload.observed_at, 'string');

  const gets = settingsCalls(testCtx.calls, 'GET');
  assert.equal(gets.length, 1, 'один GET на вызов');
  assert.equal(gets[0].method, 'GET');
});

test('oc_update_settings: PUT плоским телом без ssvcSettings и с перечитыванием applied', async (t) => {
  let current: Record<string, unknown> = { ...SETTINGS };
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute(() => current),
    putRoute((call) => {
      const body = call.body as Record<string, unknown>;
      current = { ...current, ...body };
      return {
        body: { success: true, accepted: Object.keys(body).length, ignored: [], skipped: [] },
      };
    }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_settings'), testCtx.ctx, {
    settings: { hyst: 0.75 },
  });
  assert.equal(result.isError, undefined, 'успешная запись не должна быть ошибкой');
  const payload = payloadOf(result);

  assert.equal(payload.accepted, 1);
  assert.deepEqual(payload.ignored, []);
  assert.deepEqual(payload.skipped, []);
  assert.equal(payload.partial, false);
  assert.equal(payload.applied.hyst, 0.75, 'applied — перечитанные настройки, а не тело запроса');

  const puts = settingsCalls(testCtx.calls, 'PUT');
  assert.equal(puts.length, 1, 'одно тело — один PUT');
  assert.deepEqual(puts[0].body, { hyst: 0.75 }, 'тело ровно args.settings');
  assert.equal(
    Object.prototype.hasOwnProperty.call(puts[0].body as object, 'ssvcSettings'),
    false,
    'в теле PUT не должно быть обёртки ssvcSettings',
  );
  assert.equal(puts[0].headers['content-type'], 'application/json');

  const gets = settingsCalls(testCtx.calls, 'GET');
  assert.equal(gets.length, 1, 'после записи настройки перечитываются один раз');
});

test('oc_update_settings: ignored и skipped доносятся, неизвестный ключ уходит на устройство', async (t) => {
  let current: Record<string, unknown> = { ...SETTINGS };
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute(() => current),
    putRoute((call) => {
      const body = call.body as Record<string, unknown>;
      const { unknown_setting: _dropped, ...known } = body;
      current = { ...current, ...known };
      return {
        body: {
          success: true,
          accepted: Object.keys(known).length,
          ignored: ['unknown_setting'],
          skipped: ['predec=7'],
        },
      };
    }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_settings'), testCtx.ctx, {
    settings: { hyst: 0.5, unknown_setting: 1 },
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);

  assert.equal(payload.ok, true);
  assert.deepEqual(payload.ignored, ['unknown_setting']);
  assert.deepEqual(payload.skipped, ['predec=7']);
  assert.equal(payload.partial, false);

  const puts = settingsCalls(testCtx.calls, 'PUT');
  assert.equal(puts.length, 1);
  assert.deepEqual(
    puts[0].body,
    { hyst: 0.5, unknown_setting: 1 },
    'неизвестный ключ не отбрасывается локально: его отбракует прошивка и вернёт в ignored',
  );
});

test('503 partial:true → ok:true, partial:true, hint и никакого повторного PUT', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute(() => ({ ...SETTINGS, hyst: 0.6 })),
    putRoute(() => ({
      status: 503,
      body: {
        success: false,
        partial: true,
        accepted: 1,
        failed: ['decrement=2'],
        ignored: ['sound'],
        skipped: [],
        message: 'Часть настроек не попала в очередь контроллера',
      },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_settings'), testCtx.ctx, {
    settings: { hyst: 0.6, decrement: 2 },
  });
  assert.equal(result.isError, undefined, 'partial — не ошибка инструмента');
  const payload = payloadOf(result);

  assert.equal(payload.ok, true);
  assert.equal(payload.partial, true);
  assert.equal(payload.accepted, 1);
  assert.deepEqual(payload.failed, ['decrement=2']);
  assert.deepEqual(payload.ignored, ['sound']);
  assert.equal(typeof payload.hint, 'string');
  assert.match(payload.hint, /повторять весь запрос нельзя/);
  assert.equal(payload.applied.hyst, 0.6, 'partial тоже перечитывается: видно фактическое состояние');

  assert.equal(settingsCalls(testCtx.calls, 'PUT').length, 1, 'PUT не повторяется при partial');
  assert.equal(settingsCalls(testCtx.calls, 'GET').length, 1, 'перечитывание ровно одно');
});

test('strict:true при ignored/skipped → validation_failed с перечнем', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    putRoute(() => ({
      body: { success: true, accepted: 1, ignored: ['unknown_setting'], skipped: ['predec=7'] },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_settings'), testCtx.ctx, {
    settings: { hyst: 0.5, unknown_setting: 1 },
    strict: true,
  });

  assert.equal(result.isError, true, 'strict-нарушение должно быть isError');
  const payload = payloadOf(result);
  assert.equal(payload.ok, false);
  assert.equal(payload.error.code, 'validation_failed');
  assert.deepEqual(payload.error.details.ignored, ['unknown_setting']);
  assert.deepEqual(payload.error.details.skipped, ['predec=7']);
  assert.equal(payload.error.details.reason, 'strict');
  assert.equal(
    settingsCalls(testCtx.calls, 'GET').length,
    0,
    'при strict-нарушении перечитывать нечего: отказ уже известен',
  );
});

test('strict:true при 503 partial → validation_failed, а не тихий успех', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    putRoute(() => ({
      status: 503,
      body: { success: false, partial: true, accepted: 1, failed: ['decrement=2'] },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_settings'), testCtx.ctx, {
    settings: { hyst: 0.5, decrement: 2 },
    strict: true,
  });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.partial, true);
  assert.deepEqual(payload.error.details.failed, ['decrement=2']);
});

test('400 с полями → validation_failed с details.fields и без перечитывания', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    putRoute(() => ({
      status: 400,
      body: { hyst: 'Invalid value', decrement: 'Invalid value' },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_update_settings'), testCtx.ctx, {
    settings: { hyst: 'oops', decrement: 99 },
  });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.ok, false);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.status, 400);
  assert.deepEqual(payload.error.details.fields, {
    hyst: 'Invalid value',
    decrement: 'Invalid value',
  });
  assert.deepEqual(payload.error.details.invalid_keys, ['hyst', 'decrement']);
  assert.match(payload.error.message, /hyst/);
  assert.equal(
    settingsCalls(testCtx.calls, 'GET').length,
    0,
    'при 400 не применено ничего: перечитывать не нужно',
  );
});

test('SPA-заглушка 200 text/html → feature_unsupported, а не пустые настройки', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/settings',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SSVC OpenConnect</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_get_settings'), testCtx.ctx, {});

  assert.equal(result.isError, true, 'SPA-заглушка не должна выглядеть успехом');
  const payload = payloadOf(result);
  assert.equal(payload.ok, false);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.match(payload.error.message, /SPA|JSON/);
  assert.equal('settings' in payload, false);
});

/**
 * Калиброванные клапаны и пары, как на живой сессии «18 л / 40 % / 2 кВт»:
 * derived обязан дать те же мл/ч, что считает модель `@ssvc/rectification`.
 */
const CALIBRATED = {
  heads: [1.7, 120],
  late_heads: [1, 60],
  hearts: [1, 10],
  tails: [2, 2],
  valve_bw: [7000, 12000, 7000],
  heads_timer: 7800,
  late_heads_timer: 15600,
};

test('oc_get_settings: derived — мл/ч, скважность и объёмы по таймерам', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), getRoute(() => CALIBRATED)]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_settings'), testCtx.ctx, {}));

  assert.equal(payload.derived.heads.flow_mlh, 99.2, '7000 × 1.7 / 120');
  assert.equal(payload.derived.heads.duty_pct, 1.42);
  assert.equal(payload.derived.heads.period_s, 120);
  assert.match(String(payload.derived.method), /flow_mlh = valve_bw\[i\] \* opening_s \/ period_s/);
  assert.equal(payload.derived.units.flow_mlh, 'мл/ч');
  assert.equal(payload.derived.valve_index.late_heads, 2, 'подголовники идут через клапан 3');
  assert.equal(payload.derived.late_heads.flow_mlh, 116.7, '7000 × 1 / 60');
  assert.equal(payload.derived.hearts.flow_mlh, 1200, '12000 × 1 / 10');
  assert.equal(payload.volumes_ml.heads, 215, '99.2 мл/ч × 7800 с');
  assert.equal(payload.volumes_ml.late_heads, 506, '116.7 мл/ч × 15600 с');
});

test('oc_get_settings: без valve_bw скорости не определены, есть предупреждение', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute(() => ({ heads: [1.7, 120], heads_timer: 7800 })),
  ]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_settings'), testCtx.ctx, {}));

  assert.equal(payload.derived.heads.flow_mlh, 0);
  assert.equal(payload.derived.heads.duty_pct, 1.42, 'скважность считается и без valve_bw');
  const codes = payload.warnings.map((entry: { code: string }) => entry.code);
  assert.ok(codes.includes('valve_bw_missing'));
});

test('oc_get_settings: tails на ветке late_heads помечен ignored с причиной', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), getRoute(() => CALIBRATED)]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_settings'), testCtx.ctx, {}));

  assert.equal(payload.derived.tails.ignored, true, 'на 2.3 этапа хвостов нет');
  const tailsWarning = payload.warnings.find(
    (entry: { field: string }) => entry.field === 'tails',
  );
  assert.ok(tailsWarning !== undefined, 'должно быть объяснение, почему tails не работает');
  assert.equal(tailsWarning.code, 'field_ignored_by_firmware');
});

test('oc_get_settings: predec=13 при hyst=0.06 — предупреждение о совместности', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute(() => ({ ...CALIBRATED, predec: 13, hyst: 0.06 })),
  ]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_get_settings'), testCtx.ctx, {}));

  const warning = payload.warnings.find(
    (entry: { code: string }) => entry.code === 'predec_hyst_incompatible',
  );
  assert.ok(warning !== undefined, 'несовместимость predec/hyst должна быть видна агенту');
  assert.equal(warning.field, 'predec');
});

test('oc_update_settings: ignored объясняется как read-only, warnings — по применённому', async (t) => {
  let current: Record<string, unknown> = { ...SETTINGS, auto_mode: false };
  const testCtx = await makeTestContext([
    signInRoute(),
    getRoute(() => current),
    putRoute((call) => {
      const body = call.body as Record<string, unknown>;
      const { auto_mode: _dropped, ...applied } = body;
      current = { ...current, ...applied };
      return {
        body: { success: true, accepted: Object.keys(applied).length, ignored: ['auto_mode'], skipped: [] },
      };
    }),
  ]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(tool('oc_update_settings'), testCtx.ctx, {
      settings: { hyst: 0.06, predec: 13, auto_mode: true },
    }),
  );

  assert.deepEqual(payload.ignored, ['auto_mode']);
  assert.match(payload.ignored_reasons.auto_mode, /read-only/);
  const warning = payload.warnings.find(
    (entry: { code: string }) => entry.code === 'predec_hyst_incompatible',
  );
  assert.ok(warning !== undefined, 'после записи видно, что комбинация несовместима');
});

test('registerSettingsTools: оба инструмента видны в tools/list с нужными аннотациями', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerSettingsTools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp3-settings-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(
    listed.tools.map((entry) => entry.name).sort(),
    ['oc_get_settings', 'oc_update_settings'],
  );

  const read = listed.tools.find((entry) => entry.name === 'oc_get_settings');
  assert.equal(read?.annotations?.readOnlyHint, true);
  assert.equal(read?.title, 'Действующие настройки SSVC');

  const write = listed.tools.find((entry) => entry.name === 'oc_update_settings');
  assert.equal(write?.annotations?.readOnlyHint, false);
  assert.equal(write?.annotations?.destructiveHint, false, 'риск 1 — обратимая запись');
  assert.equal(write?.title, 'Изменить настройки SSVC');
  const properties = (write?.inputSchema as { properties?: Record<string, unknown> } | undefined)
    ?.properties;
  assert.ok(properties?.settings !== undefined, 'settings обязателен в схеме');
  assert.ok(properties?.strict !== undefined, 'strict объявлен в схеме');
});
