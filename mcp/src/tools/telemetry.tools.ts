/**
 * Инструменты WP1: состояние, телеметрия и статус процесса.
 *
 * Все — риск 0 (чтение), подтверждение не требуется.
 *  - `oc_get_state`          — ОДИН `GET /rest/oc/snapshot`: info + telemetry + status
 *                              (+ sensor/faults по `include`), `partial[]` и `stale`;
 *  - `oc_get_telemetry`      — `GET /rest/telemetry` (совместимость; `include_sensors` →
 *                              ещё `GET /rest/sensor`), snapshot не используется;
 *  - `oc_get_process_status` — только стадия/статус процесса из `/rest/telemetry`.
 *
 * Данные контроллера обновляются примерно раз в две секунды: даже свежий ответ
 * отстаёт до ~2 с. `stale:true` означает, что `lastUpdate` не менялся дольше 10 с
 * (телеметрия встала), а не «ответ пришёл с опозданием».
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import {
  buildStateView,
  fetchSensors,
  fetchSnapshot,
  fetchTelemetry,
  normalizeStatus,
  normalizeTelemetryTree,
  stalenessTrackerFor,
} from '../domain/telemetry.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef } from '../mcp/types.js';
import { annotationsForRisk } from '../policy/risk.js';

const RISK = 0 as const;

/** Компоненты снимка, которые можно включить/выключить в `oc_get_state`. */
export const INCLUDE_COMPONENTS = ['telemetry', 'sensors', 'faults', 'info'] as const;
type IncludeComponent = (typeof INCLUDE_COMPONENTS)[number];

const includeSchema = z
  .union([z.array(z.enum(INCLUDE_COMPONENTS)), z.string()])
  .optional()
  .describe(
    'Что вернуть из снимка: telemetry, sensors, faults, info (по умолчанию всё). ' +
      'Принимается массив или строка через запятую.',
  );

/** Разбирает `include` в набор компонентов. Неизвестные значения игнорируются. */
function parseInclude(raw: unknown): Set<IncludeComponent> {
  if (raw === undefined) return new Set(INCLUDE_COMPONENTS);
  const list = Array.isArray(raw) ? raw : typeof raw === 'string' ? raw.split(',') : [];
  const selected = new Set<IncludeComponent>();
  for (const item of list) {
    const name = String(item).trim().toLowerCase();
    if ((INCLUDE_COMPONENTS as readonly string[]).includes(name)) {
      selected.add(name as IncludeComponent);
    }
  }
  return selected;
}

export const ocGetState = defineTool({
  name: 'oc_get_state',
  title: 'Текущее состояние контроллера',
  description:
    'Возвращает состояние контроллера одним запросом GET /rest/oc/snapshot: версии, ' +
    'телеметрию, стадию/статус процесса, датчики и сбои (по include). Это основной инструмент ' +
    '«что сейчас с контроллером». Поля partial[] перечисляют компоненты, данных по которым нет, ' +
    'а stale:true означает, что телеметрия не обновлялась дольше 10 с. Контроллер обновляет ' +
    'данные примерно раз в две секунды, поэтому снимок отстаёт до ~2 с. Ничего не меняет.',
  inputSchema: {
    include: includeSchema,
  },
  annotations: annotationsForRisk(RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const tracker = stalenessTrackerFor(ctx.client);
    const snapshot = await fetchSnapshot(ctx.client);
    const state = buildStateView(snapshot, tracker);
    const include = parseInclude(args.include);

    const partial = new Set(state.partial);
    const result: Record<string, unknown> = {
      ok: true,
      // observed_at/last_update_ms — millis() устройства (uptime), не epoch.
      observed_at: state.observed_at,
      last_update_ms: state.last_update_ms,
      stale: state.stale,
      freshness_age_ms: state.freshness_age_ms,
      status: state.status,
    };

    if (include.has('info')) {
      if (state.info === null) partial.add('info');
      else result.info = state.info;
    }
    if (include.has('telemetry')) {
      // Снимок без телеметрии — уже ошибка устройства в buildStateView;
      // здесь значение всегда есть.
      result.telemetry = state.telemetry;
    }
    if (include.has('sensors')) {
      if (state.sensor === null) partial.add('sensor');
      else result.sensor = state.sensor;
    }
    if (include.has('faults')) {
      if (state.faults === null) partial.add('faults');
      else result.faults = state.faults;
    }
    // Статус возвращается всегда; если прошивка его не прислала — это «данных нет».
    if (state.status.stage === null && state.status.status === null) {
      partial.add('status');
    }

    result.partial = [...partial];
    return result as { ok: true } & Record<string, unknown>;
  },
});

export const ocGetTelemetry = defineTool({
  name: 'oc_get_telemetry',
  title: 'Телеметрия процесса',
  description:
    'Возвращает текущий снимок телеметрии: стадию, статус, показания SSVC, давление и признак ' +
    'потери UART. Контроллер обновляет данные примерно раз в две секунды, поле last_update_ms ' +
    'показывает возраст снимка. Читает GET /rest/telemetry (не snapshot); при ' +
    'include_sensors:true добавляет показания датчиков из GET /rest/sensor.',
  inputSchema: {
    include_sensors: z
      .boolean()
      .optional()
      .describe('Добавить показания датчиков (GET /rest/sensor); по умолчанию false'),
  },
  annotations: annotationsForRisk(RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const includeSensors = args.include_sensors === true;
    const tracker = stalenessTrackerFor(ctx.client);
    const data = await fetchTelemetry(ctx.client);
    const freshness = tracker.observe(data.lastUpdate);

    const result: Record<string, unknown> = {
      ok: true,
      telemetry: normalizeTelemetryTree(data.telemetry),
      status: normalizeStatus(data.status),
      // У /rest/telemetry нет observed_at: время снимка — last_update_ms.
      observed_at: null,
      last_update_ms: data.lastUpdate,
      stale: freshness.stale,
      freshness_age_ms: freshness.age_ms,
    };
    if (includeSensors) {
      result.sensor = await fetchSensors(ctx.client);
    }
    return result as { ok: true } & Record<string, unknown>;
  },
});

export const ocGetProcessStatus = defineTool({
  name: 'oc_get_process_status',
  title: 'Статус ректификации',
  description:
    'Возвращает только стадию и статус процесса без полной телеметрии. Подходит для частого ' +
    'опроса и проверок «процесс идёт или стоит». Контроллер обновляет статус примерно раз в ' +
    'две секунды; ничего не меняет.',
  inputSchema: {},
  annotations: annotationsForRisk(RISK),
  handler: async (_args, ctx) => {
    const data = await fetchTelemetry(ctx.client);
    const status = normalizeStatus(data.status);
    return {
      ok: true,
      stage: status.stage,
      status: status.status,
      uart_error: status.uart_error,
      started_at: status.started_at,
      ended_at: status.ended_at,
      stages: status.stages,
    };
  },
});

/** Инструменты этого домена: и для `registerTelemetryTools`, и для сборки в `src/index.ts`. */
export function telemetryTools(): ToolDef[] {
  return [ocGetState, ocGetTelemetry, ocGetProcessStatus];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerTelemetryTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, telemetryTools());
}
