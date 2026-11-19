/**
 * Инструменты WP5: реле на PCF8574 (mcp-server-agents.md §9).
 *
 *  - `oc_get_relay_metadata` — риск 0, справочник реле: номера, подписи, доступность,
 *                              зарезервированные под аварии;
 *  - `oc_get_relay_state`    — риск 0, битовые маски теневого регистра по расширителям;
 *  - `oc_relay_override`     — риск 2 (влияет на процесс), ручное переключение одного реле.
 *
 * Риск 2 требует `confirm:true` (`enforceRisk` до любых сетевых вызовов); запись
 * выполняется в домене через `client.withWrite('relay', …)` и не ретраится. На GPIO-сборке
 * все три инструмента отвечают `feature_unsupported` (§2.5 п.9): это ограничение прошивки.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { getRelayMetadata, getRelayState, setRelayOverride } from '../domain/relay.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const OVERRIDE_RISK: RiskClass = 2;

export const ocGetRelayMetadata = defineTool({
  name: 'oc_get_relay_metadata',
  title: 'Метаданные реле',
  description:
    'Возвращает список реле расширителя: номера, подписи, доступность для ручного ' +
    'управления и системные реле, зарезервированные под аварии.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    // Принудительное чтение: агент должен видеть актуальные подписи и editable,
    // а не снимок, который домен держит для локальной проверки bit (TTL 60 с).
    const snapshot = await getRelayMetadata(ctx.client, { force: true });
    return { ok: true, ...snapshot } as ToolResult;
  },
});

export const ocGetRelayState = defineTool({
  name: 'oc_get_relay_state',
  title: 'Состояние реле',
  description:
    'Возвращает текущее состояние линий реле (битовую маску) для каждого расширителя.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    const snapshot = await getRelayState(ctx.client);
    return { ok: true, ...snapshot } as ToolResult;
  },
});

export const ocRelayOverride = defineTool({
  name: 'oc_relay_override',
  title: 'Ручное управление реле',
  description:
    'Включает или выключает одно реле вручную, минуя правила автоматики. Обязательно ' +
    'указывайте и enable, и energized: значение по умолчанию у контроллера включает реле. ' +
    'Системные реле аварий менять нельзя.',
  inputSchema: {
    bit: z
      .number()
      .int()
      .min(0)
      .describe(
        'Номер бита реле: 0-based глобальный target_bit из oc_get_relay_metadata. ' +
          'Не путайте с номером реле index (он 1-based). Проверяется локально по ' +
          'relay_line_count до запроса.',
      ),
    enable: z
      .boolean()
      .describe(
        'Включить ручное управление этим битом. Обязателен: у прошивки значение по ' +
          'умолчанию true, поэтому пропуск поля включит реле.',
      ),
    energized: z
      .boolean()
      .describe(
        'Требуемое состояние реле: true — включено, false — выключено. Обязателен: ' +
          'без него прошивка подставит false.',
      ),
    confirm: z
      .boolean()
      .optional()
      .describe(
        'Подтверждение действия (риск 2): ручное управление обходит автоматику. ' +
          'Без confirm:true запрос отклоняется, контроллер не переключается.',
      ),
  },
  annotations: annotationsForRisk(OVERRIDE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    try {
      // До любых сетевых вызовов: «забытый confirm» не должен дойти до реле.
      enforceRisk(ctx, 'oc_relay_override', OVERRIDE_RISK, {
        confirm: args.confirm === true,
      });

      const outcome = await setRelayOverride(ctx.client, {
        bit: args.bit,
        enable: args.enable,
        energized: args.energized,
      });

      auditAction(ctx, {
        tool: 'oc_relay_override',
        risk: OVERRIDE_RISK,
        confirm: true,
        result: 'ok',
        details: {
          bit: outcome.bit,
          enable: outcome.enable,
          energized: outcome.energized,
        },
      });

      return {
        ok: true,
        bit: outcome.bit,
        enable: outcome.enable,
        energized: outcome.energized,
      } as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_relay_override',
        risk: OVERRIDE_RISK,
        confirm: args.confirm === true,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: {
          bit: typeof args.bit === 'number' ? args.bit : null,
          enable: typeof args.enable === 'boolean' ? args.enable : null,
          energized: typeof args.energized === 'boolean' ? args.energized : null,
        },
      });
      throw error;
    }
  },
});

/** Инструменты домена: и для `registerRelayTools`, и для сборки в `src/index.ts`. */
export function relayTools(): ToolDef[] {
  return [ocGetRelayMetadata, ocGetRelayState, ocRelayOverride];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerRelayTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, relayTools());
}
