/**
 * Тесты WP7: журналы ректификации (mcp-server-agents.md §11).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети. Проверяются ловушки
 * stateful-контракта (mcp-server.md §2.5 п.6):
 *  - `GET /rest/logs` из idle запускает запрос списка, `refresh=1` — принудительный
 *    запрос, `statusOnly=1` — только снимок;
 *  - `409 log_transfer_busy` для списка — не ошибка, а `status:"busy"`;
 *  - скачивание: несколько `202` → `200 CSV`, файл на диске, имя из
 *    `Content-Disposition`, fallback `<id>.csv`, число строк и preview;
 *  - `409 busy`, `503 heap_low`, `503 queue_full` — разные коды;
 *  - таймаут 202 — `device_timeout` с подсказкой про `oc_logs_cancel`
 *    (тест использует уменьшенный бюджет, а не ждёт 90 с);
 *  - отмена `DELETE /rest/logs`;
 *  - санитизация имени: `../../evil.csv` не выходит из `save_dir`;
 *  - CSV с BOM и `\r\n` считается корректно.
 *
 * Файлы пишутся только во временный каталог `makeTestContext`, который удаляется
 * в `cleanup()`; в ответе инструмента нет полного CSV.
 */

import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { relative, resolve } from 'node:path';
import { test } from 'node:test';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { downloadLog, listLogs } from '../src/domain/logs.js';
import { OcError } from '../src/errors.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { logsTools } from '../src/tools/logs.tools.js';
import {
  makeTestContext,
  signInRoute,
  type FakeCall,
  type FakeReply,
  type FakeRoute,
} from './fakeHttp.js';

function tool(name: string): ToolDef {
  const found = logsTools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

function listCalls(calls: FakeCall[]): FakeCall[] {
  return calls.filter((call) => call.method === 'GET' && call.path === '/rest/logs');
}

function transferCalls(calls: FakeCall[], id: number): FakeCall[] {
  return calls.filter((call) => call.method === 'GET' && call.path === `/rest/logs/${id}`);
}

const LIST_FIXTURE = {
  status: 'list',
  total: 0,
  received: 0,
  files: ['12.CSV', '13.CSV'],
};

/** Длинный заголовок проверяет усечение preview до 200 символов. */
const CSV_ROWS = [
  `ts,${'x'.repeat(260)}`,
  ...Array.from({ length: 23 }, (_, index) => `2026-01-01T00:00:${String(index).padStart(2, '0')},${index}`),
  'UNIQUE_CSV_MARKER,999',
];
const CSV_BODY = `\uFEFF${CSV_ROWS.join('\r\n')}\r\n`;

/** Маршрут скачивания: три 202, затем 200 CSV с именем из Content-Disposition. */
function csvDownloadRoute(
  id: number,
  body: string,
  opts: { fileName?: string | null; contentDisposition?: string | null } = {},
): { route: FakeRoute; calls: () => number } {
  let count = 0;
  const route: FakeRoute = {
    method: 'GET',
    path: new RegExp(`^/rest/logs/${id}$`),
    reply: () => {
      count += 1;
      if (count <= 3) {
        return { status: 202, body: { status: 'receiving', total: 120, received: count * 40 } };
      }
      const headers: Record<string, string> = { 'content-type': 'text/csv; charset=utf-8' };
      const disposition =
        opts.contentDisposition === undefined
          ? opts.fileName === undefined
            ? `attachment; filename="${id}.CSV"`
            : opts.fileName === null
              ? null
              : `attachment; filename="${opts.fileName}"`
          : opts.contentDisposition;
      if (disposition !== null) headers['content-disposition'] = disposition;
      return { status: 200, headers, body };
    },
  };
  return { route, calls: () => count };
}

function constantRoute(reply: FakeReply): FakeRoute {
  return { method: 'GET', path: '/rest/logs', reply: () => reply };
}

function downloadErrorRoute(status: number, body: unknown, path = /^\/rest\/logs\/12$/): FakeRoute {
  return { method: 'GET', path, reply: () => ({ status, body }) };
}

test('logsTools: три инструмента WP7 с аннотациями риска', () => {
  const tools = logsTools();
  assert.deepEqual(
    tools.map((candidate) => candidate.name),
    ['oc_logs_list', 'oc_logs_download', 'oc_logs_cancel'],
  );
  assert.equal(tool('oc_logs_list').annotations.readOnlyHint, true);
  assert.equal(tool('oc_logs_download').annotations.readOnlyHint, true);
  assert.equal(tool('oc_logs_cancel').annotations.readOnlyHint, false);
  assert.equal(tool('oc_logs_cancel').annotations.destructiveHint, false);
});

test('oc_logs_list: из idle отдаёт список и не шлёт refresh/statusOnly', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), constantRoute({ body: LIST_FIXTURE })]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_logs_list'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'чтение списка не должно быть ошибкой');
  const payload = payloadOf(result);

  assert.equal(payload.status, 'list');
  assert.deepEqual(payload.files, ['12.CSV', '13.CSV']);
  assert.equal(payload.total, 0);
  assert.equal(payload.received, 0);
  assert.equal(typeof payload.observed_at, 'string');

  const calls = listCalls(testCtx.calls);
  assert.equal(calls.length, 1, 'один GET /rest/logs на вызов');
  assert.deepEqual(calls[0].query, {}, 'без параметров контроллер сам запрашивает список из idle');
});

