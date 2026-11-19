/**
 * Инструменты WP2: команды процесса, ожидание изменения и обработка предохранителей
 * (mcp-server-agents.md §6 + «Обновление после §9»).
 *
 * Честность контракта в текстах инструментов:
 *  - accepted/200 — это «принято в очередь», а не «выполнено»;
 *  - `confidence:"transport"` — «ушло в UART», физического подтверждения нет;
 *  - для start/next/pause/resume/stop терминальное состояние `sent` — норма
 *    (SSVC не отвечает), фактический эффект проверяется `oc_wait_for_change`;
 *  - command_id ссылается на журнал (16 записей, TTL 10 минут); 404 на журнал —
 *    «запись вытеснена», а не «команда не выполнялась».
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import {
  REPLY_COMMAND_NAMES,
  SERVICE_COMMAND_NAMES,
} from '../device/schemas/commands.js';
import {
  sendCommand,
  waitForChange,
  waitForCommandTerminal,
  type ChangeExpectation,
  type CommandDispatch,
  type CommandJournalWait,
} from '../domain/commands.js';
import { OcError } from '../errors.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { auditAction, enforceRisk } from '../policy/confirmation.js';
import { annotationsForRisk, type RiskClass } from '../policy/risk.js';

const PROCESS_RISK: RiskClass = 2;
const MAX_PARAMETERS_LENGTH = 300;

function acceptedPayload(dispatch: CommandDispatch): Record<string, unknown> {
  const payload: Record<string, unknown> = {
    ok: true,
    accepted: true,
    status: dispatch.status,
    command: dispatch.command,
    command_id: dispatch.command_id,
    warnings: dispatch.warnings,
    confidence: dispatch.confidence,
    transport_note:
      'Подтверждение транспортное: команда принята в очередь/UART, физическое выполнение ' +
      'сервер не подтверждает.',
  };
  if (dispatch.command_id === null) {
    payload.command_id_note =
      'Прошивка ответила без commandId (text/plain): команда принята, но запись журнала ' +
      'недоступна. Состояние проверяйте через oc_wait_for_change.';
  }
  if (dispatch.raw !== undefined) payload.raw = dispatch.raw;
  return payload;
}

function auditAccepted(
  ctx: ToolContext,
  tool: string,
  risk: RiskClass,
  args: Record<string, any>,
  dispatch: CommandDispatch,
): void {
  auditAction(ctx, {
    tool,
    risk,
    confirm: args.confirm === true ? true : undefined,
    result: 'accepted',
    details: {
      command: dispatch.command,
      command_id: dispatch.command_id,
      warnings_count: dispatch.warnings.length,
    },
  });
}

function auditFailure(
  ctx: ToolContext,
  tool: string,
  risk: RiskClass,
  args: Record<string, any>,
  command: string,
  error: unknown,
): void {
  auditAction(ctx, {
    tool,
    risk,
    confirm: args.confirm === true ? true : undefined,
    result: error instanceof OcError ? error.code : 'internal_error',
    details: { command },
  });
}

/** Общая часть инструментов процесса: confirm → withWrite → POST → аудит. */
async function dispatchProcessCommand(
  ctx: ToolContext,
  tool: string,
  command: string,
  args: Record<string, any>,
): Promise<ToolResult> {
  enforceRisk(ctx, tool, PROCESS_RISK, { confirm: args.confirm });
  try {
    const dispatch = await sendCommand(ctx.client, command);
    auditAccepted(ctx, tool, PROCESS_RISK, args, dispatch);
    return acceptedPayload(dispatch) as ToolResult;
  } catch (error) {
    auditFailure(ctx, tool, PROCESS_RISK, args, command, error);
    throw error;
  }
}

function processTool(spec: {
  name: string;
  command: string;
  title: string;
  description: string;
}): ToolDef {
  return defineTool({
    name: spec.name,
    title: spec.title,
    description: spec.description,
    inputSchema: {
      confirm: z
        .boolean()
        .optional()
        .describe(
          'Явное подтверждение действия, влияющего на процесс. Без confirm:true команда не отправляется.',
        ),
    },
    annotations: annotationsForRisk(PROCESS_RISK),
    handler: async (args, ctx) => dispatchProcessCommand(ctx, spec.name, spec.command, args),
  });
}

export const ocStart = processTool({
  name: 'oc_start',
  command: 'start',
  title: 'Запустить ректификацию',
  description:
    'Ставит команду START в очередь контроллера и возвращает результат постановки: accepted, ' +
    'command_id и предупреждения предохранителей (warnings). НЕ ждёт начала процесса: accepted ' +
    'означает «принято в очередь», confidence "transport" — «ушло в UART», физического ' +
    'подтверждения нет. Если сработал предохранитель XKC-25, возвращает guard_blocked с ' +
    'block_reasons. Чтобы дождаться стадии, используйте oc_wait_for_change.',
});

