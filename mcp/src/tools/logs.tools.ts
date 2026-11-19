/**
 * Инструменты WP7: журналы ректификации (mcp-server-agents.md §11).
 *
 *  - `oc_logs_list`     — риск 0, `GET /rest/logs`: снимок сессии и список файлов.
 *                         `refresh:true` — принудительно запросить список заново;
 *                         `409 log_transfer_busy` — не ошибка, а `status:"busy"`.
 *  - `oc_logs_download` — риск 0, `GET /rest/logs/{id}`: 202 → поллинг → 200 CSV →
 *                         файл на сервере. Полный CSV в ответ не попадает.
 *  - `oc_logs_cancel`   — риск 1, `DELETE /rest/logs`: освобождает сессию передачи.
 *
 * Скачивание идёт через `withWrite('logs', …)` в домене: одна сессия передачи на
 * устройство (mcp-server.md §2.5 п.6). Описания инструментов — дословно из §11.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { cancelLogs, DEFAULT_SAVE_DIR, downloadLog, listLogs } from '../domain/logs.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const CANCEL_RISK: RiskClass = 1;

export const ocLogsList = defineTool({
  name: 'oc_logs_list',
  title: 'Список журналов',
  description:
    'Запрашивает у контроллера список доступных журналов ректификации. Контроллер отдаёт ' +
    'список не мгновенно, поэтому при статусе receiving повторите вызов.',
  inputSchema: {
    refresh: z
      .boolean()
      .optional()
      .describe(
        'true — принудительно запросить список заново (?refresh=1). Без параметра контроллер ' +
          'отдаёт текущий снимок сессии, а из idle сам ставит запрос списка.',
      ),
  },
  annotations: annotationsForRisk(READ_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const snapshot = await listLogs(ctx.client, { refresh: args.refresh === true });
    const payload: Record<string, unknown> = {
      ok: true,
      status: snapshot.status,
      files: snapshot.files,
      total: snapshot.total,
      received: snapshot.received,
      observed_at: snapshot.observed_at,
    };
    // Код ошибки прошивки (например, log_transfer_busy при status:"busy") полезен агенту.
    if (snapshot.error !== null) payload.error = snapshot.error;
    return payload as ToolResult;
  },
});

export const ocLogsDownload = defineTool({
  name: 'oc_logs_download',
  title: 'Скачать журнал',
  description:
    'Скачивает журнал по номеру процесса и сохраняет его в файл на сервере, возвращая путь, ' +
    'размер и краткий фрагмент. Полный CSV в ответ не попадает, чтобы не переполнять контекст.',
  inputSchema: {
    process_id: z
      .number()
      .int()
      .min(1)
      .describe(
        'Номер процесса, целое ≥ 1. Возьмите его из oc_logs_list: файл «12.CSV» — процесс 12.',
      ),
    save_dir: z
      .string()
      .optional()
      .describe(
        `Каталог сохранения относительно рабочего каталога сервера (по умолчанию ${DEFAULT_SAVE_DIR}). ` +
          'Абсолютные пути и выход за пределы рабочего каталога запрещены.',
      ),
  },
  annotations: annotationsForRisk(READ_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const result = await downloadLog(ctx.client, args.process_id as number, {
      saveDir: typeof args.save_dir === 'string' ? args.save_dir : undefined,
    });
    ctx.log.info('Журнал сохранён в файл', {
      process_id: args.process_id,
      bytes: result.bytes,
      lines: result.lines,
    });
    return { ok: true, ...result } as ToolResult;
  },
});

export const ocLogsCancel = defineTool({
  name: 'oc_logs_cancel',
  title: 'Отменить передачу журнала',
  description:
    'Прерывает текущую передачу журнала с контроллера и освобождает сессию передачи.',
  inputSchema: {},
  annotations: annotationsForRisk(CANCEL_RISK),
  handler: async (_args, ctx) => {
    enforceRisk(ctx, 'oc_logs_cancel', CANCEL_RISK, {});
    try {
      const result = await cancelLogs(ctx.client);
      auditAction(ctx, {
        tool: 'oc_logs_cancel',
        risk: CANCEL_RISK,
        result: 'ok',
        details: { status: result.status },
      });
      return {
        ok: true,
        status: result.status,
        observed_at: result.observed_at,
      } as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_logs_cancel',
        risk: CANCEL_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
      });
      throw error;
    }
  },
});

/** Инструменты домена: и для `registerLogsTools`, и для сборки в `src/index.ts`. */
export function logsTools(): ToolDef[] {
  return [ocLogsList, ocLogsDownload, ocLogsCancel];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerLogsTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, logsTools());
}