test('oc_logs_list: refresh:true шлёт ?refresh=1', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), constantRoute({ body: LIST_FIXTURE })]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_logs_list'), testCtx.ctx, { refresh: true });
  assert.equal(result.isError, undefined);
  assert.deepEqual(payloadOf(result).files, ['12.CSV', '13.CSV']);

  const calls = listCalls(testCtx.calls);
  assert.equal(calls.length, 1);
  assert.equal(calls[0].query.refresh, '1', 'refresh:true обязан уйти в query');
  assert.equal('statusOnly' in calls[0].query, false);
});

test('listLogs statusOnly: только снимок, без refresh даже при обоих флагах', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    constantRoute({ body: { status: 'receiving', total: 0, received: 0 } }),
  ]);
  t.after(() => testCtx.cleanup());

  const first = await listLogs(testCtx.ctx.client, { statusOnly: true });
  assert.equal(first.status, 'receiving');

  const second = await listLogs(testCtx.ctx.client, { statusOnly: true, refresh: true });
  assert.equal(second.status, 'receiving');

  const calls = listCalls(testCtx.calls);
  assert.equal(calls.length, 2);
  assert.deepEqual(calls[0].query, { statusOnly: '1' });
  assert.deepEqual(calls[1].query, { statusOnly: '1' }, 'statusOnly сильнее refresh');
});

test('oc_logs_list: 409 log_transfer_busy — успех со status:"busy"', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    constantRoute({ status: 409, body: { error: 'log_transfer_busy' } }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_logs_list'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'busy для списка — не ошибка инструмента');
  const payload = payloadOf(result);

  assert.equal(payload.status, 'busy');
  assert.deepEqual(payload.files, []);
  assert.equal(payload.total, 0);
  assert.equal(payload.received, 0);
  assert.equal(payload.error, 'log_transfer_busy');
  assert.equal(listCalls(testCtx.calls).length, 1, '409 не ретраится транспортом');
});

test('oc_logs_download: 202 три раза → 200 CSV, файл и preview без полного CSV', async (t) => {
  const download = csvDownloadRoute(12, CSV_BODY);
  const testCtx = await makeTestContext([signInRoute(), download.route]);
  t.after(() => testCtx.cleanup());

  const opened = await downloadLog(testCtx.ctx.client, 12, {
    baseDir: testCtx.dir,
    saveDir: 'downloads/test',
    pollMs: 1,
  });

  assert.equal(
    opened.saved_path,
    resolve(testCtx.dir, 'downloads', 'test', '12.CSV'),
    'имя файла из Content-Disposition',
  );
  const saved = await readFile(opened.saved_path, 'utf8');
  assert.equal(saved, CSV_BODY, 'в файл попал CSV целиком');
  assert.equal(opened.bytes, Buffer.byteLength(CSV_BODY, 'utf8'));
  assert.equal(opened.lines, 25, '24 строки данных + заголовок + маркер');
  assert.equal(opened.preview.length, 20, 'preview — первые 20 строк');
  assert.equal(opened.preview[0].length, 200, 'длинная строка усечена до 200 символов');
  assert.equal(opened.preview[0].startsWith('\uFEFF'), false, 'BOM в preview не попадает');
  assert.equal(JSON.stringify(opened).includes('UNIQUE_CSV_MARKER'), false, 'полный CSV в ответ не попадает');
  assert.ok(opened.elapsed_s >= 0);

  assert.equal(download.calls(), 4, 'три 202 и финальный 200');
  assert.equal(transferCalls(testCtx.calls, 12).length, 4);
  assert.ok(
    testCtx.ctx.client.getStats().writes.completed >= 1,
    'скачивание прошло через withWrite("logs")',
  );
});

