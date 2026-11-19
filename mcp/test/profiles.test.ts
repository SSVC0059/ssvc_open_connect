/**
 * Тесты WP8: профили ректификации (mcp-server-agents.md §12).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети и без диска вне
 * временного каталога. Проверяются ловушки контракта профилей:
 *  - `GET /rest/profiles` — массив, а не объект;
 *  - `404` активного профиля — это `active_id:null`, а не ошибка;
 *  - `404` профиля — `not_found` с id в details;
 *  - `confirm` риска 2 и `reason` риска 3 проверяются ДО сети;
 *  - `allowDestructive:false` → `disabled_by_policy`;
 *  - DELETE уходит с телом `{id}`;
 *  - SPA-заглушка (`200 text/html`) — `feature_unsupported`, а не пустой список;
 *  - после записи наружу уходит перечитанное состояние, а не эхо аргументов.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { createMcpServer } from '../src/mcp/server.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { registerProfilesTools, profilesTools } from '../src/tools/profiles.tools.js';
import {
  makeTestContext,
  signInRoute,
  type FakeCall,
  type FakeRoute,
} from './fakeHttp.js';

/** Элемент списка профилей в форме `GET /rest/profiles`. */
interface ProfileFixture {
  id: string;
  name: string;
  createdAt: string;
  isApplied: boolean;
}

interface DeviceState {
  profiles: ProfileFixture[];
  active: string | null;
}

function initialState(): DeviceState {
  return {
    profiles: [
      {
        id: 'p1',
        name: 'Спирт-сырец',
        createdAt: '2026-01-01T00:00:00Z',
        isApplied: true,
      },
      {
        id: 'p2',
        name: 'Спирт-ректификат',
        createdAt: '2026-01-02T00:00:00Z',
        isApplied: false,
      },
    ],
    active: 'p1',
  };
}

/** Полное содержимое профиля: id/name/createdAt + ключи наблюдателей прошивки. */
const PROFILE_CONTENT = {
  id: 'p1',
  name: 'Спирт-сырец',
  createdAt: '2026-01-01T00:00:00Z',
  ssvcSettings: { heads: [1.2, 3], hyst: 0.5, formula: 1 },
  alarmThresholds: { '28FF64A1B2C3D4E5': { enabled: true, dangerous: 80 } },
};

