/**
 * Тесты WP9: файлы контроллера (mcp-server-agents.md §13).
 *
 * Всё — на тестовом двойнике `fakeHttp`, без реальной сети. Проверяются ловушки
 * файлового контракта (mcp-contract-notes.md §B.15, §D.7, §H.1.2):
 *  - `GET /rest/files` — массив дерева, имя узла — только базовое;
 *  - `oc_files_list` с `path` спускается по дереву, файл каталогом не считается;
 *  - скачивание пишет **байты**: бинарный файл не портится UTF-8-декодированием;
 *  - имя сохраняемого файла берётся из пути и санитизируется (`../../evil` → `evil`);
 *  - `..`, относительный путь и `save_dir` вне рабочего каталога отклоняются
 *    до сетевого вызова (`validation_failed`);
 *  - `copy` шлёт ровно `{source, destination}` и принимает 201;
 *  - удаление: reason → confirm → SSVC_ALLOW_DESTRUCTIVE, и только потом DELETE;
 *  - после удаления дерево перечитывается и возвращается `verified_absent`;
 *  - SPA-заглушка `200 text/html` — `feature_unsupported`, а не «сохранённый файл»;
 *  - перекрытый маршрут скачивания (прошивка вернула дерево) — тоже
 *    `feature_unsupported`, а не файл с JSON-деревом внутри.
 */

import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import type { FileTreeEntry } from '../src/device/schemas/files.js';
import { downloadFile, sanitizeFileName } from '../src/domain/files.js';
import { createMcpServer } from '../src/mcp/server.js';
import { runTool } from '../src/mcp/registry.js';
import type { ToolDef } from '../src/mcp/types.js';
import { filesTools, registerFilesTools } from '../src/tools/files.tools.js';
import {
  makeTestContext,
  signInRoute,
  type FakeCall,
  type FakeRoute,
} from './fakeHttp.js';

function tool(name: string): ToolDef {
  const found = filesTools().find((candidate) => candidate.name === name);
  assert.ok(found !== undefined, `инструмент ${name} должен быть в домене`);
  return found;
}

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

function callsFor(calls: FakeCall[], method: string, path: string | RegExp): FakeCall[] {
  return calls.filter(
    (call) =>
      call.method === method && (typeof path === 'string' ? call.path === path : path.test(call.path)),
  );
}

/** Дерево LittleFS из контракта: вложенные каталоги и файлы. */
const TREE: FileTreeEntry[] = [
  {
    name: 'config',
    type: 'directory',
    children: [
      { name: 'app.json', type: 'file', size: 42 },
      { name: 'zones.json', type: 'file', size: 10 },
      {
        name: 'nested',
        type: 'directory',
        children: [{ name: 'deep.txt', type: 'file', size: 3 }],
      },
    ],
  },
  { name: 'log.bin', type: 'file', size: 5 },
];

interface DeviceState {
  tree: FileTreeEntry[];
}

function treeState(): DeviceState {
  // structuredClone: маршруты ниже меняют состояние, фикстура должна быть свежей.
  return { tree: structuredClone(TREE) };
}

function treeRoute(state: DeviceState): FakeRoute {
  return { method: 'GET', path: '/rest/files', reply: () => ({ body: state.tree }) };
}

/** Удаляет узел по полному пути; возвращает true, если файл был. */
function removeFromTree(tree: FileTreeEntry[], path: string): boolean {
  const segments = path.split('/').filter((segment) => segment !== '');
  const name = segments.pop();
  if (name === undefined) return false;

  let entries: FileTreeEntry[] | undefined = tree;
  for (const segment of segments) {
    const next: FileTreeEntry | undefined = entries?.find(
      (entry: FileTreeEntry) => entry.name === segment,
    );
    if (next === undefined) return false;
    entries = next.children;
  }
  if (entries === undefined) return false;
  const index = entries.findIndex((entry) => entry.name === name);
  if (index < 0) return false;
  entries.splice(index, 1);
  return true;
}

/** `fetch` выполняется в рабочем каталоге теста, чтобы не писать в репозиторий. */
async function inTempCwd<T>(dir: string, fn: () => Promise<T>): Promise<T> {
  const previous = process.cwd();
  process.chdir(dir);
  try {
    return await fn();
  } finally {
    process.chdir(previous);
  }
}

const TEXT_FILE = '{"name":"app","version":1}';

