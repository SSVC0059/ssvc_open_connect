/**
 * Определение класса риска инструментов «по поведению» (WP11, релизная закалка §15).
 *
 * Почему не по константе: класс риска живёт внутри обработчика (`enforceRisk(ctx, tool, risk)`)
 * и намеренно не вынесен в `ToolDef` — контракт §2.2 менять нельзя. Зато политика
 * наблюдаема: у риска ≥2 обязателен `confirm`, у риска ≥3 — ещё и `reason`
 * (mcp/policy/confirmation.ts, §2.6). Поэтому тест не хардкодит имена инструментов,
 * а спрашивает у самих инструментов, что они требуют, и сверяет это с политикой.
 *
 * Вызовы идут в фейковое устройство (`test/fakeHttp.ts`), то есть без сети: риск-2
 * инструменты после `enforceRisk` упираются в 404 фейка и возвращают ошибку — это
 * ожидаемо и на классификацию не влияет.
 */

import { runTool } from '../src/mcp/registry.js';
import { allTools } from '../src/tools/index.js';
import type { ToolDef } from '../src/mcp/types.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { makeTestContext } from './fakeHttp.js';

/**
 * Аргументы-«заглушки»: покрывают обязательные поля всех инструментов, чтобы отказ
 * политики не подменялся отказом валидации.
 */
export const RISK_PROBE_ARGS: Readonly<Record<string, unknown>> = Object.freeze({
  id: 'p1',
  name: 'e2e-profile',
  content: {},
  path: '/config/app.json',
  source: '/config/app.json',
  destination: '/config/app-copy.json',
  bit: 0,
  enable: true,
  energized: false,
  index: 0,
  step: 'without_liquid',
  command: 'status',
  settings: {},
  thresholds: {},
  config: {},
  zone: 'tank',
  address: '28FF64A1B2C3D4E5',
  process_id: 1,
  subsystem: 'thermal',
  enabled: true,
});

export interface RiskProbeResult {
  name: string;
  /** Без `confirm` инструмент отвечает `confirmation_required`. */
  requiresConfirm: boolean;
  /** С `confirm:true`, но коротким `reason` — `validation_failed` с `required:'reason'`. */
  requiresReason: boolean;
}

/** Конверт ошибки из результата вызова: `structuredContent` либо текстовая часть. */
export function errorOf(result: CallToolResult): {
  code?: string;
  message?: string;
  hint?: string;
  details?: Record<string, unknown>;
} | null {
  const structured = result.structuredContent as
    | { ok?: boolean; error?: { code?: string; message?: string; hint?: string; details?: Record<string, unknown> } }
    | undefined;
  if (structured !== undefined && structured.ok === false) return structured.error ?? {};
  const text = result.content?.[0];
  if (text !== undefined && text.type === 'text') {
    try {
      const parsed = JSON.parse(text.text) as {
        ok?: boolean;
        error?: { code?: string; message?: string; hint?: string; details?: Record<string, unknown> };
      };
      if (parsed.ok === false) return parsed.error ?? {};
    } catch {
      return null;
    }
  }
  return null;
}

/** Успешный конверт результата (для e2e/тестов каталога). */
export function payloadOf(result: CallToolResult): Record<string, any> {
  const structured = result.structuredContent as Record<string, any> | undefined;
  if (structured !== undefined) return structured;
  const text = result.content?.[0];
  if (text !== undefined && text.type === 'text') {
    return JSON.parse(text.text) as Record<string, any>;
  }
  throw new Error('Результат вызова не содержит ни structuredContent, ни текстовой части');
}

/**
 * Спрашивает у разрушительных инструментов, какие требования они предъявляют.
 * Возвращает по записи на каждый инструмент с `destructiveHint`, включая риск 2:
 * так видно, что «требует reason» — это ровно класс ≥3, а не любое разрушительное действие.
 */
export async function probeDestructiveTools(
  tools: readonly ToolDef[] = allTools,
): Promise<RiskProbeResult[]> {
  const candidates = tools.filter((tool) => tool.annotations.destructiveHint === true);
  const testCtx = await makeTestContext([], { allowDestructive: false });
  const results: RiskProbeResult[] = [];
  try {
    for (const tool of candidates) {
      const withoutConfirm = await runTool(tool, testCtx.ctx, { ...RISK_PROBE_ARGS });
      const requiresConfirm = errorOf(withoutConfirm)?.code === 'confirmation_required';

      let requiresReason = false;
      if (requiresConfirm) {
        const shortReason = await runTool(tool, testCtx.ctx, {
          ...RISK_PROBE_ARGS,
          confirm: true,
          reason: 'мало',
        });
        const error = errorOf(shortReason);
        requiresReason =
          error?.code === 'validation_failed' && error.details?.required === 'reason';
      }

      results.push({ name: tool.name, requiresConfirm, requiresReason });
    }
  } finally {
    await testCtx.cleanup();
  }
  return results;
}

/** Имена инструментов класса ≥3 (требуют `confirm` + `reason`) — вычисляется, не хардкодится. */
export async function risk3ToolNames(tools: readonly ToolDef[] = allTools): Promise<string[]> {
  const probes = await probeDestructiveTools(tools);
  return probes.filter((probe) => probe.requiresReason).map((probe) => probe.name).sort();
}
