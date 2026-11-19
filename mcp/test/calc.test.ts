/**
 * Тесты `oc_calculate_plan`: чистый расчёт через общий пакет.
 *
 * Проверяем не математику (она в packages/rectification/test), а адаптер:
 * плоские аргументы → профиль → план; отсутствие сетевых вызовов; предупреждения.
 */

import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { z } from 'zod';
import { runTool } from '../src/mcp/registry.js';
import { calcTools } from '../src/tools/calc.tools.js';
import { makeTestContext, signInRoute, type FakeRoute } from './fakeHttp.js';

function payloadOf(result: CallToolResult): Record<string, any> {
  assert.ok(result.structuredContent !== undefined, 'ожидался structuredContent');
  return result.structuredContent as Record<string, any>;
}

test('oc_calculate_plan: 20 л × 40 % → 8000 мл АС, без обращений к устройству', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(calcTools()[0], testCtx.ctx, {
    volume_l: 20,
    strength_pct: 40,
    heads_percent: 3,
    hearts_percent: 75,
    late_heads_enabled: false,
    tails_enabled: false,
  });

  assert.equal(result.isError, undefined);
  const payload = payloadOf(result);
  assert.equal(payload.plan.total_as_ml, 8000);
  assert.equal(payload.plan.fractions.heads_ml, 240);
  assert.equal(testCtx.calls.length, 0, 'расчёт не должен ходить в устройство');
});

test('oc_calculate_plan: formula=2 (авто 92+) принимается и помечается предупреждением', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(calcTools()[0], testCtx.ctx, {
    volume_l: 10,
    strength_pct: 40,
    formula: 2,
    decrement: 5,
  });

  assert.equal(result.isError, undefined, 'formula=2 должен приниматься (прошивка API ≥1.9)');
  const payload = payloadOf(result);
  assert.ok(
    payload.warnings.some((line: string) => line.includes('авто 92+')),
    'расчёт обязан честно сказать, что авто-старт не моделируется',
  );

  // Значение вне диапазона по-прежнему отвергается схемой инструмента.
  // runTool вызывает обработчик напрямую, без валидации MCP-слоя, поэтому
  // границы проверяются zod-схемой так же, как это делает протокол.
  const schema = z.object(calcTools()[0].inputSchema);
  assert.equal(schema.safeParse({ volume_l: 10, strength_pct: 40, formula: 2 }).success, true);
  assert.equal(schema.safeParse({ volume_l: 10, strength_pct: 40, formula: 3 }).success, false);
});

test('oc_calculate_plan: отключённые головы/тело не выключают стадии — это видно в warnings', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(calcTools()[0], testCtx.ctx, {
    volume_l: 10,
    strength_pct: 40,
    heads_enabled: false,
    hearts_enabled: false,
  });

  const payload = payloadOf(result);
  const warnings: string[] = payload.warnings;
  assert.ok(warnings.some((line) => line.includes('heads_enabled=false')));
  assert.ok(warnings.some((line) => line.includes('hearts_enabled=false')));
});

test('oc_calculate_plan: hearts_finish_temp пересчитывает тело и даёт предупреждение', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(calcTools()[0], testCtx.ctx, {
    volume_l: 10,
    strength_pct: 40,
    hearts_finish_temp: 90,
    hearts_percent: 80,
  });

  const payload = payloadOf(result);
  assert.ok(payload.plan.fractions.hearts_ml > 0);
  assert.ok(payload.warnings.some((line: string) => line.includes('hearts_percent игнорируется')));
});

test('oc_calculate_plan: сумма фракций больше 100 % — предупреждение', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(calcTools()[0], testCtx.ctx, {
    volume_l: 18,
    strength_pct: 40,
    heads_percent: 10,
    late_heads_percent: 20,
    hearts_percent: 80,
    tails_percent: 10,
  });

  const payload = payloadOf(result);
  assert.ok(payload.warnings.some((line: string) => line.includes('больше 100')));
});