/** Маршрут файла: текст, бинарные байты или 404. */
function downloadRoute(): FakeRoute {
  return {
    method: 'GET',
    path: /^\/rest\/files\/.+/,
    reply: (call) => {
      if (call.path.endsWith('/config/app.json')) {
        return {
          status: 200,
          headers: { 'content-type': 'application/json; charset=utf-8' },
          body: TEXT_FILE,
        };
      }
      if (call.path.endsWith('/config/blob.bin')) {
        return {
          status: 200,
          headers: { 'content-type': 'application/octet-stream' },
          bodyBytes: Buffer.from([0x00, 0x01, 0x02, 0x7f, 0x80, 0x9c, 0xff, 0x0a, 0x0d]),
        };
      }
      return {
        status: 404,
        headers: { 'content-type': 'text/plain' },
        body: 'File not found or is a directory',
      };
    },
  };
}

test('filesTools: четыре инструмента WP9 с аннотациями риска', () => {
  const tools = filesTools();
  assert.deepEqual(
    tools.map((candidate) => candidate.name),
    ['oc_files_list', 'oc_file_download', 'oc_file_copy', 'oc_file_delete'],
  );
  assert.equal(tool('oc_files_list').annotations.readOnlyHint, true);
  assert.equal(tool('oc_file_download').annotations.readOnlyHint, true);
  assert.equal(tool('oc_file_copy').annotations.readOnlyHint, false);
  assert.equal(tool('oc_file_copy').annotations.destructiveHint, false, 'риск 1 — обратимая запись');
  assert.equal(tool('oc_file_delete').annotations.readOnlyHint, false);
  assert.equal(tool('oc_file_delete').annotations.destructiveHint, true);
});

test('oc_files_list: вложенное дерево и корень по умолчанию', async (t) => {
  const state = treeState();
  const testCtx = await makeTestContext([signInRoute(), treeRoute(state)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_files_list'), testCtx.ctx, {});

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.path, '/');
  assert.deepEqual(payload.tree, TREE, 'дерево отдаётся как массив, без обёртки');
  assert.deepEqual(payload.tree?.[0]?.children?.[2]?.children?.[0], {
    name: 'deep.txt',
    type: 'file',
    size: 3,
  });
  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/files').length, 1);
});

test('oc_files_list: path сужает дерево до содержимого каталога', async (t) => {
  const state = treeState();
  const testCtx = await makeTestContext([signInRoute(), treeRoute(state)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_files_list'), testCtx.ctx, { path: '/config/nested' });

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.path, '/config/nested');
  assert.deepEqual(payload.tree, [{ name: 'deep.txt', type: 'file', size: 3 }]);
});

test('oc_files_list: пустое дерево — пустой массив, а не ошибка', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    { method: 'GET', path: '/rest/files', reply: () => ({ body: [] }) },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_files_list'), testCtx.ctx, {});

  assert.equal(result.isError, undefined);
  assert.deepEqual(payloadOf(result).tree, []);
});

test('oc_files_list: несуществующий каталог → not_found', async (t) => {
  const state = treeState();
  const testCtx = await makeTestContext([signInRoute(), treeRoute(state)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_files_list'), testCtx.ctx, { path: '/nope' });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'not_found');
  assert.equal(payload.error.details.path, '/nope');
});

test('oc_files_list: путь указывает на файл → validation_failed', async (t) => {
  const state = treeState();
  const testCtx = await makeTestContext([signInRoute(), treeRoute(state)]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_files_list'), testCtx.ctx, {
    path: '/config/app.json',
  });

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'validation_failed');
});

test('oc_files_list: SPA-заглушка 200 text/html → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: '/rest/files',
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SSVC OpenConnect</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_files_list'), testCtx.ctx, {});

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'feature_unsupported');
  assert.equal('tree' in payloadOf(result), false);
});

test('oc_file_download: текстовый файл сохраняется байтами, имя — из пути', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), downloadRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await inTempCwd(testCtx.dir, () =>
    runTool(tool('oc_file_download'), testCtx.ctx, { path: '/config/app.json' }),
  );

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.ok(
    String(payload.saved_path).startsWith(testCtx.dir),
    'файл сохраняется внутри рабочего каталога',
  );
  assert.ok(String(payload.saved_path).endsWith('app.json'), 'имя — последний сегмент пути');
  assert.equal(payload.bytes, Buffer.byteLength(TEXT_FILE, 'utf8'));
  assert.equal(payload.content_type, 'application/json; charset=utf-8');
  assert.equal('content' in payload, false, 'содержимое файла в ответ не попадает');

  const saved = await readFile(payload.saved_path);
  assert.equal(saved.toString('utf8'), TEXT_FILE);
});

