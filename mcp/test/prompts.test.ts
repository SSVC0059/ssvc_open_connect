/**
 * Тесты промптов-сценариев (WP10, mcp-server-agents.md §14).
 *
 * Проверяем три вещи:
 *  1) набор и аргументы промптов совпадают с таблицей §14;
 *  2) тексты упоминают только реальные инструменты из `*Tools()` — никаких выдуманных имён;
 *  3) регистрация видна клиенту через настоящий SDK: `prompts/list` и `prompts/get`.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import type { GetPromptResult } from '@modelcontextprotocol/sdk/types.js';
import { createMcpServer } from '../src/mcp/server.js';
import { PROMPT_SPECS, registerPrompts } from '../src/prompts/prompts.js';
import { commandsTools } from '../src/tools/commands.tools.js';
import { filesTools } from '../src/tools/files.tools.js';
import { hardwareTools } from '../src/tools/hardware.tools.js';
import { infoTools } from '../src/tools/info.tools.js';
import { logsTools } from '../src/tools/logs.tools.js';
import { profilesTools } from '../src/tools/profiles.tools.js';
import { relayTools } from '../src/tools/relay.tools.js';
import { sensorsTools } from '../src/tools/sensors.tools.js';
import { settingsTools } from '../src/tools/settings.tools.js';
import { systemTools } from '../src/tools/system.tools.js';
import { telemetryTools } from '../src/tools/telemetry.tools.js';
import { xkc25Tools } from '../src/tools/xkc25.tools.js';
import type { ToolDef } from '../src/mcp/types.js';
import { makeTestContext } from './fakeHttp.js';

interface TestHarness {
  after(fn: () => void | Promise<void>): void;
}

/** Поднимает сервер с промптами и клиент in-memory, гарантированно закрывая оба. */
async function withPromptsClient(
  t: TestHarness,
  run: (client: Client) => Promise<void>,
): Promise<void> {
  const testContext = await makeTestContext([]);
  t.after(() => testContext.cleanup());

  const server = createMcpServer({ ctx: testContext.ctx, tools: [] });
  registerPrompts(server);
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);

  const client = new Client({ name: 'wp10-prompts-test', version: '1.0.0' });
  await client.connect(clientTransport);
  try {
    await run(client);
  } finally {
    await client.close();
    await server.close();
  }
}

/** Текст первого сообщения `prompts/get`: промпты отдают ровно одно текстовое сообщение. */
function promptText(result: GetPromptResult): string {
  const first = result.messages[0];
  assert.ok(first !== undefined, 'prompts/get должен вернуть хотя бы одно сообщение');
  assert.equal(first.content.type, 'text');
  return first.content.type === 'text' ? first.content.text : '';
}

/** Ожидаемые аргументы из таблицы §14: имя → обязательность. */
const EXPECTED_ARGUMENTS: Readonly<Record<string, ReadonlyArray<{ name: string; required: boolean }>>> = {
  start_rectification: [
    { name: 'profile_id', required: false },
    { name: 'confirm_token', required: false },
  ],
  troubleshoot_start_blocked: [],
  calibrate_xkc25: [{ name: 'index', required: true }],
  analyze_last_run: [{ name: 'process_id', required: false }],
  apply_profile: [{ name: 'profile_id', required: true }],
};

/** Все доменные инструменты: ровно то, что реально регистрируется в `src/index.ts`. */
function allTools(): ToolDef[] {
  return [
    ...infoTools(),
    ...telemetryTools(),
    ...commandsTools(),
    ...settingsTools(),
    ...sensorsTools(),
    ...relayTools(),
    ...hardwareTools(),
    ...xkc25Tools(),
    ...logsTools(),
    ...profilesTools(),
    ...filesTools(),
    ...systemTools(),
  ];
}

/** Аргументы, с которыми собираем текст для проверки имён инструментов. */
const SAMPLE_ARGS: Readonly<Record<string, Record<string, unknown>>> = {
  start_rectification: { profile_id: 'spirt-syrec', confirm_token: 'operator-ok' },
  troubleshoot_start_blocked: {},
  calibrate_xkc25: { index: 1 },
  analyze_last_run: { process_id: 12 },
  apply_profile: { profile_id: 'spirt-syrec' },
};

test('зарегистрировано ровно 5 промптов; имена и аргументы совпадают с таблицей', () => {
  assert.equal(PROMPT_SPECS.length, 5, 'в WP10 ровно пять сценариев');
  assert.deepEqual(
    PROMPT_SPECS.map((spec) => spec.name),
    Object.keys(EXPECTED_ARGUMENTS),
    'имена и порядок промптов должны совпадать с таблицей §14',
  );

  for (const spec of PROMPT_SPECS) {
    const expected = EXPECTED_ARGUMENTS[spec.name] ?? [];
    const shape = spec.argsSchema ?? {};
    assert.deepEqual(
      Object.keys(shape),
      expected.map((arg) => arg.name),
      `аргументы промпта ${spec.name}`,
    );
    for (const arg of expected) {
      const field = shape[arg.name];
      assert.ok(field !== undefined, `у промпта ${spec.name} нет схемы аргумента ${arg.name}`);
      assert.equal(
        field.isOptional(),
        !arg.required,
        `${spec.name}.${arg.name}: обязательность не совпадает с таблицей`,
      );
    }
  }
});

