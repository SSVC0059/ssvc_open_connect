/**
 * Инструменты WP6: датчики уровня XKC-25 (mcp-server-agents.md §10).
 *
 *  - `oc_get_xkc25_status`      — риск 0, `GET /rest/oc/xkc25/status` → слоты датчиков,
 *                                 режим предохранителя старта, `blockReasons`, `warnings`;
 *  - `oc_xkc25_calibrate`       — риск 1: один шаг мастера. `without_liquid`/`with_liquid` —
 *                                 ровно один POST; `verify_return` — POST, затем опрос
 *                                 `verify-status` каждые 500 мс до результата или `timeout_ms`;
 *  - `oc_xkc25_verify_status`   — риск 0, результат последней асинхронной проверки возврата.
 *
 * Калибровка перезаписывает сырые уровни датчика в конфигурации, поэтому:
 *  - аргументы проверяются локально ДО сети (опечатка не должна дойти до POST);
 *  - POST не повторяется ни транспортом, ни доменом;
 *  - риск 1 не требует `confirm`, но каждая запись идёт через `client.withWrite('xkc25', …)`
 *    и попадает в лог и аудит.
 *
 * Имя `ok` верхнего уровня занято конвертом инструмента, поэтому поле `ok` прошивки
 * отдаётся как `device_ok`; во вложенном `verify` оно остаётся `ok`.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { XKC25_STEPS } from '../device/schemas/xkc25.js';
import {
  calibrateXkc25,
  getXkc25Status,
  getXkc25VerifyStatus,
  normalizeCalibrationArgs,
  XKC25_DEFAULT_TIMEOUT_MS,
  XKC25_MAX_TIMEOUT_MS,
  XKC25_MIN_TIMEOUT_MS,
} from '../domain/xkc25.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const READ_RISK: RiskClass = 0;
const WRITE_RISK: RiskClass = 1;

export const ocGetXkc25Status = defineTool({
  name: 'oc_get_xkc25_status',
  title: 'Датчики уровня XKC-25',
  description:
    'Возвращает состояние датчиков уровня: зона, источник сигнала, калибровка, текущий ' +
    'уровень и заполненность, а также режим предохранителя старта и причины возможной ' +
    'блокировки.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    const snapshot = await getXkc25Status(ctx.client);
    return { ok: true, ...snapshot } as ToolResult;
  },
});

export const ocXkc25Calibrate = defineTool({
  name: 'oc_xkc25_calibrate',
  title: 'Откалибровать датчик уровня',
  description:
    'Выполняет один шаг калибровки: без жидкости, с жидкостью или проверку возврата. ' +
    'Шаг проверки возврата выполняется асинхронно — инструмент дожидается результата в ' +
    'пределах timeout_ms.',
  inputSchema: {
    index: z
      .number()
      .int()
      .min(0)
      .max(3)
      .describe(
        'Номер слота датчика, целое 0..3. Возьмите index из oc_get_xkc25_status. ' +
          'Проверяется локально до запроса к контроллеру.',
      ),
    step: z
      .enum(XKC25_STEPS)
      .describe(
        'Шаг калибровки: without_liquid — датчик без жидкости; with_liquid — датчик с ' +
          'жидкостью; verify_return — асинхронная проверка возврата уровня.',
      ),
    timeout_ms: z
      .number()
      .int()
      .min(XKC25_MIN_TIMEOUT_MS)
      .max(XKC25_MAX_TIMEOUT_MS)
      .optional()
      .describe(
        `Общий бюджет ожидания результата verify_return, ${XKC25_MIN_TIMEOUT_MS}..${XKC25_MAX_TIMEOUT_MS} мс ` +
          `(по умолчанию ${XKC25_DEFAULT_TIMEOUT_MS}). На контроллере сама проверка ограничена ` +
          '5 секундами; для without_liquid/with_liquid поле не используется.',
      ),
  },
  annotations: annotationsForRisk(WRITE_RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    enforceRisk(ctx, 'oc_xkc25_calibrate', WRITE_RISK, {});

    // Локальная проверка: неверный index/step/timeout не должен стать POST-ом,
    // потому что шаг перезаписывает сырые уровни датчика.
    const request = normalizeCalibrationArgs(args);

    try {
      const outcome = await calibrateXkc25(ctx.client, request);

      auditAction(ctx, {
        tool: 'oc_xkc25_calibrate',
        risk: WRITE_RISK,
        result: outcome.completed
          ? 'completed'
          : outcome.device_ok
            ? 'step_done'
            : 'step_rejected',
        details: {
          index: request.index,
          step: request.step,
          timeout_ms: request.timeout_ms,
          completed: outcome.completed,
          code: outcome.code,
          device_ok: outcome.device_ok,
          verify_status: outcome.verify?.status ?? null,
        },
      });

      return { ok: true, ...outcome } as ToolResult;
    } catch (error) {
      auditAction(ctx, {
        tool: 'oc_xkc25_calibrate',
        risk: WRITE_RISK,
        result: error instanceof OcError ? error.code : 'internal_error',
        details: {
          index: request.index,
          step: request.step,
          timeout_ms: request.timeout_ms,
        },
      });
      throw error;
    }
  },
});

export const ocXkc25VerifyStatus = defineTool({
  name: 'oc_xkc25_verify_status',
  title: 'Результат проверки возврата',
  description:
    'Возвращает результат последней асинхронной проверки возврата датчика: идёт, ' +
    'завершена, не запускалась.',
  inputSchema: {},
  annotations: annotationsForRisk(READ_RISK),
  handler: async (_args, ctx) => {
    const view = await getXkc25VerifyStatus(ctx.client);
    return {
      ok: true,
      status: view.status,
      index: view.index,
      // Поле `ok` прошивки: верхний уровень занят конвертом инструмента.
      device_ok: view.ok,
      completed: view.completed,
      code: view.code,
      message: view.message,
      raw_level: view.raw_level,
      observed_at: view.observed_at,
    } as ToolResult;
  },
});

/** Инструменты домена: и для `registerXkc25Tools`, и для сборки в `src/index.ts`. */
export function xkc25Tools(): ToolDef[] {
  return [ocGetXkc25Status, ocXkc25Calibrate, ocXkc25VerifyStatus];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerXkc25Tools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, xkc25Tools());
}
