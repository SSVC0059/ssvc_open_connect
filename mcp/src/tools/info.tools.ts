/**
 * Инструменты WP1: информация, возможности и диагностика MCP-сервера.
 *
 * Все три — риск 0 (чтение), подтверждение не требуется.
 *  - `oc_get_info`         — `GET /rest/oc/info` (через capability-кэш 60 с);
 *  - `oc_get_capabilities` — `/rest/oc/info` + `/rest/features` (`include_runtime`);
 *  - `oc_diagnostics`      — состояние самого сервера, БЕЗ обращений к контроллеру.
 *
 * Каждый домен экспортирует `registerX(server, ctx)` — единая точка подключения для
 * WP11; `infoTools()` возвращает тот же список `ToolDef`, который регистрируется, и
 * используется в `src/index.ts` (сервер создаётся на сессию внутри `http/app.ts`,
 * поэтому `startService` принимает именно список определений).
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { CAPABILITY_TTL_MS } from '../device/CapabilityCache.js';
import {
  READ_ONLY_SETTING_KEYS,
  WRITABLE_SETTING_KEYS,
} from '../device/schemas/settings.js';
import {
  buildCapabilities,
  loadRuntimeFeatures,
  mapInfo,
} from '../domain/info.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef } from '../mcp/types.js';
import { annotationsForRisk } from '../policy/risk.js';

const RISK = 0 as const;

export const ocGetInfo = defineTool({
  name: 'oc_get_info',
  title: 'Информация о контроллере',
  description:
    'Возвращает версию прошивки OpenConnect, версию и режим UART API контроллера SSVC и ' +
    'совместимость. Используйте перед любыми действиями, чтобы понять, какие функции доступны.',
  inputSchema: {},
  annotations: annotationsForRisk(RISK),
  handler: async (_args, ctx) => {
    const info = await ctx.caps.get();
    const view = mapInfo(info);
    return {
      ok: true,
      device: view.device,
      api_features: view.api_features,
      unavailable: view.unavailable,
    };
  },
});

export const ocGetCapabilities = defineTool({
  name: 'oc_get_capabilities',
  title: 'Доступные функции устройства',
  description:
    'Возвращает реестр возможностей UART API (predec, extended_heads, get_log и другие), ' +
    'рантайм-флаги прошивки и реестр ключей настроек: какие записываются, а какие доступны ' +
    'только на чтение. Показывает, что именно поддерживает подключённый контроллер; ' +
    'недоступные функции перечислены явно.',
  inputSchema: {
    include_runtime: z
      .boolean()
      .optional()
      .describe('Читать рантайм-флаги прошивки (GET /rest/features); по умолчанию true'),
  },
  annotations: annotationsForRisk(RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const includeRuntime = args.include_runtime !== false;
    const info = await ctx.caps.get();
    const runtime = includeRuntime ? await loadRuntimeFeatures(ctx.client) : null;
    const view = buildCapabilities(info, runtime);
    return {
      ok: true,
      include_runtime: includeRuntime,
      api_features: view.api_features,
      unavailable: view.unavailable,
      features: view.features,
      features_meta: view.features_meta,
      settings: {
        writable: [...WRITABLE_SETTING_KEYS],
        readonly: [...READ_ONLY_SETTING_KEYS],
      },
    };
  },
});

export const ocDiagnostics = defineTool({
  name: 'oc_diagnostics',
  title: 'Диагностика MCP-сервера',
  description:
    'Возвращает состояние самого MCP-сервера: конфигурацию без секретов, статус авторизации ' +
    'на контроллере, возраст кэша возможностей, счётчики запросов и последние события аудита. ' +
    'К контроллеру не обращается.',
  inputSchema: {},
  annotations: annotationsForRisk(RISK),
  handler: async (_args, ctx) => {
    const stats = ctx.client.getStats();
    const auth = stats.auth;
    const authStatus = auth.securityOff ? 'disabled' : auth.authenticated ? 'ok' : 'unknown';
    const cachedInfo = ctx.caps.peek();
    const lastAudit = await ctx.audit.last(10);
    const sessionsSeen = new Set(lastAudit.map((record) => record.session));

    return {
      ok: true,
      // Секреты (пароль контроллера, токен MCP-клиентов) сюда не попадают никогда.
      config: {
        device: {
          url: ctx.cfg.device.url,
          user: ctx.cfg.device.user,
          timeout_ms: ctx.cfg.device.timeoutMs,
        },
        mcp: {
          host: ctx.cfg.mcp.host,
          port: ctx.cfg.mcp.port,
          allowed_hosts: ctx.cfg.mcp.allowedHosts,
          allowed_cidrs: ctx.cfg.mcp.allowedCidrs,
          tls: ctx.cfg.mcp.tls !== null,
          max_sessions: ctx.cfg.mcp.maxSessions,
          idle_ms: ctx.cfg.mcp.idleMs,
        },
        raw: {
          mode: ctx.cfg.raw.mode,
          dir: ctx.cfg.raw.dir,
          max_mb: ctx.cfg.raw.maxMb,
        },
        allow_destructive: ctx.cfg.allowDestructive,
        audit_path: ctx.cfg.auditPath,
        log_level: ctx.cfg.logLevel,
      },
      auth: {
        status: authStatus,
        authenticated: auth.authenticated,
        security_off: auth.securityOff,
        sign_ins: auth.signIns,
        relogins: auth.relogins,
        token_age_ms: auth.tokenAgeMs,
        last_sign_in_at: auth.lastSignInAt,
      },
      capability_cache: {
        age_ms: ctx.caps.ageMs(),
        ttl_ms: CAPABILITY_TTL_MS,
        has_data: cachedInfo !== null,
        firmware: cachedInfo?.oc.version ?? null,
        ssvc_version: cachedInfo?.ssvc.version ?? null,
        api: cachedInfo?.ssvc.api ?? null,
      },
      counters: {
        requests: stats.requests,
        retries: stats.retries,
        auth_retries: stats.authRetries,
        write_operations: stats.writeOperations,
        errors: stats.errors,
        last_status: stats.lastStatus,
        last_request_at: stats.lastRequestAt,
        writes: { pending: stats.writes.size, completed: stats.writes.completed },
      },
      sessions: (() => {
        const snapshot = ctx.sessions?.();
        return {
          current: ctx.audit.session,
          // В stdio HTTP-сессий нет вовсе: null означает «неприменимо», а не «неизвестно».
          active: snapshot?.active ?? null,
          max: snapshot?.max ?? null,
          entries: snapshot?.entries ?? [],
          seen_in_audit: sessionsSeen.size,
          note:
            snapshot === undefined
              ? 'Транспорт stdio: HTTP-сессий нет, счётчик active неприменим.'
              : 'Активные MCP-сессии HTTP-слоя; entries — снимок без секретов.',
        };
      })(),
      last_audit: lastAudit,
    };
  },
});

/** Инструменты этого домена: и для `registerInfoTools`, и для сборки в `src/index.ts`. */
export function infoTools(): ToolDef[] {
  return [ocGetInfo, ocGetCapabilities, ocDiagnostics];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerInfoTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, infoTools());
}
