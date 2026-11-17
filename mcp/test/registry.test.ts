/**
 * Тесты реестра инструментов (таблица §4 WP0).
 *
 * Проверяем через настоящий MCP-клиент и `InMemoryTransport`: это тот же протокол,
 * что и по HTTP, но без сети. Главный вопрос пакета — поведение `structuredContent`
 * при объявленной и необъявленной `outputSchema` (решение зафиксировано в README).
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { fail } from '../src/errors.js';
import { registerTools } from '../src/mcp/registry.js';
import { createMcpServer } from '../src/mcp/server.js';
import { defineTool, type ToolContext, type ToolDef } from '../src/mcp/types.js';
import { makeTestContext } from './fakeHttp.js';

interface TestHarness {
  after(fn: () => void | Promise<void>): void;
}

/** Поднимает сервер и клиент in-memory, гарантированно закрывая оба. */
async function withClient(
  t: TestHarness,
  tools: ToolDef[],
  run: (client: Client, ctx: ToolContext) => Promise<void>,
): Promise<void> {
  const testContext = await makeTestContext([]);
  t.after(() => testContext.cleanup());

  const server = createMcpServer({ ctx: testContext.ctx, tools });
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp0-registry-test', version: '1.0.0' });
  await client.connect(clientTransport);
  try {
    await run(client, testContext.ctx);
  } finally {
    await client.close();
    await server.close();
  }
}

/** Текстовая часть результата: в ней всегда лежит тот же JSON, что и в structuredContent. */
function firstText(result: unknown): string {
  const content = (result as { content?: Array<{ type?: string; text?: string }> }).content;
  return content?.[0]?.text ?? '';
}

function structured(result: unknown): Record<string, unknown> | undefined {
  return (result as { structuredContent?: Record<string, unknown> }).structuredContent;
}

function isErrorResult(result: unknown): boolean {
  return (result as { isError?: boolean }).isError === true;
}

const ECHO_TOOL = defineTool({
  name: 'oc_test_echo',
  title: 'Тестовое эхо',
  description: 'Возвращает переданный текст. Ничего не меняет на контроллере.',
  inputSchema: { text: z.string().describe('Текст, который нужно вернуть') },
  annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
  handler: async (args) => ({
    ok: true,
    echo: args.text,
    observed_at: '2026-01-01T00:00:00.000Z',
  }),
});

test('инструмент регистрируется и вызывается: успех → ok:true + structuredContent', async (t) => {
  await withClient(t, [ECHO_TOOL], async (client) => {
    const listed = await client.listTools();
    const entry = listed.tools.find((tool) => tool.name === 'oc_test_echo');
    assert.ok(entry !== undefined, 'инструмент должен быть в tools/list');
    assert.equal(entry.title, 'Тестовое эхо');
    assert.match(entry.description ?? '', /Ничего не меняет/);
    assert.equal(entry.annotations?.readOnlyHint, true);

    const result = await client.callTool({
      name: 'oc_test_echo',
      arguments: { text: 'привет' },
    });
    assert.equal(isErrorResult(result), false);

    const payload = structured(result);
    assert.ok(payload !== undefined, 'structuredContent обязателен для успеха');
    assert.deepEqual(payload, {
      ok: true,
      echo: 'привет',
      observed_at: '2026-01-01T00:00:00.000Z',
    });
    // Текстовая часть — тот же JSON: клиент, который не читает structuredContent, не теряет данные.
    assert.deepEqual(JSON.parse(firstText(result)), payload);
  });
});

test('ok:false превращается в isError:true и конверт ошибки', async (t) => {
  const failing = defineTool({
    name: 'oc_test_blocked',
    title: 'Тестовый предохранитель',
    description: 'Всегда возвращает отказ guard_blocked. Ничего не делает.',
    inputSchema: {},
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    handler: async () =>
      fail('guard_blocked', 'Контроллер отклонил команду: сработал предохранитель', {
        hint: 'Проверьте датчики уровня XKC-25.',
        details: { blockReasons: ['tank_not_full'] },
      }),
  });

  await withClient(t, [failing], async (client) => {
    const result = await client.callTool({ name: 'oc_test_blocked', arguments: {} });
    assert.equal(isErrorResult(result), true, 'отказ должен быть isError:true');

    const payload = structured(result);
    assert.ok(payload !== undefined, 'без outputSchema конверт ошибки идёт и в structuredContent');
    const error = payload.error as Record<string, unknown>;
    assert.equal(error.code, 'guard_blocked');
    assert.equal(error.retryable, false);
    assert.match(String(error.hint), /XKC-25/);

    // Текст совпадает со structuredContent — клиенту достаточно прочитать любой из них.
    assert.deepEqual(JSON.parse(firstText(result)), payload);
  });
});