test('oc_logs_download: без Content-Disposition имя — <id>.csv', async (t) => {
  const download = csvDownloadRoute(34, 'a,b\n1,2\n', { fileName: null });
  const withRoute = await makeTestContext([signInRoute(), download.route]);
  t.after(() => withRoute.cleanup());

  const opened = await downloadLog(withRoute.ctx.client, 34, {
    baseDir: withRoute.dir,
    saveDir: 'downloads/fallback',
    pollMs: 1,
  });

  assert.equal(opened.saved_path, resolve(withRoute.dir, 'downloads', 'fallback', '34.csv'));
  assert.equal(opened.lines, 2);
  assert.deepEqual(opened.preview, ['a,b', '1,2']);
});

test('oc_logs_download: 409 log_transfer_busy → busy (retryable) без ретраев', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    downloadErrorRoute(409, { error: 'log_transfer_busy' }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_logs_download'), testCtx.ctx, { process_id: 12 });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.ok, false);
  assert.equal(payload.error.code, 'busy');
  assert.equal(payload.error.retryable, true);
  assert.match(payload.error.message, /Передача журнала уже идёт/);
  assert.match(payload.error.hint, /oc_logs_cancel/);
  assert.equal(transferCalls(testCtx.calls, 12).length, 1, '409 не ретраится');
});

test('oc_logs_download: 503 log_heap_low → busy «недостаточно памяти»', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    downloadErrorRoute(503, { error: 'log_heap_low' }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_logs_download'), testCtx.ctx, { process_id: 12 });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'busy');
  assert.equal(payload.error.retryable, true);
  assert.match(payload.error.message, /недостаточно памяти/);
  assert.equal(payload.error.details.error, 'log_heap_low');
});

test('oc_logs_download: 503 queue_full → queue_full', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    downloadErrorRoute(503, { error: 'queue_full' }),
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_logs_download'), testCtx.ctx, { process_id: 12 });
  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'queue_full');
  assert.equal(payload.error.retryable, true);
});

test('oc_logs_download: таймаут 202 → device_timeout с подсказкой oc_logs_cancel', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    downloadErrorRoute(202, { status: 'receiving', total: 120, received: 40 }),
  ]);
  t.after(() => testCtx.cleanup());

  const startedAt = Date.now();
  await assert.rejects(
    () =>
      downloadLog(testCtx.ctx.client, 12, {
        baseDir: testCtx.dir,
        saveDir: 'downloads/timeout',
        timeoutMs: 120,
        pollMs: 5,
      }),
    (error: unknown) => {
      assert.ok(error instanceof OcError, 'ожидался OcError');
      assert.equal(error.code, 'device_timeout');
      assert.equal(error.retryable, true);
      assert.match(error.hint ?? '', /oc_logs_cancel/);
      assert.equal(error.details?.process_id, 12);
      return true;
    },
  );

  const elapsed = Date.now() - startedAt;
  assert.ok(elapsed < 5_000, `таймаут не должен ждать 90 с (фактически ${elapsed} мс)`);
  assert.ok(transferCalls(testCtx.calls, 12).length >= 2, 'было несколько повторов 202');
});

