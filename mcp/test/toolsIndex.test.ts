/**
 * Тесты единого каталога инструментов (WP11, §15 п.1).
 *
 * Что защищается:
 *  - каталог действительно собран из всех доменов (50 инструментов, 12 групп);
 *  - имена уникальны: дубликат — исключение при импорте каталога, а не тихая
 *    перезапись инструмента в реестре MCP;
 *  - у каждого `ToolDef` заполнены `name/title/description/inputSchema` и аннотации;
 *  - инструменты класса риска ≥3 существуют и помечены как разрушительные.
 *
 * Класс риска определяется по поведению политики (`test/riskProbe.ts`), а не списком
 * имён в тесте: имена не хардкодятся, иначе тест перестал бы замечать новый
 * разрушительный инструмент.
 */

import assert from 'node:assert/strict';
import test from 'node:test';
import {
  allTools,
  assertUniqueToolNames,
  findTool,
  toolGroups,
  uniqueToolNames,
} from '../src/tools/index.js';
import type { ToolDef } from '../src/mcp/types.js';
import { probeDestructiveTools } from './riskProbe.js';

/** Инструментов в каталоге v1: 51 (mcp-server.md §4, §15). */
const EXPECTED_TOOL_COUNT = 51;

/** Домены WP1–WP9: по группе на файл `src/tools/<домен>.tools.ts`. */
const EXPECTED_DOMAINS = [
  'calc',
  'commands',
  'files',
  'hardware',
  'info',
  'logs',
  'profiles',
  'relay',
  'sensors',
  'settings',
  'system',
  'telemetry',
  'xkc25',
];

test('каталог: ровно 51 инструмент, имена уникальны', () => {
  assert.equal(allTools.length, EXPECTED_TOOL_COUNT);
  assert.equal(new Set(uniqueToolNames).size, allTools.length);
  assert.equal(uniqueToolNames.length, allTools.length);
  assert.deepEqual(
    [...uniqueToolNames].sort(),
    allTools.map((tool) => tool.name).sort(),
    'uniqueToolNames — то же содержимое, что allTools',
  );
});

test('каталог: имя каждого инструмента — oc_<домен>_<действие>', () => {
  for (const name of uniqueToolNames) {
    assert.match(name, /^oc_[a-z0-9]+(?:_[a-z0-9]+)*$/, `имя «${name}» не в snake_case`);
  }
});

test('каталог: у каждого ToolDef есть name/title/description/inputSchema и аннотации', () => {
  for (const tool of allTools) {
    assert.equal(typeof tool.name, 'string');
    assert.notEqual(tool.name.trim(), '', 'name не может быть пустым');
    assert.equal(typeof tool.title, 'string');
    assert.ok(tool.title.trim().length > 0, `${tool.name}: пустой title`);
    assert.ok(tool.title.length <= 60, `${tool.name}: title длиннее 60 символов`);
    assert.equal(typeof tool.description, 'string');
    assert.ok(tool.description.trim().length > 0, `${tool.name}: пустой description`);
    assert.equal(typeof tool.inputSchema, 'object');
    assert.notEqual(tool.inputSchema, null);
    for (const [field, schema] of Object.entries(tool.inputSchema)) {
      assert.equal(
        typeof (schema as { safeParse?: unknown }).safeParse,
        'function',
        `${tool.name}: inputSchema.${field} — не zod-схема`,
      );
    }
    assert.equal(typeof tool.annotations.readOnlyHint, 'boolean');
    assert.equal(typeof tool.annotations.destructiveHint, 'boolean');
    assert.equal(typeof tool.annotations.idempotentHint, 'boolean');
    assert.equal(tool.annotations.openWorldHint, false, `${tool.name}: openWorldHint должен быть false`);
    assert.equal(typeof tool.handler, 'function');
  }
});

test('каталог: аннотации согласованы с политикой (чтение идемпотентно, разрушительное — нет)', () => {
  for (const tool of allTools) {
    if (tool.annotations.readOnlyHint) {
      assert.equal(
        tool.annotations.destructiveHint,
        false,
        `${tool.name}: чтение не может быть разрушительным`,
      );
      assert.equal(tool.annotations.idempotentHint, true, `${tool.name}: чтение идемпотентно`);
    }
    if (tool.annotations.destructiveHint) {
      assert.equal(
        tool.annotations.readOnlyHint,
        false,
        `${tool.name}: разрушительное действие не может быть readOnly`,
      );
    }
  }
});

test('каталог: группы доменов покрывают WP1–WP9 и дают весь каталог', () => {
  assert.deepEqual(
    toolGroups.map((group) => group.domain).sort(),
    EXPECTED_DOMAINS,
    'каждая группа — домен из mcp-server-agents.md §3',
  );
  const fromGroups = toolGroups.flatMap((group) => group.tools());
  assert.equal(fromGroups.length, allTools.length);
  assert.deepEqual(
    fromGroups.map((tool) => tool.name).sort(),
    [...uniqueToolNames].sort(),
  );
});

test('каталог: повторное имя — исключение на старте, а не тихая перезапись', () => {
  const first = allTools[0];
  assert.ok(first !== undefined);
  const duplicate: ToolDef = { ...first };
  assert.throws(
    () => assertUniqueToolNames([first, duplicate]),
    /повторяющиеся имена|зарегистрирован дважды|unique|уникальн/i,
    'дубликат обязан ломать старт',
  );
  // Одиночный инструмент проходит: проверка не ломает нормальную сборку.
  assert.doesNotThrow(() => assertUniqueToolNames([first]));
  assert.equal(assertUniqueToolNames([]).length, 0);
});

test('каталог: есть поиск по имени (для тестов и диагностики)', () => {
  const tool = findTool('oc_get_info');
  assert.ok(tool !== undefined);
  assert.equal(tool?.name, 'oc_get_info');
  assert.equal(findTool('oc_no_such_tool'), undefined);
});

test('каталог: инструменты риска ≥3 существуют и помечены разрушительными', async () => {
  const probes = await probeDestructiveTools(allTools);
  assert.ok(probes.length > 0, 'разрушительные инструменты должны существовать');

  const risk3 = probes.filter((probe) => probe.requiresReason);
  assert.ok(
    risk3.length > 0,
    'в каталоге должен быть хотя бы один инструмент класса ≥3 (confirm + reason)',
  );

  for (const probe of risk3) {
    const tool = findTool(probe.name);
    assert.ok(tool !== undefined, `инструмент «${probe.name}» есть в каталоге`);
    assert.equal(
      tool?.annotations.destructiveHint,
      true,
      `${probe.name}: риск ≥3 обязан быть помечен destructiveHint`,
    );
    assert.equal(tool?.annotations.readOnlyHint, false, `${probe.name}: риск ≥3 не readOnly`);
    assert.equal(tool?.annotations.idempotentHint, false, `${probe.name}: риск ≥3 не идемпотентен`);
  }

  // Обратное направление: если инструмент не помечен разрушительным, требовать reason
  // он не может — иначе агент упрётся в политику, о которой аннотации молчат.
  for (const probe of probes) {
    const tool = findTool(probe.name);
    if (tool?.annotations.destructiveHint === true) continue;
    assert.equal(
      probe.requiresReason,
      false,
      `${probe.name}: требует reason, но не помечен destructiveHint`,
    );
  }
});
