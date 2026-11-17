/**
 * Инструменты WP4: датчики, привязка к зонам и пороги тревог
 * (mcp-server-agents.md §8).
 *
 *  - `oc_get_sensors`           — риск 0, `GET /rest/sensor` → плоский `zones[]`;
 *  - `oc_get_zones`             — риск 0, `GET /rest/zones` → карта «адрес → зона»;
 *  - `oc_set_zone`              — риск 1 (обратимая запись), `PUT /rest/sensors/zone` по
 *                                 одному адресу, затем перечитывание фактической привязки;
 *  - `oc_get_alarms`            — риск 0, `GET /rest/alarms` → `thresholds` как есть;
 *  - `oc_set_alarm_thresholds`  — риск 1, `POST /rest/alarms`, затем перечитывание порогов.
 *
 * Риск 1 по §2.6 не требует `confirm`, но каждая запись идёт через
 * `client.withWrite('sensors', …)`, пишется в лог и в аудит. Все маршруты admin-only:
 * 403 приходит из транспорта как `forbidden_admin_required` с подсказкой про admin.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { KNOWN_ZONE_NAMES } from '../device/schemas/sensors.js';
import {
  getAlarms,
  getSensors,
  getZones,
  setAlarmThresholds,
  setZone,
} from '../domain/sensors.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const WRITE_RISK: RiskClass = 1;

export const ocGetSensors = defineTool({
  name: 'oc_get_sensors',
  title: 'Показания датчиков',
  description:
    'Возвращает текущие показания датчиков по зонам: адрес, значение, единицы и тип. ' +
    'Данные обновляются циклом опроса прошивки, а не по запросу.',
  inputSchema: {
    zone: z
      .string()
      .optional()
      .describe(
        'Имя зоны для фильтра: unknown, inlet_water, outlet_water, act, tank, outdoor, ' +
          'receiver_tank_1, receiver_tank_2, receiver_tank_3. Без аргумента — все зоны.',
      ),
  },
  annotations: annotationsForRisk(READ_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const snapshot = await getSensors(ctx.client, {
      zone: typeof args.zone === 'string' ? args.zone : null,
    });
    return {
      ok: true,
      zones: snapshot.zones,
      observed_at: snapshot.observed_at,
    } as ToolResult;
  },
});

export const ocGetZones = defineTool({
  name: 'oc_get_zones',
  title: 'Привязка датчиков к зонам',
  description:
    'Возвращает карту «адрес датчика → зона»: куб, царга, приёмные ёмкости.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    const snapshot = await getZones(ctx.client);
    return {
      ok: true,
      zones: snapshot.zones,
      observed_at: snapshot.observed_at,
    } as ToolResult;
  },
});

export const ocSetZone = defineTool({
  name: 'oc_set_zone',
  title: 'Привязать датчик к зоне',
  description:
    'Меняет зону одного датчика по его адресу. Требует прав администратора; изменение ' +
    'сразу применяется к опросу.',
  inputSchema: {
    address: z
      .string()
      .describe(
        'Адрес 1-Wire: ровно 16 hex-символов, например 28FF64A1B2C3D4E5 (регистр не важен). ' +
          'Адрес есть в oc_get_zones или oc_get_sensors; проверяется локально до запроса.',
      ),
    zone: z
      .string()
      .describe(
        `Имя зоны (регистр не важен): ${KNOWN_ZONE_NAMES.join(', ')}. ` +
          'Неизвестное имя прошивка молча превращает в unknown, поэтому возвращается фактическая зона.',
      ),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_set_zone', WRITE_RISK, {});

    try {
      const outcome = await setZone(ctx.client, args.address, args.zone);

      auditAction(ctx, {
        tool: 'oc_set_zone',
        risk: WRITE_RISK,
        result: outcome.applied ? 'ok' : 'not_applied',
        details: {
          address: outcome.address,
          requested_zone: args.zone,
          actual_zone: outcome.zone,
          applied: outcome.applied,
          already_set: outcome.already_set,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        address: outcome.address,
        zone: outcome.zone,
        applied: outcome.applied,
        already_set: outcome.already_set,
        observed_at: outcome.observed_at,
      };
      if (outcome.warning !== undefined) payload.warning = outcome.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_set_zone',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: {
          address: typeof args.address === 'string' ? args.address.slice(0, 64) : null,
          requested_zone: typeof args.zone === 'string' ? args.zone.slice(0, 64) : null,
        },
      });
      throw error;
    }
  },
});

export const ocGetAlarms = defineTool({
  name: 'oc_get_alarms',
  title: 'Пороги тревог',
  description:
    'Возвращает пороги min/dangerous/critical по каждому датчику и признак включения тревог.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    const snapshot = await getAlarms(ctx.client);
    return {
      ok: true,
      thresholds: snapshot.thresholds,
      observed_at: snapshot.observed_at,
    } as ToolResult;
  },
});

export const ocSetAlarmThresholds = defineTool({
  name: 'oc_set_alarm_thresholds',
  title: 'Изменить пороги тревог',
  description:
    'Обновляет пороги тревог для указанных датчиков. Передавайте только те адреса, ' +
    'которые нужно изменить.',
  inputSchema: {
    thresholds: z
      .record(z.unknown())
      .describe(
        'Карта «адрес датчика → объект порогов»: enabled (boolean), min, dangerous, critical ' +
          '(числа). Можно передать часть полей — прошивка подставит значения по умолчанию. ' +
          'Неуказанные адреса не меняются.',
      ),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_set_alarm_thresholds', WRITE_RISK, {});

    const raw: unknown = args.thresholds;
    const addresses =
      typeof raw === 'object' && raw !== null && !Array.isArray(raw) ? Object.keys(raw) : [];

    try {
      const outcome = await setAlarmThresholds(ctx.client, raw);

      auditAction(ctx, {
        tool: 'oc_set_alarm_thresholds',
        risk: WRITE_RISK,
        result: outcome.applied ? 'ok' : 'not_applied',
        details: {
          addresses_count: outcome.requested_addresses.length,
          applied: outcome.applied,
          not_applied: outcome.not_applied,
          mismatched_count: outcome.mismatched.length,
        },
      });

      const payload: Record<string, unknown> = {
        ok: true,
        thresholds: outcome.thresholds,
        applied: outcome.applied,
        requested_addresses: outcome.requested_addresses,
        not_applied: outcome.not_applied,
        mismatched: outcome.mismatched,
        observed_at: outcome.observed_at,
      };
      if (outcome.warning !== undefined) payload.warning = outcome.warning;
      return payload as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_set_alarm_thresholds',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: { addresses_count: addresses.length },
      });
      throw error;
    }
  },
});

/** Инструменты домена: и для `registerSensorsTools`, и для сборки в `src/index.ts`. */
export function sensorsTools(): ToolDef[] {
  return [ocGetSensors, ocGetZones, ocSetZone, ocGetAlarms, ocSetAlarmThresholds];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerSensorsTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, sensorsTools());
}
