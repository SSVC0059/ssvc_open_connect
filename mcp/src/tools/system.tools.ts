/**
 * Инструменты WP9: подсистемы и система (mcp-server-agents.md §13).
 *
 *  - `oc_get_subsystems` — риск 0, `GET /rest/subsystem` — карта bool;
 *  - `oc_set_subsystem`  — риск 1, `PUT /rest/subsystem` с таймаутом 120 с:
 *                          Telegram/VK включаются дольше минуты. Даже при
 *                          `success:false` инструмент перечитывает карту и
 *                          возвращает `errors[]`, а не бросает исключение;
 *  - `oc_system_status`  — риск 0, `GET /rest/systemStatus` в нормализованном виде;
 *  - `oc_restart`        — риск 3, `POST /rest/restart`: confirm + reason
 *                          (≥10 символов) + SSVC_ALLOW_DESTRUCTIVE=1.
 *
 * Описания инструментов — дословно из §13.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import {
  getSubsystems,
  getSystemStatus,
  restartController,
  setSubsystem,
  SUBSYSTEM_TIMEOUT_MS,
} from '../domain/system.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk, MIN_REASON_LENGTH } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const WRITE_RISK: RiskClass = 1;
const RESTART_RISK: RiskClass = 3;

const NAME_FIELD_DESCRIPTION =
  'Имя подсистемы — ключ карты oc_get_subsystems: thermal, i2c_bus, atm_sensor, ' +
  'lcd1602_display, xkc25_sensor, telegram_bot, vk_bot, settings. Неизвестное имя ' +
  'контроллер вернёт в errors[], а не применит.';

const ENABLED_FIELD_DESCRIPTION =
  'Целевое состояние: true — включить, false — выключить. Прошивка трактует ' +
  'не-bool значение как false, поэтому поле обязательное и строго boolean.';

const CONFIRM_FIELD_DESCRIPTION =
  'Явное подтверждение перезагрузки (риск 3). Без confirm:true запрос отклоняется ' +
  'локально, контроллер не перезагружается.';

const REASON_FIELD_DESCRIPTION =
  `Причина перезагрузки не короче ${MIN_REASON_LENGTH} символов: попадает в журнал аудита. ` +
  'Без причины перезагрузка отклоняется (validation_failed).';

export const ocGetSubsystems = defineTool({
  name: 'oc_get_subsystems',
  title: 'Состояние подсистем',
  description:
    'Возвращает, какие подсистемы контроллера включены: I²C, тепловая, атмосферная, ' +
    'Telegram, VK, настройки.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx: ToolContext) => {
    const snapshot = await getSubsystems(ctx.client);
    return {
      ok: true,
      subsystems: snapshot.systems,
      observed_at: snapshot.observed_at,
    } as ToolResult;
  },
});

export const ocSetSubsystem = defineTool({
  name: 'oc_set_subsystem',
  title: 'Включить или выключить подсистему',
  description:
    'Переключает подсистему контроллера. Некоторые подсистемы (Telegram, VK) включаются ' +
    'дольше минуты — инструмент ждёт до двух минут. ВНИМАНИЕ: выключение safety-подсистем ' +
    '(thermal — тепловая защита, xkc25_sensor — предохранитель старта по уровню) влияет на ' +
    'безопасность процесса. Инструмент не требует confirm по политике (риск 1), поэтому ' +
    'проверяйте такие вызовы особенно внимательно.',
  inputSchema: {
    name: z.string().describe(NAME_FIELD_DESCRIPTION),
    enabled: z.boolean().describe(ENABLED_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_set_subsystem', WRITE_RISK, {});
    try {
      const result = await setSubsystem(
        ctx.client,
        { name: args.name, enabled: args.enabled },
        { timeoutMs: SUBSYSTEM_TIMEOUT_MS },
      );

      // success:false — это отчёт устройства, а не исключение: агент должен видеть
      // errors[] и фактическую карту. В аудит пишем оба исхода.
      auditAction(ctx, {
        tool: 'oc_set_subsystem',
        risk: WRITE_RISK,
        result: result.success ? 'ok' : 'device_rejected',
        details: {
          name: result.name,
          enabled: result.enabled,
          errors: result.errors.length,
          reread: result.subsystems !== null,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        name: result.name,
        enabled: result.enabled,
        success: result.success,
        errors: result.errors,
        subsystems: result.subsystems,
        observed_at: result.observed_at,
      };
      if (result.warning !== null) payload.warning = result.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_set_subsystem',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: {
          name: typeof args.name === 'string' ? args.name : null,
          enabled: typeof args.enabled === 'boolean' ? args.enabled : null,
        },
      });
      throw error;
    }
  },
});

export const ocSystemStatus = defineTool({
  name: 'oc_system_status',
  title: 'Состояние системы',
  description:
    'Возвращает состояние контроллера: свободную память, время работы, версию прошивки и ' +
    'данные чипа.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx: ToolContext) => {
    const snapshot = await getSystemStatus(ctx.client);
    return {
      ok: true,
      system: snapshot.system,
      observed_at: snapshot.observed_at,
    } as ToolResult;
  },
});

export const ocRestart = defineTool({
  name: 'oc_restart',
  title: 'Перезагрузить контроллер',
  description:
    'Перезагружает контроллер. Текущий процесс ректификации будет прерван; требует ' +
    'подтверждения и причины.',
  inputSchema: {
    confirm: z.boolean().optional().describe(CONFIRM_FIELD_DESCRIPTION),
    reason: z.string().optional().describe(REASON_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(RESTART_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    // Риск 3: confirm → reason (≥10) → SSVC_ALLOW_DESTRUCTIVE. Всё — до сети.
    enforceRisk(ctx, 'oc_restart', RESTART_RISK, {
      confirm: args.confirm,
      reason: args.reason,
    });

    try {
      const result = await restartController(ctx.client);
      auditAction(ctx, {
        tool: 'oc_restart',
        risk: RESTART_RISK,
        confirm: true,
        reason: typeof args.reason === 'string' ? args.reason : undefined,
        result: 'ok',
      });
      return {
        ok: true,
        hint: result.hint,
        observed_at: result.observed_at,
      } as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_restart',
        risk: RESTART_RISK,
        confirm: args.confirm === true,
        reason: typeof args.reason === 'string' ? args.reason : undefined,
        result: error instanceof OcError ? error.code : 'internal_error',
      });
      throw error;
    }
  },
});

/** Инструменты домена: и для `registerSystemTools`, и для сборки в `src/index.ts`. */
export function systemTools(): ToolDef[] {
  return [ocGetSubsystems, ocSetSubsystem, ocSystemStatus, ocRestart];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerSystemTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, systemTools());
}