function tool(name: string): ToolDef {
  const found = profilesTools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

function callsFor(calls: FakeCall[], method: string, path: string): FakeCall[] {
  return calls.filter((call) => call.method === method && call.path === path);
}

/** `GET /rest/profiles`: отдаёт текущий массив состояния. */
function listRoute(state: DeviceState): FakeRoute {
  return {
    method: 'GET',
    path: '/rest/profiles',
    reply: () => ({ body: state.profiles }),
  };
}

/** `GET /rest/profiles/active`: 404 — активного профиля нет. */
function activeRoute(state: DeviceState): FakeRoute {
  return {
    method: 'GET',
    path: '/rest/profiles/active',
    reply: () =>
      state.active === null
        ? { status: 404, body: { error: 'No active profile found' } }
        : { body: { id: state.active } },
  };
}

test('oc_profiles_list: массив профилей и активный id', async (t) => {
  const state = initialState();
  const testCtx = await makeTestContext([signInRoute(), listRoute(state), activeRoute(state)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profiles_list'), testCtx.ctx, {});

  assert.equal(result.isError, undefined, 'чтение не должно быть ошибкой');
  const payload = payloadOf(result);
  assert.deepEqual(payload.profiles, state.profiles, 'профили отдаются как массив, без обёртки');
  assert.equal(payload.active_id, 'p1');
  assert.equal(typeof payload.observed_at, 'string');
  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/profiles').length, 1);
  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/profiles/active').length, 1);
});

test('oc_profiles_list: 404 активного профиля → active_id:null, не ошибка', async (t) => {
  const state = initialState();
  state.active = null;
  const testCtx = await makeTestContext([signInRoute(), listRoute(state), activeRoute(state)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profiles_list'), testCtx.ctx, {});

  assert.equal(result.isError, undefined, 'отсутствие активного профиля — штатное состояние');
  const payload = payloadOf(result);
  assert.equal(payload.active_id, null);
  assert.equal(payload.profiles.length, 2);
});

test('oc_profile_get: полное содержимое профиля по id', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/profiles/content',
      reply: (call) => {
        assert.equal(call.query.id, 'p1', 'id уходит query-параметром');
        return { body: PROFILE_CONTENT };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_get'), testCtx.ctx, { id: 'p1' });

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.id, 'p1');
  assert.deepEqual(payload.profile, PROFILE_CONTENT, 'профиль отдаётся целиком, без обёртки');
});

test('oc_profile_get: 404 профиля → not_found с id в details', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/profiles/content',
      reply: () => ({
        status: 404,
        body: { error: 'Profile content not found or failed to retrieve' },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_get'), testCtx.ctx, { id: 'p404' });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.ok, false);
  assert.equal(payload.error.code, 'not_found');
  assert.equal(payload.error.details.id, 'p404');
  assert.equal(payload.error.details.status, 404);
});

test('oc_profile_get: пустой id → validation_failed без сетевого вызова', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_get'), testCtx.ctx, { id: '   ' });

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'validation_failed');
  assert.equal(testCtx.calls.length, 0, 'локальная валидация не должна ходить в устройство');
});

test('oc_profile_create: id из ответа, тело name+content, перечитывание списка', async (t) => {
  const state = initialState();
  let createBody: unknown;
  const testCtx = await makeTestContext([
    signInRoute(),
    listRoute(state),
    {
      method: 'POST',
      path: '/rest/profiles',
      reply: (call) => {
        createBody = call.body;
        const body = call.body as { name: string };
        state.profiles.push({
          id: 'p3',
          name: body.name,
          createdAt: '2026-01-03T00:00:00Z',
          isApplied: false,
        });
        return { status: 201, body: { success: true, id: 'p3' } };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const content = { ssvcSettings: { hyst: 0.7 } };
  const result = await runTool(tool('oc_profile_create'), testCtx.ctx, {
    name: 'Новый профиль',
    content,
  });

  assert.equal(result.isError, undefined, 'успешное создание не должно быть ошибкой');
  const payload = payloadOf(result);
  assert.equal(payload.id, 'p3');
  assert.equal(payload.profile.name, 'Новый профиль', 'profile — перечитанные метаданные профиля');
  assert.equal(payload.profile.isApplied, false);
  assert.deepEqual(createBody, { name: 'Новый профиль', content }, 'тело: name + content');

  assert.equal(callsFor(testCtx.calls, 'POST', '/rest/profiles').length, 1);
  assert.equal(
    callsFor(testCtx.calls, 'GET', '/rest/profiles').length,
    1,
    'после создания список перечитывается один раз',
  );
});

test('oc_profile_copy: id копии, тело sourceId/newName', async (t) => {
  const state = initialState();
  let copyBody: unknown;
  const testCtx = await makeTestContext([
    signInRoute(),
    listRoute(state),
    {
      method: 'POST',
      path: '/rest/profiles/copy',
      reply: (call) => {
        copyBody = call.body;
        const body = call.body as { newName: string };
        state.profiles.push({
          id: 'p9',
          name: body.newName,
          createdAt: '2026-01-04T00:00:00Z',
          isApplied: false,
        });
        return { status: 201, body: { success: true, id: 'p9' } };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_copy'), testCtx.ctx, {
    source_id: 'p1',
    new_name: 'Копия сырца',
  });

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.id, 'p9');
  assert.equal(payload.profile.name, 'Копия сырца', 'profile — фактическое имя копии');
  assert.deepEqual(copyBody, { sourceId: 'p1', newName: 'Копия сырца' });
});

test('oc_profile_copy: 409 «source not found» → not_found с source_id', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'POST',
      path: '/rest/profiles/copy',
      reply: () => ({
        status: 409,
        body: { error: 'Failed to copy profile. source not found ' },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_copy'), testCtx.ctx, {
    source_id: 'p404',
    new_name: 'Копия',
  });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'not_found');
  assert.equal(payload.error.details.source_id, 'p404');
});

test('oc_profile_rename: PUT /rest/profiles/meta с телом id/name', async (t) => {
  const state = initialState();
  let metaBody: unknown;
  const testCtx = await makeTestContext([
    signInRoute(),
    listRoute(state),
    {
      method: 'PUT',
      path: '/rest/profiles/meta',
      reply: (call) => {
        metaBody = call.body;
        const body = call.body as { id: string; name: string };
        const target = state.profiles.find((profile) => profile.id === body.id);
        if (target !== undefined) target.name = body.name;
        return { body: { success: 'Profile updated' } };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_rename'), testCtx.ctx, {
    id: 'p2',
    name: 'Новое имя',
  });

  assert.equal(result.isError, undefined);
  assert.deepEqual(metaBody, { id: 'p2', name: 'Новое имя' }, 'тело: id + name');
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.profile.name, 'Новое имя', 'наружу — фактическое имя после записи');
});

test('oc_profile_update_content: POST /rest/profiles/content с телом id/content', async (t) => {
  const state = initialState();
  let contentBody: unknown;
  const testCtx = await makeTestContext([
    signInRoute(),
    listRoute(state),
    {
      method: 'POST',
      path: '/rest/profiles/content',
      reply: (call) => {
        contentBody = call.body;
        return { body: { success: 'Profile content updated' } };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const content = { ssvcSettings: { hyst: 0.6 }, alarmThresholds: {} };
  const result = await runTool(tool('oc_profile_update_content'), testCtx.ctx, {
    id: 'p1',
    content,
  });

  assert.equal(result.isError, undefined);
  assert.deepEqual(contentBody, { id: 'p1', content });
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.profile.id, 'p1');
});

test('oc_profile_update_content: 404 профиля → not_found', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'POST',
      path: '/rest/profiles/content',
      reply: () => ({
        status: 404,
        body: { error: 'Profile not found or failed to update content' },
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_update_content'), testCtx.ctx, {
    id: 'p404',
    content: { ssvcSettings: {} },
  });

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'not_found');
  assert.equal(payloadOf(result).error.details.id, 'p404');
});

test('oc_profile_save_current: POST /rest/profiles/save с телом id', async (t) => {
  const state = initialState();
  let saveBody: unknown;
  const testCtx = await makeTestContext([
    signInRoute(),
    listRoute(state),
    {
      method: 'POST',
      path: '/rest/profiles/save',
      reply: (call) => {
        saveBody = call.body;
        return { body: { success: true, message: 'Current settings saved to profile.' } };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_save_current'), testCtx.ctx, { id: 'p1' });

  assert.equal(result.isError, undefined);
  assert.deepEqual(saveBody, { id: 'p1' });
  assert.equal(payloadOf(result).ok, true);
});

test('oc_profile_apply без confirm → confirmation_required без сетевых вызовов', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_apply'), testCtx.ctx, { id: 'p2' });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'confirmation_required');
  assert.match(payload.error.hint, /confirm: true/);
  assert.equal(testCtx.calls.length, 0, 'без confirm устройство не должно получать запрос');
});

test('oc_profile_apply с confirm → set-active и фактический applied_id', async (t) => {
  const state = initialState();
  let setActiveBody: unknown;
  const testCtx = await makeTestContext([
    signInRoute(),
    activeRoute(state),
    {
      method: 'POST',
      path: '/rest/profiles/set-active',
      reply: (call) => {
        setActiveBody = call.body;
        const body = call.body as { id: string };
        state.active = body.id;
        return { body: { success: 'Profile set as active and applied' } };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_apply'), testCtx.ctx, {
    id: 'p2',
    confirm: true,
  });

  assert.equal(result.isError, undefined);
  assert.deepEqual(setActiveBody, { id: 'p2' });
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.applied_id, 'p2', 'applied_id — ответ /rest/profiles/active, а не аргумент');
  assert.equal(payload.warning, undefined, 'расхождений нет — предупреждения нет');
  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/profiles/active').length, 1);
});

test('oc_profile_apply: устройство осталось на другом профиле → warning, не отказ', async (t) => {
  const state = initialState();
  const testCtx = await makeTestContext([
    signInRoute(),
    activeRoute(state),
    {
      method: 'POST',
      path: '/rest/profiles/set-active',
      reply: () => ({ body: { success: 'Profile set as active and applied' } }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_apply'), testCtx.ctx, {
    id: 'p2',
    confirm: true,
  });

  assert.equal(result.isError, undefined, 'запись уже произошла: повторять её нельзя');
  const payload = payloadOf(result);
  assert.equal(payload.applied_id, 'p1', 'фактическое состояние устройства');
  assert.match(payload.warning, /p1/);
});

test('oc_profile_delete без reason → validation_failed без сетевых вызовов', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: true });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_delete'), testCtx.ctx, {
    id: 'p2',
    confirm: true,
  });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.required, 'reason');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_profile_delete без confirm → confirmation_required без сетевых вызовов', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: true });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_delete'), testCtx.ctx, {
    id: 'p2',
    reason: 'self-test cleanup',
  });

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'confirmation_required');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_profile_delete при allowDestructive:false → disabled_by_policy', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: false });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_delete'), testCtx.ctx, {
    id: 'p2',
    confirm: true,
    reason: 'self-test cleanup',
  });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'disabled_by_policy');
  assert.equal(payload.error.details.flag, 'SSVC_ALLOW_DESTRUCTIVE');
  assert.equal(testCtx.calls.length, 0, 'флаг проверяется до сети');
});

test('oc_profile_delete: успешное удаление — DELETE с телом {id} и проверкой отсутствия', async (t) => {
  const state = initialState();
  let deleteBody: unknown;
  const testCtx = await makeTestContext(
    [
      signInRoute(),
      listRoute(state),
      {
        method: 'DELETE',
        path: '/rest/profiles/delete',
        reply: (call) => {
          deleteBody = call.body;
          const body = call.body as { id: string };
          state.profiles = state.profiles.filter((profile) => profile.id !== body.id);
          return { body: { success: 'Profile deleted' } };
        },
      },
    ],
    { allowDestructive: true },
  );
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profile_delete'), testCtx.ctx, {
    id: 'p2',
    confirm: true,
    reason: 'self-test cleanup',
  });

  assert.equal(result.isError, undefined);
  assert.deepEqual(deleteBody, { id: 'p2' }, 'тело DELETE содержит id');
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.deleted_id, 'p2');
  assert.equal(payload.verified_absent, true, 'отсутствие подтверждено перечитанным списком');
  assert.equal(callsFor(testCtx.calls, 'DELETE', '/rest/profiles/delete').length, 1);
});