export const ocPause = processTool({
  name: 'oc_pause',
  command: 'pause',
  title: 'Поставить отбор на паузу',
  description:
    'Ставит команду PAUSE в очередь контроллера. Процесс остаётся запущенным, отбор ' +
    'приостанавливается до oc_resume. Не ждёт реакции SSVC: accepted и confidence "transport" ' +
    'означают только постановку/отправку в UART; фактическое состояние проверяйте oc_wait_for_change.',
});

export const ocResume = processTool({
  name: 'oc_resume',
  command: 'resume',
  title: 'Продолжить отбор',
  description:
    'Ставит команду RESUME в очередь контроллера и возвращает результат постановки (accepted, ' +
    'command_id). Не ждёт выполнения: confidence "transport" означает «ушло в UART»; эффект ' +
    'проверяйте через oc_wait_for_change.',
});

export const ocNext = processTool({
  name: 'oc_next',
  command: 'next',
  title: 'Перейти к следующей фракции',
  description:
    'Ставит команду NEXT в очередь контроллера: текущая фракция завершается, начинается ' +
    'следующая по программе. Не ждёт смены стадии: accepted и confidence "transport" — только ' +
    'постановка в очередь; новую стадию дождитесь через oc_wait_for_change.',
});

export const ocStop = processTool({
  name: 'oc_stop',
  command: 'stop',
  title: 'Остановить ректификацию',
  description:
    'Ставит команду STOP в очередь контроллера. Это штатная остановка; для аварийной ' +
    'используйте oc_emergency_stop. Не ждёт выполнения: accepted и confidence "transport" ' +
    'означают «принято в очередь/ушло в UART»; возврат процесса в waiting проверяйте ' +
    'через oc_wait_for_change.',
});

export const ocEmergencyStop = defineTool({
  name: 'oc_emergency_stop',
  title: 'Аварийная остановка',
  description:
    'Немедленно ставит команду EMERGENCY_STOP в аварийную очередь контроллера, минуя ' +
    'накопленные команды. Подтверждение не требуется: это защитное действие. accepted и ' +
    'confidence "transport" означают постановку в очередь/отправку в UART, а не физическое ' +
    'завершение процесса.',
  inputSchema: {},
  annotations: annotationsForRisk(PROCESS_RISK),
  handler: async (_args, ctx) => {
    // Защитное действие: enforceRisk вернётся сразу (oc_emergency_stop в RISK_EXEMPT_TOOLS),
    // confirm не требуется по контракту §2.6.
    enforceRisk(ctx, 'oc_emergency_stop', PROCESS_RISK, {});
    try {
      const dispatch = await sendCommand(ctx.client, 'emergency_stop', { urgent: true });
      auditAction(ctx, {
        tool: 'oc_emergency_stop',
        risk: PROCESS_RISK,
        result: 'accepted',
        details: { command: dispatch.command, command_id: dispatch.command_id },
      });
      return acceptedPayload(dispatch) as ToolResult;
    } catch (error) {
      auditFailure(ctx, 'oc_emergency_stop', PROCESS_RISK, {}, 'emergency_stop', error);
      throw error;
    }
  },
});

const expectValueSchema = z
  .union([z.string(), z.array(z.string()).min(1)])
  .describe('Ожидаемое значение или список допустимых значений (совпадение по любому).');

const expectSchema = z
  .object({
    stage: expectValueSchema.optional(),
    status: expectValueSchema.optional(),
    telemetry_type: expectValueSchema.optional(),
    uart_error: z.boolean().optional(),
  })
  .describe(
    'Ожидаемые поля состояния. Сравнивается только то, что указано (подмножество). ' +
      'stage/status — из статуса процесса, telemetry_type — поле type телеметрии.',
  );

export const ocWaitForChange = defineTool({
  name: 'oc_wait_for_change',
  title: 'Дождаться изменения состояния',
  description:
    'Опрашивает снимок состояния (/rest/oc/snapshot), пока указанные поля не совпадут с ' +
    'ожидаемыми, либо пока не истечёт таймаут. Возвращает matched, timeout и последнее ' +
    'наблюдённое состояние; таймаут — не ошибка. Если контроллер потерял UART ' +
    '(uart_error:true), ожидание останавливается с matched:false и явной пометкой: данные ' +
    'недостоверны. Сам ничего не меняет.',
  inputSchema: {
    expect: expectSchema,
    timeout_s: z
      .number()
      .int()
      .min(1)
      .max(600)
      .optional()
      .describe('Общий таймаут ожидания в секундах, 1..600 (по умолчанию 120).'),
    poll_ms: z
      .number()
      .int()
      .min(250)
      .max(5000)
      .optional()
      .describe('Интервал опроса в миллисекундах, 250..5000 (по умолчанию 1000).'),
  },
  annotations: annotationsForRisk(0),
  handler: async (args, ctx) => {
    const expect =
      typeof args.expect === 'object' && args.expect !== null
        ? (args.expect as ChangeExpectation)
        : {};
    const timeoutS = typeof args.timeout_s === 'number' ? args.timeout_s : 120;
    const pollMs = typeof args.poll_ms === 'number' ? args.poll_ms : 1_000;
    const result = await waitForChange(ctx.client, expect, {
      timeoutMs: timeoutS * 1_000,
      pollMs,
    });
    return { ok: true, ...result } as ToolResult;
  },
});

