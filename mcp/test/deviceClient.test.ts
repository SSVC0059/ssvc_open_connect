/**
 * Тесты `DeviceClient`/`AuthSession` (таблица §4 WP0).
 *
 * Всё через `fakeHttp` — реальной сети нет. Проверяем гарантии контракта §2.4:
 * вход и кэш JWT, повтор после 401, security off по 404, ретраи только для GET,
 * разбор 409 blockReasons и 503 queue_full, сериализацию записей.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { z } from 'zod';
import { DeviceClient } from '../src/device/DeviceClient.js';
import { ocInfoSchema } from '../src/device/schemas/info.js';
import { OcError } from '../src/errors.js';
import { silentLogger } from '../src/logging.js';
import { fakeHttp, signInRoute, testConfig, TEST_JWT, type FakeRoute } from './fakeHttp.js';

const INFO_FIXTURE = {
  ssvc: { version: '2.5.1', api: '1.10', api_code: 110, mode: 'late_heads' },
  oc: {
    version: '0.2.9.1',
    is_support_api: true,
    is_support_release: true,
    api_min: '1.7',
    api_target: '1.10',
    api_compatibility: 'supported',
    api_features: [{ name: 'get_log', min_api: '1.10', available: true }],
  },
};

function makeClient(routes: FakeRoute[]) {
  const fake = fakeHttp(routes);
  const client = new DeviceClient({
    cfg: testConfig(),
    http: fake.http,
    log: silentLogger(),
    // В тестах backoff нулевой: проверяем поведение, а не выдержку.
    retry: { attempts: 3, baseDelayMs: 0, maxDelayMs: 0 },
  });
  return { client, calls: fake.calls };
}

function getCalls(calls: ReturnType<typeof fakeHttp>['calls'], path: string) {
  return calls.filter((call) => call.method === 'GET' && call.path === path);
}

function assertOcError(error: unknown, code: string): OcError {
  assert.ok(error instanceof OcError, `ожидался OcError, получено: ${String(error)}`);
  assert.equal(error.code, code);
  return error;
}

test('signIn выполняется один раз, JWT кэшируется и уходит в Authorization', async () => {
  const { client, calls } = makeClient([
    signInRoute(),
    { method: 'GET', path: '/rest/oc/info', reply: () => ({ status: 200, body: INFO_FIXTURE }) },
  ]);

  const first = await client.get('/rest/oc/info', ocInfoSchema);
  const second = await client.get('/rest/oc/info', ocInfoSchema);

  assert.equal(first.ssvc.version, '2.5.1');
  assert.equal(second.ssvc.api, '1.10');
  assert.equal(calls.filter((call) => call.path === '/rest/signIn').length, 1, 'вход должен быть один');
  const infoCalls = getCalls(calls, '/rest/oc/info');
  assert.equal(infoCalls.length, 2);
  for (const call of infoCalls) {
    assert.equal(call.headers.authorization, `Bearer ${TEST_JWT}`);
  }
  assert.equal(client.auth.stats().signIns, 1);
});

test('401 приводит к повторному входу и одному повтору запроса', async () => {
  let infoAttempts = 0;
  let signIns = 0;
  const { client, calls } = makeClient([
    {
      method: 'POST',
      path: '/rest/signIn',
      reply: () => {
        signIns += 1;
        return { status: 200, body: { access_token: signIns === 1 ? 'jwt-first' : 'jwt-second' } };
      },
    },
    {
      method: 'GET',
      path: '/rest/verifyAuthorization',
      reply: () => ({ status: 401 }),
    },
    {
      method: 'GET',
      path: '/rest/oc/info',
      reply: () => {
        infoAttempts += 1;
        return infoAttempts === 1
          ? { status: 401, body: { error: 'unauthorized' } }
          : { status: 200, body: INFO_FIXTURE };
      },
    },
  ]);

  const info = await client.get('/rest/oc/info', ocInfoSchema);

  assert.equal(info.ssvc.version, '2.5.1');
  assert.equal(infoAttempts, 2, 'первый 401 и один повтор');
  assert.equal(signIns, 2, 'повторный вход после проверки токена');
  assert.equal(calls.filter((call) => call.path === '/rest/verifyAuthorization').length, 1);
  const infoCalls = getCalls(calls, '/rest/oc/info');
  assert.equal(infoCalls[0]?.headers.authorization, 'Bearer jwt-first');
  assert.equal(infoCalls[1]?.headers.authorization, 'Bearer jwt-second');
});

test('404 на signIn = security выключена: запросы идут без Authorization', async () => {
  const { client, calls } = makeClient([
    { method: 'POST', path: '/rest/signIn', reply: () => ({ status: 404, body: '' }) },
    { method: 'GET', path: '/rest/oc/info', reply: () => ({ status: 200, body: INFO_FIXTURE }) },
  ]);

  const info = await client.get('/rest/oc/info', ocInfoSchema);

  assert.equal(info.oc.version, '0.2.9.1');
  assert.equal(client.auth.isSecurityOff, true);
  assert.equal(client.auth.cachedToken, null);
  const infoCalls = getCalls(calls, '/rest/oc/info');
  assert.equal(infoCalls.length, 1);
  assert.equal(infoCalls[0]?.headers.authorization, undefined);
});

test('404 на verifyAuthorization = security выключена', async () => {
  const { client } = makeClient([
    signInRoute(),
    { method: 'GET', path: '/rest/verifyAuthorization', reply: () => ({ status: 404, body: '' }) },
  ]);

  await client.auth.signIn();
  const verdict = await client.auth.verify();

  assert.equal(verdict.status, 'security_off');
  assert.equal(verdict.httpStatus, 404);
  assert.equal(client.auth.isSecurityOff, true);
  assert.equal(client.auth.cachedToken, null);
});

test('ретраи только для GET: 503 на GET повторяется, на PUT — нет', async () => {
  let telemetryAttempts = 0;
  const { client, calls } = makeClient([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/telemetry',
      reply: () => {
        telemetryAttempts += 1;
        return telemetryAttempts < 3
          ? { status: 503, body: { error: 'busy' } }
          : { status: 200, body: { telemetry: {}, status: {} } };
      },
    },
    { method: 'PUT', path: '/rest/settings', reply: () => ({ status: 503, body: { error: 'busy' } }) },
  ]);

  const snapshot = await client.get('/rest/telemetry', z.any());
  assert.deepEqual(snapshot, { telemetry: {}, status: {} });
  assert.equal(telemetryAttempts, 3, 'GET повторяется до трёх попыток');

  const putError = await client
    .put('/rest/settings', { hyst: 0.5 }, z.any())
    .then(() => null)
    .catch((error: unknown) => error);
  assertOcError(putError, 'busy');
  assert.equal(
    calls.filter((call) => call.method === 'PUT').length,
    1,
    'PUT не повторяется: тело могло уже примениться',
  );
});

test('409 blockReasons превращается в guard_blocked с причинами', async () => {
  const { client } = makeClient([
    signInRoute(),
    {
      method: 'POST',
      path: '/rest/commands',
      reply: () => ({
        status: 409,
        body: { status: 'rejected', blockReasons: ['tank_not_full', 'xkc25_not_calibrated'] },
      }),
    },
  ]);

  const error = await client
    .sendCommand('start')
    .then(() => null)
    .catch((caught: unknown) => caught);

  const ocError = assertOcError(error, 'guard_blocked');
  assert.deepEqual(ocError.details?.blockReasons, ['tank_not_full', 'xkc25_not_calibrated']);
  assert.match(String(ocError.hint), /XKC-25/);
});

test('503 queue_full на команде = queue_full и без повтора', async () => {
  const { client, calls } = makeClient([
    signInRoute(),
    {
      method: 'POST',
      path: '/rest/commands',
      reply: () => ({ status: 503, body: 'queue full: too many commands' }),
    },
  ]);

  const error = await client
    .sendCommand('stop')
    .then(() => null)
    .catch((caught: unknown) => caught);

  assertOcError(error, 'queue_full');
  assert.equal(calls.filter((call) => call.method === 'POST' && call.path === '/rest/commands').length, 1);
});

test('withWrite сериализует записи: одновременно выполняется не больше одной', async () => {
  const { client } = makeClient([]);
  const order: string[] = [];
  let active = 0;
  let maxActive = 0;

  const write = (name: string, delayMs: number): Promise<string> =>
    client.withWrite(name, async () => {
      active += 1;
      maxActive = Math.max(maxActive, active);
      order.push(`start:${name}`);
      await new Promise((resolve) => setTimeout(resolve, delayMs));
      order.push(`end:${name}`);
      active -= 1;
      return name;
    });

  const results = await Promise.all([write('a', 20), write('b', 1), write('c', 5)]);

  assert.deepEqual(results, ['a', 'b', 'c'], 'результаты сохраняют порядок постановки');
  assert.equal(maxActive, 1, 'две записи не должны выполняться одновременно');
  assert.deepEqual(order, ['start:a', 'end:a', 'start:b', 'end:b', 'start:c', 'end:c']);
  assert.equal(client.getStats().writes.completed, 3);

  // Ошибка одной записи не рвёт очередь для следующих.
  const failing = client.withWrite('fail', async () => {
    throw new Error('write failed');
  });
  await assert.rejects(failing, /write failed/);
  assert.equal(await write('d', 0), 'd');
});
