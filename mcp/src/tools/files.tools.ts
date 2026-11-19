/**
 * Инструменты WP9: файлы контроллера (mcp-server-agents.md §13).
 *
 *  - `oc_files_list`     — риск 0, дерево `GET /rest/files` (или его подкаталог);
 *  - `oc_file_download`  — риск 0, `GET /rest/files{path}` → файл на сервере,
 *                          наружу только путь и размер (содержимое, особенно
 *                          бинарное, в контекст модели не попадает);
 *  - `oc_file_copy`      — риск 1, `POST /rest/files/copy` внутри контроллера;
 *  - `oc_file_delete`    — риск 3, `DELETE /rest/files{path}`: confirm + reason
 *                          (≥10 символов) + SSVC_ALLOW_DESTRUCTIVE=1.
 *
 * Пути валидируются локально до сети (`validation_failed`): `..`, управляющие
 * символы, обратные слэши и относительные пути до прошивки не доходят. Удаление
 * после успеха подтверждается перечитанным деревом (`verified_absent`).
 *
 * Описания инструментов — дословно из §13.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import {
  copyFile,
  DEFAULT_DOWNLOAD_DIR,
  deleteFile,
  downloadFile,
  listFiles,
} from '../domain/files.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk, MIN_REASON_LENGTH } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const WRITE_RISK: RiskClass = 1;
const DELETE_RISK: RiskClass = 3;

const PATH_FIELD_DESCRIPTION =
  'Абсолютный путь на контроллере, начиная с «/», например /config/app.json. ' +
  'Проверяется локально: «..», управляющие символы и относительные пути отклоняются ' +
  'без запроса к устройству.';

const SAVE_DIR_FIELD_DESCRIPTION =
  `Каталог сохранения относительно рабочего каталога сервера (по умолчанию ${DEFAULT_DOWNLOAD_DIR}). ` +
  'Абсолютные пути и выход за пределы рабочего каталога запрещены.';

const DELETE_CONFIRM_DESCRIPTION =
  'Явное подтверждение удаления (риск 3). Без confirm:true запрос отклоняется локально, ' +
  'файл не удаляется.';

const REASON_FIELD_DESCRIPTION =
  `Причина удаления не короче ${MIN_REASON_LENGTH} символов: попадает в журнал аудита. ` +
  'Без причины удаление отклоняется (validation_failed).';

export const ocFilesList = defineTool({
  name: 'oc_files_list',
  title: 'Файлы контроллера',
  description: 'Возвращает дерево файлов и каталогов на контроллере.',
  inputSchema: {
    path: z.string().optional().describe(
      'Каталог, содержимое которого нужно показать, например /config. Без параметра — ' +
        'всё дерево от корня. Путь должен начинаться с «/».',
    ),
  },
  annotations: annotationsForRisk(READ_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const snapshot = await listFiles(ctx.client, { path: args.path });
    return {
      ok: true,
      path: snapshot.path,
      tree: snapshot.tree,
      observed_at: snapshot.observed_at,
    } as ToolResult;
  },
});

export const ocFileDownload = defineTool({
  name: 'oc_file_download',
  title: 'Скачать файл',
  description:
    'Скачивает файл с контроллера и сохраняет его на сервере, возвращая путь и размер.',
  inputSchema: {
    path: z.string().describe(PATH_FIELD_DESCRIPTION),
    save_dir: z.string().optional().describe(SAVE_DIR_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(READ_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const result = await downloadFile(ctx.client, args.path, {
      saveDir: typeof args.save_dir === 'string' ? args.save_dir : undefined,
    });
    ctx.log.info('Файл контроллера сохранён', {
      path: result.path,
      bytes: result.bytes,
    });
    return {
      ok: true,
      path: result.path,
      saved_path: result.saved_path,
      bytes: result.bytes,
      content_type: result.content_type,
      observed_at: result.observed_at,
    } as ToolResult;
  },
});

export const ocFileCopy = defineTool({
  name: 'oc_file_copy',
  title: 'Скопировать файл',
  description: 'Копирует файл внутри контроллера.',
  inputSchema: {
    source: z.string().describe(`Исходный файл. ${PATH_FIELD_DESCRIPTION}`),
    destination: z
      .string()
      .describe(
        `Путь нового файла на контроллере. ${PATH_FIELD_DESCRIPTION} ` +
          'Существующий файл назначения будет перезаписан прошивкой.',
      ),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_file_copy', WRITE_RISK, {});
    try {
      const result = await copyFile(ctx.client, {
        source: args.source,
        destination: args.destination,
      });
      auditAction(ctx, {
        tool: 'oc_file_copy',
        risk: WRITE_RISK,
        result: 'ok',
        details: { source: result.source, destination: result.destination },
      });
      return {
        ok: true,
        source: result.source,
        destination: result.destination,
        observed_at: result.observed_at,
      } as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_file_copy',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: {
          source: typeof args.source === 'string' ? args.source : null,
          destination: typeof args.destination === 'string' ? args.destination : null,
        },
      });
      throw error;
    }
  },
});

export const ocFileDelete = defineTool({
  name: 'oc_file_delete',
  title: 'Удалить файл',
  description:
    'Удаляет файл на контроллере без возможности восстановления. Требует подтверждения и ' +
    'причины.',
  inputSchema: {
    path: z.string().describe(PATH_FIELD_DESCRIPTION),
    confirm: z.boolean().optional().describe(DELETE_CONFIRM_DESCRIPTION),
    reason: z.string().optional().describe(REASON_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(DELETE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    // Риск 3: confirm → reason (≥10) → SSVC_ALLOW_DESTRUCTIVE. Всё — до сети.
    enforceRisk(ctx, 'oc_file_delete', DELETE_RISK, {
      confirm: args.confirm,
      reason: args.reason,
    });

    try {
      const result = await deleteFile(ctx.client, { path: args.path });
      auditAction(ctx, {
        tool: 'oc_file_delete',
        risk: DELETE_RISK,
        confirm: true,
        reason: typeof args.reason === 'string' ? args.reason : undefined,
        result: 'ok',
        details: {
          path: result.path,
          verified_absent: result.verified_absent,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        path: result.path,
        verified_absent: result.verified_absent,
        observed_at: result.observed_at,
      };
      if (result.warning !== null) payload.warning = result.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_file_delete',
        risk: DELETE_RISK,
        confirm: args.confirm === true,
        reason: typeof args.reason === 'string' ? args.reason : undefined,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { path: typeof args.path === 'string' ? args.path : null },
      });
      throw error;
    }
  },
});

/** Инструменты домена: и для `registerFilesTools`, и для сборки в `src/index.ts`. */
export function filesTools(): ToolDef[] {
  return [ocFilesList, ocFileDownload, ocFileCopy, ocFileDelete];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerFilesTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, filesTools());
}