test('oc_file_download: бинарный файл пишется байт-в-байт, без порчи UTF-8', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), downloadRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await inTempCwd(testCtx.dir, () =>
    runTool(tool('oc_file_download'), testCtx.ctx, { path: '/config/blob.bin' }),
  );

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  const expected = Buffer.from([0x00, 0x01, 0x02, 0x7f, 0x80, 0x9c, 0xff, 0x0a, 0x0d]);

  assert.equal(payload.bytes, expected.length, 'размер — в байтах, а не в символах');
  assert.equal(payload.content_type, 'application/octet-stream');
  assert.deepEqual(await readFile(payload.saved_path), expected);
});

test('oc_file_download: 404 файла → not_found', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), downloadRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await inTempCwd(testCtx.dir, () =>
    runTool(tool('oc_file_download'), testCtx.ctx, { path: '/config/missing.txt' }),
  );

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'not_found');
  assert.equal(payload.error.details.path, '/config/missing.txt');
});

test('oc_file_download: path traversal и путь без «/» → validation_failed без сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), downloadRoute()]);
  t.after(() => testCtx.cleanup());

  for (const path of ['/../../evil', '/config/../config/app.json', 'config/app.json']) {
    const result = await runTool(tool('oc_file_download'), testCtx.ctx, { path });
    assert.equal(result.isError, true, `путь ${path} должен быть отклонён`);
    const payload = payloadOf(result);
    assert.equal(payload.error.code, 'validation_failed');
  }
  assert.equal(testCtx.calls.length, 0, 'локальная валидация не должна ходить в устройство');
});

test('oc_file_download: save_dir вне рабочего каталога → validation_failed без сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), downloadRoute()]);
  t.after(() => testCtx.cleanup());

  for (const saveDir of ['../outside', 'C:\\outside', 'a/../../outside']) {
    const result = await runTool(tool('oc_file_download'), testCtx.ctx, {
      path: '/config/app.json',
      save_dir: saveDir,
    });
    assert.equal(result.isError, true, `save_dir ${saveDir} должен быть отклонён`);
    assert.equal(payloadOf(result).error.code, 'validation_failed');
  }
  assert.equal(testCtx.calls.length, 0);
});

test('sanitizeFileName: каталоги отбрасываются, имя остаётся внутри save_dir', () => {
  assert.equal(sanitizeFileName('../../evil'), 'evil');
  assert.equal(sanitizeFileName('/config/app.json'), 'app.json');
  assert.equal(sanitizeFileName('..\\..\\evil.txt'), 'evil.txt');
  assert.equal(sanitizeFileName('/config/..'), 'download.bin');
  assert.equal(sanitizeFileName('/config/'), 'download.bin');
});

test('oc_file_download: SPA-заглушка вместо файла → feature_unsupported', async (t) => {
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: /^\/rest\/files\/.+/,
      reply: () => ({
        status: 200,
        headers: { 'content-type': 'text/html; charset=utf-8' },
        body: '<!DOCTYPE html><html><body>SPA</body></html>',
      }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await inTempCwd(testCtx.dir, () =>
    runTool(tool('oc_file_download'), testCtx.ctx, { path: '/config/app.json' }),
  );

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'feature_unsupported');
});

test('oc_file_download: перекрытый маршрут (дерево вместо файла) → feature_unsupported', async (t) => {
  const state = treeState();
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'GET',
      path: /^\/rest\/files\/.+/,
      reply: () => ({ status: 200, body: state.tree }),
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await inTempCwd(testCtx.dir, () =>
    runTool(tool('oc_file_download'), testCtx.ctx, { path: '/config/app.json' }),
  );

  assert.equal(result.isError, true, 'дерево не должно сохраняться как файл');
  assert.equal(payloadOf(result).error.code, 'feature_unsupported');
});

test('downloadFile: path указывает в save_dir, save_dir создаётся при необходимости', async (t) => {
  const testCtx = await makeTestContext([signInRoute(), downloadRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await downloadFile(testCtx.ctx.client, '/config/app.json', {
    saveDir: 'nested/downloads',
    baseDir: testCtx.dir,
  });

  assert.ok(result.saved_path.startsWith(testCtx.dir));
  assert.ok(result.saved_path.endsWith('app.json'));
  assert.equal(result.bytes, Buffer.byteLength(TEXT_FILE, 'utf8'));
});

test('oc_file_copy: тело source/destination и 201 → ok', async (t) => {
  let copyBody: unknown;
  const testCtx = await makeTestContext([
    signInRoute(),
    {
      method: 'POST',
      path: '/rest/files/copy',
      reply: (call) => {
        copyBody = call.body;
        return { status: 201, body: '' };
      },
    },
  ]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_file_copy'), testCtx.ctx, {
    source: '/config/app.json',
    destination: '/config/app-copy.json',
  });

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.source, '/config/app.json');
  assert.equal(payload.destination, '/config/app-copy.json');
  assert.deepEqual(copyBody, {
    source: '/config/app.json',
    destination: '/config/app-copy.json',
  });
  assert.equal(callsFor(testCtx.calls, 'POST', '/rest/files/copy').length, 1);
});