test('oc_logs_download: 202 status:error err_mnt → device_error без ожидания 90 с', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    downloadErrorRoute(202, { status: 'error', error: 'err_mnt' }),
  ]);
  t.after(() => testCtx.cleanup());

  const startedAt = Date.now();
  const result = await runTool(tool('oc_logs_download'), testCtx.ctx, { process_id: 12 });
  const elapsed = Date.now() - startedAt;

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'device_error');
  assert.match(payload.error.message, /err_mnt/);
  assert.match(payload.error.hint, /SD-карт/);
  assert.equal(payload.error.details.device_error, 'err_mnt');
  assert.ok(elapsed < 5_000, `терминальный 202 не должен ждать таймаут (${elapsed} мс)`);
  assert.equal(transferCalls(testCtx.calls, 12).length, 1, 'терминальный 202 не повторяется');
});

test('oc_logs_cancel: DELETE /rest/logs возвращает status', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    { method: 'DELETE', path: '/rest/logs', reply: () => ({ body: { status: 'idle' } }) },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_logs_cancel'), testCtx.ctx, {});
  assert.equal(result.isError, undefined, 'отмена — не ошибка');
  const payload = payloadOf(result);
  assert.equal(payload.status, 'idle');
  assert.equal(typeof payload.observed_at, 'string');

  const deletes = testCtx.calls.filter(
    (call) => call.method === 'DELETE' && call.path === '/rest/logs',
  );
  assert.equal(deletes.length, 1, 'ровно один DELETE /rest/logs');
});

test('oc_logs_download: имя из Content-Disposition санитизируется внутри save_dir', async (t) => {
  const download = csvDownloadRoute(12, 'a,b\r\n1,2\r\n', { fileName: '../../evil.csv' });
  const withRoute = await makeTestContext([signInRoute(), download.route]);
  t.after(() => withRoute.cleanup());

  const opened = await downloadLog(withRoute.ctx.client, 12, {
    baseDir: withRoute.dir,
    saveDir: 'downloads/sanitize',
    pollMs: 1,
  });

  const saveDir = resolve(withRoute.dir, 'downloads', 'sanitize');
  assert.equal(opened.saved_path, resolve(saveDir, 'evil.csv'));
  assert.equal(
    relative(saveDir, opened.saved_path),
    'evil.csv',
    'путь остаётся внутри save_dir',
  );
  assert.equal(opened.saved_path.includes('..'), false, 'никаких .. в итоговом пути');
  assert.equal(await readFile(opened.saved_path, 'utf8'), 'a,b\r\n1,2\r\n');
});

test('oc_logs_download: save_dir вне рабочего каталога отклоняется до сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  await assert.rejects(
    () =>
      downloadLog(testCtx.ctx.client, 12, {
        baseDir: testCtx.dir,
        saveDir: '../outside',
      }),
    (error: unknown) => {
      assert.ok(error instanceof OcError);
      assert.equal(error.code, 'validation_failed');
      assert.match(error.message, /выходит за пределы/);
      return true;
    },
  );
  assert.equal(transferCalls(testCtx.calls, 12).length, 0, 'до сети дело не дошло');
});

test('oc_logs_download: BOM и \\r\\n не ломают число строк и preview', async (t) => {
  const body = '\uFEFFa,b\r\n1,2\r\n3,4\r\n';
  const download = csvDownloadRoute(12, body, { fileName: '12.CSV' });
  const withRoute = await makeTestContext([signInRoute(), download.route]);
  t.after(() => withRoute.cleanup());

  const opened = await downloadLog(withRoute.ctx.client, 12, {
    baseDir: withRoute.dir,
    saveDir: 'downloads/bom',
    pollMs: 1,
  });

  assert.equal(opened.lines, 3, 'BOM не создаёт лишнюю строку, завершающий перевод — тоже');
  assert.deepEqual(opened.preview, ['a,b', '1,2', '3,4']);
  assert.equal(opened.bytes, Buffer.byteLength(body, 'utf8'));
  assert.equal(await readFile(opened.saved_path, 'utf8'), body, 'BOM сохраняется в файле');
});

test('oc_logs_download: неверный process_id отклоняется до сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  await assert.rejects(
    () => downloadLog(testCtx.ctx.client, 0, { baseDir: testCtx.dir }),
    (error: unknown) => {
      assert.ok(error instanceof OcError);
      assert.equal(error.code, 'validation_failed');
      return true;
    },
  );
  assert.equal(testCtx.calls.filter((call) => call.path.startsWith('/rest/logs')).length, 0);
});