test('SPA-заглушка 200 text/html → feature_unsupported, а не пустой список', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/profiles',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SSVC OpenConnect</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_profiles_list'), testCtx.ctx, {});

  assert.equal(result.isError, true, 'SPA-заглушка не должна выглядеть успехом');
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.equal('profiles' in payload, false);
});

test('oc_profile_create: пустое имя и имя длиннее 120 символов → validation_failed без сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const empty = await runTool(tool('oc_profile_create'), testCtx.ctx, { name: '   ' });
  assert.equal(empty.isError, true);
  assert.equal(payloadOf(empty).error.code, 'validation_failed');

  const long = await runTool(tool('oc_profile_create'), testCtx.ctx, {
    name: 'я'.repeat(121),
  });
  assert.equal(long.isError, true);
  const payload = payloadOf(long);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.max_length, 120);

  assert.equal(testCtx.calls.length, 0, 'валидация имени локальная');
});

test('registerProfilesTools: девять инструментов видны в tools/list с нужными аннотациями', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerProfilesTools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp8-profiles-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(
    listed.tools.map((entry) => entry.name).sort(),
    [
      'oc_profile_apply',
      'oc_profile_copy',
      'oc_profile_create',
      'oc_profile_delete',
      'oc_profile_get',
      'oc_profile_rename',
      'oc_profile_save_current',
      'oc_profile_update_content',
      'oc_profiles_list',
    ],
  );

  const list = listed.tools.find((entry) => entry.name === 'oc_profiles_list');
  assert.equal(list?.annotations?.readOnlyHint, true);
  assert.equal(list?.title, 'Список профилей');

  const get = listed.tools.find((entry) => entry.name === 'oc_profile_get');
  assert.equal(get?.annotations?.readOnlyHint, true);

  const apply = listed.tools.find((entry) => entry.name === 'oc_profile_apply');
  assert.equal(apply?.annotations?.destructiveHint, true, 'риск 2 влияет на процесс');
  const applySchema = apply?.inputSchema as { properties?: Record<string, unknown> } | undefined;
  assert.ok(applySchema?.properties?.confirm !== undefined, 'confirm объявлен в схеме');

  const remove = listed.tools.find((entry) => entry.name === 'oc_profile_delete');
  assert.equal(remove?.annotations?.readOnlyHint, false);
  const removeSchema = remove?.inputSchema as { properties?: Record<string, unknown> } | undefined;
  assert.ok(removeSchema?.properties?.reason !== undefined, 'reason объявлен в схеме');
  assert.ok(removeSchema?.properties?.confirm !== undefined, 'confirm объявлен в схеме');

  const create = listed.tools.find((entry) => entry.name === 'oc_profile_create');
  const createSchema = create?.inputSchema as { properties?: Record<string, unknown> } | undefined;
  assert.ok(createSchema?.properties?.name !== undefined);
  assert.ok(createSchema?.properties?.content !== undefined);
});