test('oc_file_copy: относительный путь → validation_failed без сети', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_file_copy'), testCtx.ctx, {
    source: 'config/app.json',
    destination: '/config/app-copy.json',
  });

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'validation_failed');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_file_delete без reason → validation_failed без сетевых вызовов', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: true });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_file_delete'), testCtx.ctx, {
    path: '/config/app.json',
    confirm: true,
  });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'validation_failed');
  assert.equal(payload.error.details.required, 'reason');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_file_delete без confirm → confirmation_required без сетевых вызовов', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: true });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_file_delete'), testCtx.ctx, {
    path: '/config/app.json',
    reason: 'self-test cleanup',
  });

  assert.equal(result.isError, true);
  assert.equal(payloadOf(result).error.code, 'confirmation_required');
  assert.equal(testCtx.calls.length, 0);
});

test('oc_file_delete при allowDestructive:false → disabled_by_policy', async (t) => {
  const testCtx = await makeTestContext([signInRoute()], { allowDestructive: false });
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_file_delete'), testCtx.ctx, {
    path: '/config/app.json',
    confirm: true,
    reason: 'self-test cleanup',
  });

  assert.equal(result.isError, true);
  const payload = payloadOf(result);
  assert.equal(payload.error.code, 'disabled_by_policy');
  assert.equal(payload.error.details.flag, 'SSVC_ALLOW_DESTRUCTIVE');
  assert.equal(testCtx.calls.length, 0, 'флаг проверяется до сети');
});

test('oc_file_delete: DELETE, затем перечитанное дерево без файла → verified_absent', async (t) => {
  const state = treeState();
  const testCtx = await makeTestContext(
    [
      signInRoute(),
      treeRoute(state),
      {
        method: 'DELETE',
        path: /^\/rest\/files\/.+/,
        reply: (call) => {
          const removed = removeFromTree(state.tree, call.path.replace('/rest/files', ''));
          return removed
            ? { status: 204, body: '' }
            : { status: 500, body: 'Failed to delete file' };
        },
      },
    ],
    { allowDestructive: true },
  );
  t.after(() => testCtx.cleanup());

  const result = await runTool(tool('oc_file_delete'), testCtx.ctx, {
    path: '/config/nested/deep.txt',
    confirm: true,
    reason: 'self-test cleanup',
  });

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.ok, true);
  assert.equal(payload.path, '/config/nested/deep.txt');
  assert.equal(payload.verified_absent, true, 'отсутствие подтверждено перечитанным деревом');
  assert.equal(payload.warning, undefined);

  const deletes = callsFor(testCtx.calls, 'DELETE', /^\/rest\/files\/.+/);
  assert.equal(deletes.length, 1);
  assert.equal(deletes[0].path, '/rest/files/config/nested/deep.txt');
  assert.equal(callsFor(testCtx.calls, 'GET', '/rest/files').length, 1, 'дерево перечитано');
});

test('registerFilesTools: четыре инструмента видны в tools/list', async (t) => {
  const testCtx = await makeTestContext([signInRoute()]);
  t.after(() => testCtx.cleanup());

  const server = createMcpServer({ ctx: testCtx.ctx, tools: [] });
  registerFilesTools(server, testCtx.ctx);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp9-files-test', version: '1.0.0' });
  await client.connect(clientTransport);
  t.after(async () => {
    await client.close();
    await server.close();
  });

  const listed = await client.listTools();
  assert.deepEqual(
    listed.tools.map((entry) => entry.name).sort(),
    ['oc_file_copy', 'oc_file_delete', 'oc_file_download', 'oc_files_list'],
  );

  const remove = listed.tools.find((entry) => entry.name === 'oc_file_delete');
  assert.equal(remove?.annotations?.destructiveHint, true);
  const removeSchema = remove?.inputSchema as { properties?: Record<string, unknown> } | undefined;
  assert.ok(removeSchema?.properties?.confirm !== undefined);
  assert.ok(removeSchema?.properties?.reason !== undefined);
});