test('oc_calculate_plan: нулевая пропускная способность — предупреждение', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const result = await runTool(calcTools()[0], testCtx.ctx, {
    volume_l: 18,
    strength_pct: 40,
    valve_bw_mlh: [0, 12000, 7000],
  });

  const payload = payloadOf(result);
  assert.ok(payload.warnings.some((line: string) => line.includes('равна 0')));
});

test('oc_calculate_plan: без power_kw — source default и предупреждение', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(calcTools()[0], testCtx.ctx, { volume_l: 18, strength_pct: 40 }),
  );

  assert.equal(payload.assumptions.power_kw, 2.5);
  assert.equal(payload.assumptions.power_kw_source, 'default');
  assert.match(String(payload.assumptions.warning), /default/);
});

test('oc_calculate_plan: с power_kw — source input и без предупреждения', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(calcTools()[0], testCtx.ctx, {
      volume_l: 18,
      strength_pct: 40,
      power_kw: 2,
    }),
  );

  assert.equal(payload.assumptions.power_kw, 2);
  assert.equal(payload.assumptions.power_kw_source, 'input');
  assert.equal(payload.assumptions.warning, null);
});

test('oc_calculate_plan: settings — готовые пары, recipe — доли, без дублей (D3)', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(calcTools()[0], testCtx.ctx, { volume_l: 18, strength_pct: 40 }),
  );

  assert.ok(Array.isArray(payload.settings.heads), 'пары идут массивом, как ждёт контроллер');
  assert.equal(payload.settings.heads.length, 2);
  assert.ok(payload.settings.heads[0] > 0, 'нулей в применяемом блоке быть не должно');
  assert.ok(payload.settings.hearts[0] > 0);
  assert.equal(typeof payload.settings.heads_timer, 'number');
  assert.equal(payload.settings.start_delay, 1200, 'стабилизация — отдельным start_delay');
  assert.equal(
    payload.settings.heads_timer,
    Math.max(0, payload.plan.timers_sec.heads - 1200),
    'heads_timer не должен включать стабилизацию: иначе этап голов растянется на 20 минут',
  );

  assert.equal(payload.recipe.heads.percent, 3);
  assert.equal(payload.recipe.heads.period_s, 120);
  assert.equal(payload.recipe.hearts.percent, 75);

  assert.equal('suggested_profile' in payload, false, 'старое зеркало профиля удалено');
  assert.equal('ssvc' in payload.plan, false, 'второго представления настроек нет');
  assert.ok(
    payload.assumptions.notes.some((line: string) => line.includes('valve_bw[i]')),
    'метод пересчёта должен ехать вместе с планом',
  );
});

test('oc_calculate_plan: compare:true читает устройство и возвращает diff', async (t) => {
  const CURRENT: FakeRoute = {
    method: 'GET',
    path: '/rest/settings',
    reply: () => ({
      body: {
        ssvcSettings: {
          heads: [1.7, 120],
          late_heads: [1, 60],
          hearts: [1, 10],
          tails: [2, 2],
          heads_timer: 7800,
          late_heads_timer: 15600,
        },
      },
    }),
  };
  const testCtx = await makeTestContext([signInRoute(), CURRENT]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(calcTools()[0], testCtx.ctx, {
      volume_l: 18,
      strength_pct: 40,
      compare: true,
    }),
  );

  assert.ok(Array.isArray(payload.diff.changed));
  assert.ok(payload.diff.changed.includes('heads'), 'пары плана отличаются от контроллера');
  assert.equal(
    testCtx.calls.some((call) => call.path === '/rest/settings'),
    true,
    'compare:true — единственный случай, когда расчёт читает устройство',
  );
});

test('oc_calculate_plan: без compare устройство не читается', async (t) => {
  const testCtx = await makeTestContext([]);
  t.after(() => testCtx.cleanup());

  const payload = payloadOf(
    await runTool(calcTools()[0], testCtx.ctx, { volume_l: 18, strength_pct: 40 }),
  );

  assert.equal('diff' in payload, false);
  assert.equal(testCtx.calls.length, 0, 'по умолчанию расчёт остаётся чистым');
});
