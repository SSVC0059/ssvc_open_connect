/**
 * Тесты WP2: команды, журнал попыток, ожидание изменения и предохранители
 * (mcp-server-agents.md §6 + «Обновление после §9»).
 *
 * Проверяем контракт прошивки 0.2.9.2:
 *  - 200 JSON accepted + commandId + warnings; 200 text/plain старой прошивки — accepted без id;
 *  - 409 JSON blockReasons → guard_blocked; 409 text → feature_unsupported;
 *  - 503 → queue_full; 501 → unsupported_operation (not_implemented);
 *  - журнал: queued → running → done, 404 → state:"unknown" (запись вытеснена);
 *  - oc_wait_for_change: совпадение, таймаут, потеря UART, пустой expect;
 *  - oc_send_command не пропускает неизвестные/процессные команды без сетевого вызова;
 *  - oc_start без confirm → confirmation_required без сетевого вызова;
 *  - SPA-заглушка 200 text/html — не успех;
 *  - oc_emergency_stop без confirm работает (защитное действие).
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import {
  getCommandStatus,
  waitForCommandTerminal,
} from '../src/domain/commands.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { commandsTools } from '../src/tools/commands.tools.js';
import { makeTestContext, signInRoute, type FakeRoute } from './fakeHttp.js';

const SNAPSHOT = {
  observed_at: 827_618,
  telemetry: {
    type: 'waiting',
    stop: false,
    stops: 0,
    common: { mmhg: '765.7', tp1: 27.19 },
  },
  status: {
    stage: 'waiting',
    status: 'idle',
    startTime: '',
    endTime: '',
    stages: { waiting: 'running' },
    uartConnectionError: false,
  },
  lastUpdate: 825_908,
  partial: [] as string[],
};

function tool(name: string): ToolDef {
  const found = commandsTools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

function postCommandRoute(reply: () => { status?: number; body?: unknown; headers?: Record<string, string> }): FakeRoute {
  return { method: 'POST', path: '/rest/commands', reply };
}

function journalRoute(reply: (id: string) => { status?: number; body?: unknown }): FakeRoute {
  return {
    method: 'GET',
    path: /^\/rest\/commands\/\d+$/,
    reply: (call) => reply(call.path.split('/').pop() ?? ''),
  };
}

function snapshotRoute(reply: () => unknown): FakeRoute {
  return { method: 'GET', path: '/rest/oc/snapshot', reply: () => ({ body: reply() }) };
}

function okAccepted(command: string, commandId: number | string | null, warnings: string[] = []) {
  return {
    status: 'accepted',
    command,
    ...(commandId === null ? {} : { commandId }),
    ...(warnings.length === 0 ? {} : { warnings }),
  };
}

test('oc_start: accepted с command_id и warnings, POST ровно один', async (t) => {
  const warning = 'Датчик уровня receiver_tank_1 не откалиброван: работа без предохранителя';
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 200, body: okAccepted('start', 12, [warning]) })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_start'), testCtx.ctx, { confirm: true });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.accepted, true);
  assert.equal(payload.command, 'start');
  assert.equal(payload.command_id, 12);
  assert.deepEqual(payload.warnings, [warning]);
  assert.equal(payload.confidence, 'transport');
  assert.match(String(payload.transport_note), /транспортн/i);

  const posts = testCtx.calls.filter(
    (call) => call.method === 'POST' && call.path === '/rest/commands',
  );
  assert.equal(posts.length, 1);
  assert.deepEqual(posts[0].body, { commands: 'start', parameters: '' });
  // ESPAsyncWebServer не разбирает chunked-body: POST обязан уходить с Content-Length.
  assert.equal(
    posts[0].headers['content-length'],
    String(Buffer.byteLength(JSON.stringify({ commands: 'start', parameters: '' }), 'utf8')),
  );
});

test('oc_start: предупреждения ≠ отказ, пустой warnings остаётся массивом', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 200, body: okAccepted('start', 3) })),
  ]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(await runTool(tool('oc_start'), testCtx.ctx, { confirm: true }));
  assert.deepEqual(payload.warnings, []);
  assert.equal(payload.command_id, 3);
});

test('oc_start: 409 JSON blockReasons → guard_blocked с причинами и подсказкой про XKC-25', async (t) => {
  const reasons = [
    'Датчик уровня receiver_tank_1 не откалиброван',
    'Старт заблокирован: не выполнена проверка без жидкости',
  ];
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({
      status: 409,
      body: { status: 'rejected', command: 'start', blockReasons: reasons },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_start'), testCtx.ctx, { confirm: true });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.ok, false);
  assert.equal(payload.error.code, 'guard_blocked');
  assert.equal(payload.error.retryable, false);
  assert.deepEqual(payload.error.details.block_reasons, reasons);
  assert.match(String(payload.error.hint), /XKC-25/);
  assert.match(String(payload.error.hint), /калибр/i);
  assert.equal(
    testCtx.calls.filter((call) => call.method === 'POST' && call.path === '/rest/commands').length,
    1,
  );
});

test('oc_start: 409 text "Command unsupported by device API" → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 409, body: 'Command unsupported by device API' })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_start'), testCtx.ctx, { confirm: true });
  const payload = payloadOf(result);
  assert.equal(result.isError, true);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.equal(payload.error.retryable, false);
  assert.match(String(payload.error.message), /не поддерживается/i);
});

test('oc_start: 503 "Command queue is full" → queue_full, POST не ретраится', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 503, body: 'Command queue is full' })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_start'), testCtx.ctx, { confirm: true });
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'queue_full');
  assert.equal(payload.error.retryable, true);
  assert.equal(
    testCtx.calls.filter((call) => call.method === 'POST' && call.path === '/rest/commands').length,
    1,
    'POST не повторяется внутри инструмента: решает клиент',
  );
});

test('oc_start: 501 → unsupported_operation (not_implemented)', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 501, body: 'Command not implemented' })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_start'), testCtx.ctx, { confirm: true });
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'unsupported_operation');
  assert.equal(payload.error.details.reason, 'not_implemented');
  assert.equal(payload.error.retryable, false);
});

test('oc_start: 200 text/plain старой прошивки — accepted без command_id, raw сохраняется', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 200, body: 'Command accepted' })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_start'), testCtx.ctx, { confirm: true });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.accepted, true);
  assert.equal(payload.command_id, null);
  assert.equal(payload.raw, 'Command accepted');
  assert.match(String(payload.command_id_note), /без commandId/);
});

test('oc_start: SPA-заглушка 200 text/html — не успех, feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({
      status: 200,
      body: '<!DOCTYPE html><html><body>OpenConnect SPA</body></html>',
      headers: { 'content-type': 'text/html; charset=utf-8' },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_start'), testCtx.ctx, { confirm: true });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.ok, false);
  assert.equal(payload.error.code, 'feature_unsupported');
  assert.equal(payload.error.details.reason, 'spa_html_response');
});

test('oc_start без confirm → confirmation_required и ни одного сетевого вызова', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_start'), testCtx.ctx, {});
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'confirmation_required');
  assert.equal(testCtx.calls.length, 0, 'до устройства запрос не доходит');
});

test('oc_emergency_stop: без confirm принимается (защитное действие)', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 200, body: okAccepted('emergency_stop', 13) })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_emergency_stop'), testCtx.ctx, {});
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.command, 'emergency_stop');
  assert.equal(payload.command_id, 13);
  const posts = testCtx.calls.filter(
    (call) => call.method === 'POST' && call.path === '/rest/commands',
  );
  assert.equal(posts.length, 1);
  assert.deepEqual(posts[0].body, { commands: 'emergency_stop', parameters: '' });
});

test('getCommandStatus/waitForCommandTerminal: queued → running → done', async (t) => {
  const states = ['queued', 'running', 'done'];
  let index = 0;
  const testCtx = await makeTestContext([
    signInRoute(),
    journalRoute(() => {
      const state = states[Math.min(index, states.length - 1)];
      index += 1;
      const finished = state === 'done';
      return {
        status: 200,
        body: {
          id: 7,
          command: 'status',
          state,
          queued_at: 100,
          started_at: state === 'queued' ? null : 110,
          finished_at: finished ? 120 : null,
          ok: finished ? true : null,
          reason: null,
          confidence: 'transport',
        },
      };
    }),
    postCommandRoute(() => ({ status: 200, body: okAccepted('status', 7) })),
  ]);
  t.after(() => testCtx.cleanup());

  const wait = await waitForCommandTerminal(testCtx.ctx.client, 7, {
    pollMs: 1,
    timeoutMs: 1_000,
  });
  assert.equal(wait.state, 'done');
  assert.equal(wait.terminal, true);
  assert.equal(wait.timed_out, false);
  assert.equal(wait.polls, 3);
  assert.equal(wait.ok, true);
  assert.equal(wait.confidence, 'transport');
  assert.equal(wait.finished_at, 120);
});

test('getCommandStatus: 404 журнала → state unknown (запись вытеснена), не ошибка', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    journalRoute(() => ({ status: 404, body: { error: 'not_found' } })),
  ]);
  t.after(() => testCtx.cleanup());

  const view = await getCommandStatus(testCtx.ctx.client, 7);
  assert.equal(view.state, 'unknown');
  assert.equal(view.evicted, true);
  assert.equal(view.terminal, true);
  assert.equal(view.command_id, 7);
  assert.match(String(view.note), /вытеснена/i);
});

test('waitForCommandTerminal: sent не финал для команд с ответом — ждёт done', async (t) => {
  const states = ['sent', 'done'];
  let index = 0;
  const testCtx = await makeTestContext([
    signInRoute(),
    journalRoute(() => {
      const state = states[Math.min(index, states.length - 1)];
      index += 1;
      return {
        status: 200,
        body: {
          id: 24,
          command: 'status',
          state,
          queued_at: 100,
          started_at: 101,
          finished_at: state === 'done' ? 130 : null,
          ok: state === 'done' ? true : null,
          reason: null,
          confidence: 'transport',
        },
      };
    }),
  ]);
  t.after(() => testCtx.cleanup());

  const wait = await waitForCommandTerminal(testCtx.ctx.client, 24, {
    pollMs: 1,
    timeoutMs: 1_000,
  });
  assert.equal(wait.state, 'done', 'sent — «ушло в UART», ответа ещё нет; ждём done');
  assert.equal(wait.terminal, true);
  assert.equal(wait.timed_out, false);
  assert.equal(wait.polls, 2);
  assert.equal(wait.ok, true);
});

test('waitForCommandTerminal: sent так и не стал done → timed_out, состояние sent', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    journalRoute(() => ({
      status: 200,
      body: {
        id: 25,
        command: 'status',
        state: 'sent',
        queued_at: 100,
        started_at: 101,
        finished_at: null,
        ok: null,
        reason: null,
        confidence: 'transport',
      },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const wait = await waitForCommandTerminal(testCtx.ctx.client, 25, {
    pollMs: 5,
    timeoutMs: 30,
  });
  assert.equal(wait.state, 'sent');
  assert.equal(wait.timed_out, true);
  assert.ok(wait.polls >= 1);
});

test('oc_send_command status: accepted, журнал опрошен до done', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 200, body: okAccepted('status', 9) })),
    journalRoute(() => ({
      status: 200,
      body: {
        id: 9,
        command: 'status',
        state: 'done',
        queued_at: 10,
        started_at: 11,
        finished_at: 12,
        ok: true,
        reason: null,
        confidence: 'transport',
      },
    })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_send_command'), testCtx.ctx, {
    command: 'status',
    confirm: true,
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.accepted, true);
  assert.equal(payload.command_id, 9);
  assert.equal(payload.reply_expected, true);
  assert.equal(payload.journal.state, 'done');
  assert.equal(payload.journal.ok, true);
});

test('oc_send_command status: 404 журнала → ok:true, state unknown', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    postCommandRoute(() => ({ status: 200, body: okAccepted('status', 10) })),
    journalRoute(() => ({ status: 404, body: { error: 'not_found' } })),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_send_command'), testCtx.ctx, {
    command: 'status',
    confirm: true,
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.journal.state, 'unknown');
  assert.equal(payload.journal.evicted, true);
});

test('oc_send_command: неизвестное имя команды отвергается без сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_send_command'), testCtx.ctx, {
    command: 'reboot',
    confirm: true,
  });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.reason, 'invalid_argument');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_send_command: команда процесса (start) сюда не входит', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_send_command'), testCtx.ctx, {
    command: 'start',
    confirm: true,
  });
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.reason, 'invalid_argument');
  assert.ok(payload.error.details.allowed.includes('status'));
  assert.equal(testCtx.calls.length, 0);
});

test('oc_send_command: parameters длиннее 300 символов — invalid_argument без сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_send_command'), testCtx.ctx, {
    command: 'set',
    parameters: 'x'.repeat(301),
    confirm: true,
  });
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.reason, 'invalid_argument');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_wait_for_change: matched на смене stage (второй опрос)', async (t) => {
  let polls = 0;
  const testCtx = await makeTestContext([
    signInRoute(),
    snapshotRoute(() => {
      polls += 1;
      if (polls === 1) return SNAPSHOT;
      return {
        ...SNAPSHOT,
        telemetry: { ...SNAPSHOT.telemetry, type: 'heads' },
        status: { ...SNAPSHOT.status, stage: 'heads', status: 'running' },
        lastUpdate: 826_500,
        observed_at: 827_700,
      };
    }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_wait_for_change'), testCtx.ctx, {
    expect: { stage: ['tp1_waiting', 'heads'] },
    timeout_s: 2,
    poll_ms: 250,
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.matched, true);
  assert.equal(payload.timeout, false);
  assert.equal(payload.observed.stage, 'heads');
  assert.equal(payload.observed.status, 'running');
  assert.equal(payload.observed.telemetry_type, 'heads');
  assert.deepEqual(payload.matched_fields, ['stage']);
  assert.ok(payload.polls >= 2);
});

test('oc_wait_for_change: таймаут без изменения — не ошибка, matched:false timeout:true', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), snapshotRoute(() => SNAPSHOT)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_wait_for_change'), testCtx.ctx, {
    expect: { stage: 'heads' },
    timeout_s: 1,
    poll_ms: 250,
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.matched, false);
  assert.equal(payload.timeout, true);
  assert.equal(payload.observed.stage, 'waiting');
  assert.match(String(payload.notice), /не совпали/i);
  assert.ok(payload.elapsed_s >= 1);
});

test('oc_wait_for_change: uart_error:true → matched:false с явной пометкой', async (t) => {
  const uartBroken = {
    ...SNAPSHOT,
    status: { ...SNAPSHOT.status, uartConnectionError: true },
  };
  const testCtx = await makeTestContext([signInRoute(), snapshotRoute(() => uartBroken)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_wait_for_change'), testCtx.ctx, {
    expect: { stage: 'heads' },
    timeout_s: 5,
    poll_ms: 250,
  });
  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.matched, false);
  assert.equal(payload.timeout, false);
  assert.equal(payload.uart_error, true);
  assert.equal(payload.observed.uart_error, true);
  assert.equal(payload.polls, 1, 'при потере UART ждать дальше бессмысленно');
  assert.match(String(payload.notice), /UART/);
});

test('oc_wait_for_change: пустой expect → validation_failed без чтения устройства', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_wait_for_change'), testCtx.ctx, {
    expect: {},
    timeout_s: 1,
    poll_ms: 250,
  });
  const payload = payloadOf(result);
  assert.equal(result.isError, true);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.reason, 'invalid_argument');
  assert.equal(testCtx.calls.length, 0);
});
