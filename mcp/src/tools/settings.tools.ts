/**
 * Инструменты WP3: чтение и частичное изменение настроек SSVC
 * (mcp-server-agents.md §7).
 *
 *  - `oc_get_settings`    — риск 0, `GET /rest/settings`, наружу только `ssvcSettings`;
 *  - `oc_update_settings` — риск 1 (обратимая запись), `PUT /rest/settings` плоским телом.
 *
 * Риск 1 по §2.6 не требует `confirm`, но каждая запись идёт через
 * `client.withWrite('settings', …)` и попадает в аудит. Неизвестные ключи НЕ
 * валидируются локально: прошивка отбрасывает их в `ignored`, а список
 * `PARAM_HANDLERS` в описании поля — только подсказка агенту.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { KNOWN_SETTING_KEYS } from '../device/schemas/settings.js';
import { getSettings, PARTIAL_HINT, updateSettings } from '../domain/settings.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const WRITE_RISK: RiskClass = 1;

export const ocGetSettings = defineTool({
  name: 'oc_get_settings',
  title: 'Действующие настройки SSVC',
  description:
    'Возвращает текущие настройки контроллера SSVC: фракции, таймеры, гистерезис, декремент, ' +
    'формулу и прочее. Значения читаются из последнего ответа контроллера.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    const snapshot = await getSettings(ctx.client);
    return {
      ok: true,
      settings: snapshot.settings,
      observed_at: snapshot.observed_at,
    } as ToolResult;
  },
});

export const ocUpdateSettings = defineTool({
  name: 'oc_update_settings',
  title: 'Изменить настройки SSVC',
  description:
    'Отправляет частичное обновление настроек: в теле только изменяемые поля, плоским ' +
    'объектом. Возвращает отчёт контроллера: что принято, что проигнорировано и что ' +
    'отброшено из-за версии API, а также перечитанные настройки. При частичном применении ' +
    'повторять весь запрос нельзя.',
  inputSchema: {
    settings: z
      .record(z.unknown())
      .describe(
        'Плоский объект «ключ → значение»: только изменяемые поля, без обёртки ssvcSettings. ' +
          `Известные прошивке ключи: ${KNOWN_SETTING_KEYS.join(', ')}. ` +
          'Неизвестные ключи не отклоняются локально: прошивка вернёт их в ignored.',
      ),
    strict: z
      .boolean()
      .optional()
      .describe(
        'true — «всё или ничего»: непустые ignored/skipped или partial превращают ответ ' +
          'в validation_failed с перечнем. По умолчанию false, отчёт доносится как есть.',
      ),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args, ctx) => {
    enforceRisk(ctx, 'oc_update_settings', WRITE_RISK, {});

    const rawSettings: unknown = args.settings;
    if (
      typeof rawSettings !== 'object' ||
      rawSettings === null ||
      Array.isArray(rawSettings)
    ) {
      throw new OcError('validation_failed', 'Поле settings должно быть JSON-объектом', {
        hint:
          'Передайте settings плоским объектом: {"hyst": 0.5, "decrement": 2} — без обёртки ' +
          'ssvcSettings и без массива.',
        details: { reason: 'invalid_argument', field: 'settings' },
      });
    }
    const settings = rawSettings as Record<string, unknown>;
    const keys = Object.keys(settings);

    try {
      const outcome = await updateSettings(ctx.client, settings, {
        strict: args.strict === true,
      });

      auditAction(ctx, {
        tool: 'oc_update_settings',
        risk: WRITE_RISK,
        result: outcome.partial ? 'partial' : 'ok',
        details: {
          keys_count: keys.length,
          accepted: outcome.accepted,
          ignored_count: outcome.ignored.length,
          skipped_count: outcome.skipped.length,
          partial: outcome.partial,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        accepted: outcome.accepted,
        ignored: outcome.ignored,
        skipped: outcome.skipped,
        applied: outcome.applied,
        partial: outcome.partial,
      };
      if (outcome.failed.length > 0) payload.failed = outcome.failed;
      if (outcome.message !== null) payload.message = outcome.message;
      if (outcome.applied_error !== null) payload.applied_error = outcome.applied_error;
      if (outcome.partial) payload.hint = PARTIAL_HINT;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_update_settings',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { keys_count: keys.length },
      });
      throw error;
    }
  },
});

/** Инструменты домена: и для `registerSettingsTools`, и для сборки в `src/index.ts`. */
export function settingsTools(): ToolDef[] {
  return [ocGetSettings, ocUpdateSettings];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerSettingsTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, settingsTools());
}
