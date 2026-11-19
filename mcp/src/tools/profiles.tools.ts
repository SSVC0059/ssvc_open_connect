/**
 * Инструменты WP8: профили ректификации (mcp-server-agents.md §12).
 *
 *  - `oc_profiles_list`          — риск 0, список + активный id (`active_id:null`, если 404);
 *  - `oc_profile_get`            — риск 0, полное содержимое профиля;
 *  - `oc_profile_create`         — риск 1, создать профиль (name, content?);
 *  - `oc_profile_copy`           — риск 1, копия существующего профиля;
 *  - `oc_profile_rename`         — риск 1, `PUT /rest/profiles/meta`;
 *  - `oc_profile_update_content` — риск 1, слияние содержимого (merge, не replace);
 *  - `oc_profile_save_current`   — риск 1, текущие настройки контроллера → профиль;
 *  - `oc_profile_apply`          — риск 2, применить профиль (влияет на процесс);
 *  - `oc_profile_delete`         — риск 3, удалить профиль (confirm + reason + флаг).
 *
 * Правила, которые здесь важны:
 *  - `enforceRisk` вызывается до любых сетевых вызовов: `oc_profile_apply` без
 *    `confirm:true` и `oc_profile_delete` без `confirm:true`/`reason` не доходят
 *    до контроллера;
 *  - риск 1 `confirm` не требует, но каждая запись идёт через
 *    `client.withWrite('profiles', …)` в домене и попадает в аудит;
 *  - после записи наружу уходит перечитанное фактическое состояние профиля
 *    (домен), а не эхо аргументов;
 *  - `reason` проверяется `enforceRisk` (≥ 10 символов) и попадает в аудит.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { PROFILE_NAME_MAX_LENGTH } from '../device/schemas/profiles.js';
import {
  applyProfile,
  copyProfile,
  createProfile,
  deleteProfile,
  getProfileContent,
  listProfiles,
  renameProfile,
  saveCurrentSettings,
  updateProfileContent,
} from '../domain/profiles.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk, MIN_REASON_LENGTH } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const WRITE_RISK: RiskClass = 1;
const APPLY_RISK: RiskClass = 2;
const DELETE_RISK: RiskClass = 3;

const ID_FIELD_DESCRIPTION =
  'Идентификатор профиля из oc_profiles_list, например p1. Проверяется локально: ' +
  'пустая строка отклоняется без запроса к контроллеру.';

const NAME_FIELD_DESCRIPTION =
  `Отображаемое название профиля: непустое, до ${PROFILE_NAME_MAX_LENGTH} символов. ` +
  'Прошивка проверяет только непустоту; предел задан сервером, чтобы имя оставалось читаемым.';

const CONFIRM_FIELD_DESCRIPTION =
  'Явное подтверждение действия (риск 2). Без confirm:true контроллер не получает запрос, ' +
  'профиль не применяется.';

const DELETE_CONFIRM_DESCRIPTION =
  'Явное подтверждение удаления (риск 3). Без confirm:true запрос отклоняется локально, ' +
  'профиль не удаляется.';

const REASON_FIELD_DESCRIPTION =
  `Причина удаления не короче ${MIN_REASON_LENGTH} символов: попадает в журнал аудита. ` +
  'Без причины удаление отклоняется (validation_failed).';

const CONTENT_FIELD_DESCRIPTION =
  'Содержимое профиля JSON-объектом: ssvcSettings, alarmThresholds, telegram, vk — ' +
  'набор ключей задаёт прошивка. Массив или скаляр прошивка отвергнет с 400.';

export const ocProfilesList = defineTool({
  name: 'oc_profiles_list',
  title: 'Список профилей',
  description:
    'Возвращает профили ректификации с идентификаторами, названиями и признаком активного.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx: ToolContext) => {
    const snapshot = await listProfiles(ctx.client);
    return {
      ok: true,
      profiles: snapshot.profiles,
      active_id: snapshot.active_id,
      observed_at: snapshot.observed_at,
    } as ToolResult;
  },
});

export const ocProfileGet = defineTool({
  name: 'oc_profile_get',
  title: 'Профиль целиком',
  description:
    'Возвращает полное содержимое профиля: параметры сырца, фракции, настройки SSVC и ' +
    'правила реле.',
  inputSchema: {
    id: z.string().min(1).describe(ID_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(READ_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const result = await getProfileContent(ctx.client, args.id);
    return {
      ok: true,
      id: result.id,
      profile: result.profile,
      observed_at: result.observed_at,
    } as ToolResult;
  },
});

export const ocProfileCreate = defineTool({
  name: 'oc_profile_create',
  title: 'Создать профиль',
  description:
    'Создаёт новый профиль с указанным названием и, при необходимости, содержимым.',
  inputSchema: {
    name: z.string().min(1).max(PROFILE_NAME_MAX_LENGTH).describe(NAME_FIELD_DESCRIPTION),
    content: z.record(z.unknown()).optional().describe(CONTENT_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_profile_create', WRITE_RISK, {});
    try {
      const outcome = await createProfile(ctx.client, {
        name: args.name,
        content: args.content,
      });

      auditAction(ctx, {
        tool: 'oc_profile_create',
        risk: WRITE_RISK,
        result: 'ok',
        details: {
          id: outcome.id,
          name_length: typeof args.name === 'string' ? args.name.length : null,
          has_content: args.content !== undefined && args.content !== null,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        id: outcome.id,
        profile: outcome.profile,
        observed_at: outcome.observed_at,
      };
      if (outcome.warning !== null) payload.warning = outcome.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_profile_create',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { name_length: typeof args.name === 'string' ? args.name.length : null },
      });
      throw error;
    }
  },
});

export const ocProfileCopy = defineTool({
  name: 'oc_profile_copy',
  title: 'Скопировать профиль',
  description: 'Создаёт копию существующего профиля под новым названием.',
  inputSchema: {
    source_id: z.string().min(1).describe(`Исходный профиль. ${ID_FIELD_DESCRIPTION}`),
    new_name: z
      .string()
      .min(1)
      .max(PROFILE_NAME_MAX_LENGTH)
      .describe(`Название копии. ${NAME_FIELD_DESCRIPTION}`),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_profile_copy', WRITE_RISK, {});
    try {
      const outcome = await copyProfile(ctx.client, {
        sourceId: args.source_id,
        newName: args.new_name,
      });

      auditAction(ctx, {
        tool: 'oc_profile_copy',
        risk: WRITE_RISK,
        result: 'ok',
        details: {
          source_id: typeof args.source_id === 'string' ? args.source_id : null,
          id: outcome.id,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        id: outcome.id,
        profile: outcome.profile,
        observed_at: outcome.observed_at,
      };
      if (outcome.warning !== null) payload.warning = outcome.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_profile_copy',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: {
          source_id: typeof args.source_id === 'string' ? args.source_id : null,
        },
      });
      throw error;
    }
  },
});

export const ocProfileRename = defineTool({
  name: 'oc_profile_rename',
  title: 'Переименовать профиль',
  description: 'Меняет отображаемое название профиля.',
  inputSchema: {
    id: z.string().min(1).describe(ID_FIELD_DESCRIPTION),
    name: z.string().min(1).max(PROFILE_NAME_MAX_LENGTH).describe(NAME_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_profile_rename', WRITE_RISK, {});
    try {
      const outcome = await renameProfile(ctx.client, { id: args.id, name: args.name });

      auditAction(ctx, {
        tool: 'oc_profile_rename',
        risk: WRITE_RISK,
        result: 'ok',
        details: {
          id: outcome.id,
          name: typeof outcome.state.profile?.name === 'string' ? outcome.state.profile.name : null,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        id: outcome.id,
        profile: outcome.state.profile,
        observed_at: outcome.state.observed_at,
      };
      if (outcome.state.warning !== null) payload.warning = outcome.state.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_profile_rename',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { id: typeof args.id === 'string' ? args.id : null },
      });
      throw error;
    }
  },
});

export const ocProfileUpdateContent = defineTool({
  name: 'oc_profile_update_content',
  title: 'Обновить содержимое профиля',
  description:
    'Сливает переданные ключи с содержимым профиля: верхнеуровневые ключи заменяются или добавляются, ' +
    'ключ со значением null удаляется. Остальные настройки профиля сохраняются — это не замена файла целиком.',
  inputSchema: {
    id: z.string().min(1).describe(ID_FIELD_DESCRIPTION),
    content: z.record(z.unknown()).describe(CONTENT_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_profile_update_content', WRITE_RISK, {});
    try {
      const outcome = await updateProfileContent(ctx.client, {
        id: args.id,
        content: args.content,
      });

      auditAction(ctx, {
        tool: 'oc_profile_update_content',
        risk: WRITE_RISK,
        result: 'ok',
        details: {
          id: outcome.id,
          keys_count:
            typeof args.content === 'object' && args.content !== null
              ? Object.keys(args.content as Record<string, unknown>).length
              : null,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        id: outcome.id,
        profile: outcome.state.profile,
        observed_at: outcome.state.observed_at,
      };
      if (outcome.state.warning !== null) payload.warning = outcome.state.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_profile_update_content',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { id: typeof args.id === 'string' ? args.id : null },
      });
      throw error;
    }
  },
});

export const ocProfileSaveCurrent = defineTool({
  name: 'oc_profile_save_current',
  title: 'Сохранить текущие настройки в профиль',
  description: 'Записывает действующие настройки контроллера в указанный профиль.',
  inputSchema: {
    id: z.string().min(1).describe(ID_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_profile_save_current', WRITE_RISK, {});
    try {
      const outcome = await saveCurrentSettings(ctx.client, { id: args.id });

      auditAction(ctx, {
        tool: 'oc_profile_save_current',
        risk: WRITE_RISK,
        result: 'ok',
        details: { id: outcome.id },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        id: outcome.id,
        profile: outcome.state.profile,
        observed_at: outcome.state.observed_at,
      };
      if (outcome.state.warning !== null) payload.warning = outcome.state.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_profile_save_current',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { id: typeof args.id === 'string' ? args.id : null },
      });
      throw error;
    }
  },
});

export const ocProfileApply = defineTool({
  name: 'oc_profile_apply',
  title: 'Применить профиль',
  description:
    'Делает профиль активным и применяет его настройки к контроллеру. Влияет на процесс ' +
    'отбора.',
  inputSchema: {
    id: z.string().min(1).describe(ID_FIELD_DESCRIPTION),
    confirm: z.boolean().optional().describe(CONFIRM_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(APPLY_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    // До любых сетевых вызовов: «забытый confirm» не должен применять профиль.
    enforceRisk(ctx, 'oc_profile_apply', APPLY_RISK, { confirm: args.confirm });

    try {
      const outcome = await applyProfile(ctx.client, { id: args.id });

      auditAction(ctx, {
        tool: 'oc_profile_apply',
        risk: APPLY_RISK,
        confirm: true,
        result: 'ok',
        details: { id: typeof args.id === 'string' ? args.id : null, applied_id: outcome.applied_id },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        applied_id: outcome.applied_id,
        observed_at: outcome.observed_at,
      };
      if (outcome.warning !== null) payload.warning = outcome.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_profile_apply',
        risk: APPLY_RISK,
        confirm: args.confirm === true,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { id: typeof args.id === 'string' ? args.id : null },
      });
      throw error;
    }
  },
});

export const ocProfileDelete = defineTool({
  name: 'oc_profile_delete',
  title: 'Удалить профиль',
  description:
    'Удаляет профиль без возможности восстановления. Требует подтверждения и причины.',
  inputSchema: {
    id: z.string().min(1).describe(ID_FIELD_DESCRIPTION),
    confirm: z.boolean().optional().describe(DELETE_CONFIRM_DESCRIPTION),
    reason: z.string().optional().describe(REASON_FIELD_DESCRIPTION),
  },
  annotations: annotationsForRisk(DELETE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    // Риск 3: confirm → reason (≥10) → SSVC_ALLOW_DESTRUCTIVE. Всё — до сети.
    enforceRisk(ctx, 'oc_profile_delete', DELETE_RISK, {
      confirm: args.confirm,
      reason: args.reason,
    });

    try {
      const outcome = await deleteProfile(ctx.client, { id: args.id });

      auditAction(ctx, {
        tool: 'oc_profile_delete',
        risk: DELETE_RISK,
        confirm: true,
        reason: typeof args.reason === 'string' ? args.reason : undefined,
        result: 'ok',
        details: { deleted_id: outcome.deleted_id, verified_absent: outcome.verified_absent },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        deleted_id: outcome.deleted_id,
        verified_absent: outcome.verified_absent,
        observed_at: outcome.observed_at,
      };
      if (outcome.warning !== null) payload.warning = outcome.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_profile_delete',
        risk: DELETE_RISK,
        confirm: args.confirm === true,
        reason: typeof args.reason === 'string' ? args.reason : undefined,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { id: typeof args.id === 'string' ? args.id : null },
      });
      throw error;
    }
  },
});

/** Инструменты домена: и для `registerProfilesTools`, и для сборки в `src/index.ts`. */
export function profilesTools(): ToolDef[] {
  return [
    ocProfilesList,
    ocProfileGet,
    ocProfileCreate,
    ocProfileCopy,
    ocProfileRename,
    ocProfileUpdateContent,
    ocProfileSaveCurrent,
    ocProfileApply,
    ocProfileDelete,
  ];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerProfilesTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, profilesTools());
}