test('без outputSchema structuredContent проходит с произвольными полями (без проверки)', async (t) => {
  const freeform = defineTool({
    name: 'oc_test_freeform',
    title: 'Тестовый свободный ответ',
    description: 'Возвращает вложенный объект. Схема вывода не объявлена.',
    inputSchema: {},
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    handler: async () => ({
      ok: true,
      value: 'строка',
      extra: { nested: [1, 2, 3], flag: true },
      неизвестное_поле: 'проходит как есть',
    }),
  });

  await withClient(t, [freeform], async (client) => {
    const result = await client.callTool({ name: 'oc_test_freeform', arguments: {} });
    assert.equal(isErrorResult(result), false);
    const payload = structured(result);
    assert.ok(payload !== undefined);
    assert.deepEqual(payload.extra, { nested: [1, 2, 3], flag: true });
    assert.equal(payload['неизвестное_поле'], 'проходит как есть');
  });
});

test('с outputSchema успешный structuredContent проверяется и проходит', async (t) => {
  const strict = defineTool({
    name: 'oc_test_strict',
    title: 'Тестовый строгий ответ',
    description: 'Возвращает число по объявленной схеме вывода.',
    inputSchema: {},
    outputSchema: { ok: z.literal(true), value: z.number() },
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    handler: async () => ({ ok: true, value: 42 }),
  });

  await withClient(t, [strict], async (client) => {
    const result = await client.callTool({ name: 'oc_test_strict', arguments: {} });
    assert.equal(isErrorResult(result), false);
    assert.deepEqual(structured(result), { ok: true, value: 42 });
  });
});

test('с outputSchema несоответствие схеме отвергается SDK', async (t) => {
  const broken = defineTool({
    name: 'oc_test_broken',
    title: 'Тестовый сломанный ответ',
    description: 'Возвращает строку вместо числа — нарушение объявленной схемы вывода.',
    inputSchema: {},
    outputSchema: { ok: z.literal(true), value: z.number() },
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    // Намеренно нарушаем схему: тест фиксирует, что SDK это ловит.
    handler: async () => ({ ok: true, value: 'не число' }) as unknown as { ok: true; value: number },
  });

  await withClient(t, [broken], async (client) => {
    const result = await client.callTool({ name: 'oc_test_broken', arguments: {} });
    assert.equal(isErrorResult(result), true, 'несоответствие outputSchema должно быть ошибкой');
    assert.match(firstText(result), /Output validation error/i);
  });
});

test('с outputSchema отказ не несёт structuredContent (клиент проверяет и ошибки)', async (t) => {
  const strictFail = defineTool({
    name: 'oc_test_strict_fail',
    title: 'Тестовый строгий отказ',
    description: 'Возвращает отказ при объявленной схеме вывода.',
    inputSchema: {},
    outputSchema: { ok: z.literal(true), value: z.number() },
    annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    handler: async () => fail('disabled_by_policy', 'Действие выключено настройкой сервера'),
  });

  await withClient(t, [strictFail], async (client) => {
    const result = await client.callTool({ name: 'oc_test_strict_fail', arguments: {} });
    assert.equal(isErrorResult(result), true);
    assert.equal(
      structured(result),
      undefined,
      'конверт ошибки не должен попадать в structuredContent: клиент SDK валидирует его даже при isError',
    );
    const payload = JSON.parse(firstText(result)) as { error: { code: string } };
    assert.equal(payload.error.code, 'disabled_by_policy');
  });
});

test('неизвестное исключение обработчика → internal_error, а не падение сервера', async (t) => {
  const throwing = defineTool({
    name: 'oc_test_throws',
    title: 'Тестовое исключение',
    description: 'Бросает исключение, чтобы проверить конверт internal_error.',
    inputSchema: {},
    annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    handler: async () => {
      throw new Error('boom');
    },
  });

  await withClient(t, [throwing], async (client) => {
    const result = await client.callTool({ name: 'oc_test_throws', arguments: {} });
    assert.equal(isErrorResult(result), true);
    const payload = structured(result);
    assert.ok(payload !== undefined);
    const error = payload.error as Record<string, unknown>;
    assert.equal(error.code, 'internal_error');
    assert.match(String(error.message), /boom/);
  });
});

test('повторное имя инструмента — ошибка на старте, а не в рантайме', async (t) => {
  const testContext = await makeTestContext([]);
  t.after(() => testContext.cleanup());
  const server: McpServer = createMcpServer({ ctx: testContext.ctx, tools: [] });

  assert.throws(
    () => registerTools(server, testContext.ctx, [ECHO_TOOL, ECHO_TOOL]),
    /дважды/,
  );
});
