/**
 * Инструменты WP5: аппаратная конфигурация и сбои (mcp-server-agents.md §9).
 *
 *  - `oc_get_hardware_config`    — риск 0, конфигурация + capabilities + needs_reboot + пробы;
 *  - `oc_update_hardware_config` — риск 1 (обратимая запись), ЧАСТИЧНЫЙ объект без обёртки,
 *                                  запись через `client.withWrite('hardware', …)`;
 *  - `oc_get_hardware_faults`    — риск 0, активные сбои и список ответивших адресов I²C.
 *
 * Риск 1 не требует `confirm`, но каждая запись пишется в лог и аудит. Если прошивка
 * сообщает `needsReboot:true`, ответ инструмента содержит `hint` про `oc_restart`;
 * сам инструмент контроллер не перезагружает.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import {
  getHardwareConfig,
  getHardwareFaults,
  updateHardwareConfig,
} from '../domain/hardware.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const WRITE_RISK: RiskClass = 1;

export const ocGetHardwareConfig = defineTool({
  name: 'oc_get_hardware_config',
  title: 'Аппаратная конфигурация',
  description:
    'Возвращает настройки подключённого оборудования: датчик давления, RTC, дисплей, ' +
    'реле, датчики уровня, а также результаты проб I²C и признак необходимости перезагрузки.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    const snapshot = await getHardwareConfig(ctx.client);
    return { ok: true, ...snapshot } as ToolResult;
  },
});

export const ocUpdateHardwareConfig = defineTool({
  name: 'oc_update_hardware_config',
  title: 'Изменить аппаратную конфигурацию',
  description:
    'Частично обновляет настройки оборудования. Изменения, требующие перезагрузки, ' +
    'отмечаются в needs_reboot; применяются после перезапуска контроллера.',
  inputSchema: {
    config: z
      .record(z.unknown())
      .describe(
        'Частичный объект настроек в формате oc_get_hardware_config: например ' +
          '{"pressureSensorEnabled": true, "lcd1602Enabled": false, ' +
          '"relayPcf8574Addresses": [36]}. Уходит на контроллер как есть, без обёртки; ' +
          'неуказанные поля не меняются.',
      ),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_update_hardware_config', WRITE_RISK, {});

    const keys =
      typeof args.config === 'object' && args.config !== null && !Array.isArray(args.config)
        ? Object.keys(args.config)
        : [];

    try {
      const snapshot = await updateHardwareConfig(ctx.client, args.config);

      auditAction(ctx, {
        tool: 'oc_update_hardware_config',
        risk: WRITE_RISK,
        result: 'ok',
        details: {
          keys_count: keys.length,
          keys,
          needs_reboot: snapshot.needs_reboot,
        },
      });

      const payload: Record<string, unknown> = { ok: true, ...snapshot };
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_update_hardware_config',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { keys_count: keys.length, keys },
      });
      throw error;
    }
  },
});

export const ocGetHardwareFaults = defineTool({
  name: 'oc_get_hardware_faults',
  title: 'Аппаратные сбои',
  description:
    'Возвращает активные сбои оборудования (I²C NACK, шина недоступна, устройство не ' +
    'найдено, потеря UART) и список ответивших адресов I²C для диагностики.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    const snapshot = await getHardwareFaults(ctx.client);
    return { ok: true, ...snapshot } as ToolResult;
  },
});

/** Инструменты домена: и для `registerHardwareTools`, и для сборки в `src/index.ts`. */
export function hardwareTools(): ToolDef[] {
  return [ocGetHardwareConfig, ocUpdateHardwareConfig, ocGetHardwareFaults];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerHardwareTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, hardwareTools());
}