test('тексты промптов упоминают только существующие инструменты из *Tools()', () => {
  const tools = allTools();
  const known = new Set(tools.map((tool) => tool.name));
  assert.equal(known.size, 50, 'каталог инструментов: 50 уникальных имён');

  // Минимум, который каждый сценарий обязан назвать по таблице §14.
  const requiredMentions: Readonly<Record<string, readonly string[]>> = {
    start_rectification: [
      'oc_get_info',
      'oc_get_process_status',
      'oc_get_xkc25_status',
      'oc_start',
      'oc_wait_for_change',
    ],
    troubleshoot_start_blocked: [
      'oc_get_xkc25_status',
      'oc_get_hardware_faults',
    ],
    calibrate_xkc25: ['oc_xkc25_calibrate', 'oc_get_xkc25_status'],
    analyze_last_run: ['oc_logs_list', 'oc_logs_download'],
    apply_profile: ['oc_profile_get', 'oc_get_settings', 'oc_profile_apply'],
  };

  for (const spec of PROMPT_SPECS) {
    const text = spec.build(SAMPLE_ARGS[spec.name] ?? {});
    const mentioned: string[] = text.match(/\boc_[a-z0-9_]+\b/g) ?? [];
    assert.ok(mentioned.length > 0, `промпт ${spec.name} должен называть инструменты`);

    for (const name of mentioned) {
      assert.ok(
        known.has(name),
        `промпт ${spec.name} упоминает несуществующий инструмент «${name}»`,
      );
    }
    for (const name of requiredMentions[spec.name] ?? []) {
      assert.ok(
        mentioned.includes(name),
        `промпт ${spec.name} должен упоминать «${name}» по таблице §14`,
      );
    }
  }
});

test('prompts/list через SDK-клиент отдаёт 5 промптов с аргументами', async (t) => {
  await withPromptsClient(t, async (client) => {
    const listed = await client.listPrompts();
    assert.equal(listed.prompts.length, 5);
    assert.deepEqual(
      listed.prompts.map((prompt) => prompt.name),
      Object.keys(EXPECTED_ARGUMENTS),
    );

    const byName = new Map(listed.prompts.map((prompt) => [prompt.name, prompt]));

    const start = byName.get('start_rectification');
    assert.ok(start !== undefined);
    assert.deepEqual(
      start.arguments?.map((arg) => ({ name: arg.name, required: arg.required })),
      EXPECTED_ARGUMENTS.start_rectification,
    );

    const calibrate = byName.get('calibrate_xkc25');
    assert.ok(calibrate !== undefined);
    assert.deepEqual(
      calibrate.arguments?.map((arg) => ({ name: arg.name, required: arg.required })),
      EXPECTED_ARGUMENTS.calibrate_xkc25,
    );

    const blocked = byName.get('troubleshoot_start_blocked');
    assert.ok(blocked !== undefined);
    assert.deepEqual(blocked.arguments ?? [], [], 'у промпта без аргументов пустой список');

    // У каждого промпта есть русское название и описание.
    for (const prompt of listed.prompts) {
      assert.ok((prompt.title ?? '').length > 0, `${prompt.name}: пустой title`);
      assert.ok((prompt.description ?? '').length > 0, `${prompt.name}: пустое description`);
    }
  });
});

test('prompts/get start_rectification подставляет profile_id в текст', async (t) => {
  await withPromptsClient(t, async (client) => {
    const result = await client.getPrompt({
      name: 'start_rectification',
      arguments: { profile_id: 'spirt-syrec' },
    });

    const text = promptText(result);
    assert.match(text, /spirt-syrec/, 'идентификатор профиля должен попасть в текст');
    assert.match(text, /oc_profile_get/);
    assert.match(text, /oc_start/);
    // Без confirm_token сценарий обязан остановиться перед запуском.
    assert.match(text, /Не вызывай oc_start/);
  });
});

test('промпт без аргументов вызывается без arguments', async (t) => {
  await withPromptsClient(t, async (client) => {
    const result = await client.getPrompt({ name: 'troubleshoot_start_blocked' });
    const text = promptText(result);
    assert.match(text, /oc_get_xkc25_status/);
    assert.match(text, /oc_get_hardware_faults/);
    assert.match(text, /block_reasons/);
  });
});