export const ocSendCommand = defineTool({
  name: 'oc_send_command',
  title: 'Служебная команда контроллеру',
  description:
    'Отправляет служебную команду из белого списка (version, status, get_settings, settings, ' +
    'set, at, get_log). Команды управления процессом сюда не входят — для них есть отдельные ' +
    'инструменты. accepted и confidence "transport" означают только транспортное подтверждение; ' +
    'для команд, ждущих ответа SSVC, инструмент опрашивает журнал /rest/commands/{id} до ' +
    'терминального состояния. state "done" — SSVC ответил, state "sent" — ушло в UART, ответа ' +
    'не будет; 404 журнала означает «запись вытеснена», а не «команда не выполнялась».',
  inputSchema: {
    command: z
      .enum(SERVICE_COMMAND_NAMES)
      .describe(
        'Служебная команда: at, version, get_settings, settings, status, set, get_log. ' +
          'Управление процессом (start/pause/resume/next/stop/emergency_stop) сюда не входит.',
      ),
    parameters: z
      .string()
      .max(MAX_PARAMETERS_LENGTH)
      .optional()
      .describe('Параметры команды (для set/at/get_log), не длиннее 300 символов.'),
    confirm: z
      .boolean()
      .optional()
      .describe('Явное подтверждение влияющего на контроллер действия; без confirm:true команда не отправляется.'),
  },
  annotations: annotationsForRisk(PROCESS_RISK),
  handler: async (args, ctx) => {
    enforceRisk(ctx, 'oc_send_command', PROCESS_RISK, { confirm: args.confirm });

    const command = typeof args.command === 'string' ? args.command.trim() : '';
    if (!(SERVICE_COMMAND_NAMES as readonly string[]).includes(command)) {
      throw new OcError(
        'validation_failed',
        `Команда «${command || '(пусто)'}» не входит в список служебных команд`,
        {
          hint:
            `Разрешены: ${SERVICE_COMMAND_NAMES.join(', ')}. Для управления процессом ` +
            'используйте oc_start, oc_pause, oc_resume, oc_next, oc_stop, oc_emergency_stop.',
          details: {
            reason: 'invalid_argument',
            allowed: [...SERVICE_COMMAND_NAMES],
            received: command,
          },
        },
      );
    }

    const parameters = typeof args.parameters === 'string' ? args.parameters : '';
    const parametersBytes = Buffer.byteLength(parameters, 'utf8');
    if (parametersBytes > MAX_PARAMETERS_LENGTH) {
      throw new OcError(
        'validation_failed',
        `Параметры команды «${command}» длиннее ${MAX_PARAMETERS_LENGTH} байт`,
        {
          hint: 'Сократите parameters: прошивка принимает не больше 300 байт в SET-команде.',
          details: {
            reason: 'invalid_argument',
            length_bytes: parametersBytes,
            max_length_bytes: MAX_PARAMETERS_LENGTH,
          },
        },
      );
    }

    try {
      const dispatch = await sendCommand(ctx.client, command, { parameters });
      const replyExpected = (REPLY_COMMAND_NAMES as readonly string[]).includes(command);
      let journal: CommandJournalWait | null = null;
      if (replyExpected && dispatch.command_id !== null) {
        journal = await waitForCommandTerminal(ctx.client, dispatch.command_id);
      }

      auditAccepted(ctx, 'oc_send_command', PROCESS_RISK, args, dispatch);

      return {
        ok: true,
        ...acceptedPayload(dispatch),
        reply_expected: replyExpected,
        journal,
        journal_note:
          journal === null
            ? replyExpected
              ? 'Команда ждёт ответа SSVC, но command_id нет (старая прошивка): ' +
                'состояние журнала недоступно, проверьте эффект через oc_wait_for_change.'
              : 'Для этой команды ответ SSVC не ожидается: терминальное состояние sent — норма.'
            : null,
      } as ToolResult;
    } catch (error) {
      auditFailure(ctx, 'oc_send_command', PROCESS_RISK, args, command, error);
      throw error;
    }
  },
});

/** Инструменты домена: и для `registerCommandsTools`, и для сборки в `src/index.ts`. */
export function commandsTools(): ToolDef[] {
  return [
    ocStart,
    ocPause,
    ocResume,
    ocNext,
    ocStop,
    ocEmergencyStop,
    ocWaitForChange,
    ocSendCommand,
  ];
}

/** Регистрирует инструменты домена в конкретном MCP-сервере (сессия). */
export function registerCommandsTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, commandsTools());
}
