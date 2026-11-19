/**
 * Схемы домена команд SSVC (WP2, mcp-server-agents.md §6 + «Обновление после §9»).
 *
 * Контракт прошивки 0.2.9.2:
 *  - `POST /rest/commands` при успехе отдаёт JSON
 *    `{status:"accepted", command, commandId, warnings?[]}`; старые прошивки —
 *    `text/plain "Command accepted"` (тогда `commandId` нет);
 *  - `GET /rest/commands/{id}` — журнал попытки
 *    `{id, command, state, queued_at, started_at, finished_at, ok, reason, confidence}`;
 *    `state` ∈ `queued/running/sent/done/rejected/timeout`.
 *
 * Схемы намеренно терпимые:
 *  - `commandId`/`id` бывают числом или числовой строкой;
 *  - `state` читаем строкой, а не enum: новая прошивка может добавить состояние,
 *    и `device_error` из-за этого был бы хуже, чем честное «неизвестное состояние»;
 *  - `reason` бывает `null` и строкой;
 *  - неизвестные поля пропускаем (`.passthrough()`), чтобы не ломаться на расширениях.
 *
 * `sent` — нормальное терминальное состояние для start/next/pause/resume/stop:
 * ответа SSVC по этим командам не ждут. `done` — только когда SSVC ответил.
 * `confidence:"transport"` означает «ушло в UART», а не «физически выполнено».
 */

import { z } from 'zod';

/** Все имена, которые принимает `POST /rest/commands` (COMMAND_MAP прошивки). */
export const COMMAND_NAMES = [
  'at',
  'next',
  'pause',
  'stop',
  'start',
  'resume',
  'version',
  'get_settings',
  'settings',
  'emergency_stop',
  'status',
  'set',
  'get_log',
] as const;
export type CommandName = (typeof COMMAND_NAMES)[number];

/** Команды управления процессом: для них есть отдельные инструменты. */
export const PROCESS_COMMAND_NAMES = [
  'start',
  'next',
  'pause',
  'resume',
  'stop',
  'emergency_stop',
] as const;
export type ProcessCommandName = (typeof PROCESS_COMMAND_NAMES)[number];

/**
 * Служебные команды, разрешённые в `oc_send_command` (§6): version, status,
 * get_settings, set, at, get_log (settings — исторический алиас get_settings).
 * Управление процессом сюда не входит.
 */
export const SERVICE_COMMAND_NAMES = [
  'at',
  'version',
  'get_settings',
  'settings',
  'status',
  'set',
  'get_log',
] as const;
export type ServiceCommandName = (typeof SERVICE_COMMAND_NAMES)[number];

/**
 * Команды, на которые SSVC отвечает: для них имеет смысл опрашивать журнал
 * `GET /rest/commands/{id}` до терминального состояния. Для start/next/pause/
 * resume/stop ответа не ждут — их терминальное состояние `sent`.
 */
export const REPLY_COMMAND_NAMES = [
  'status',
  'version',
  'get_settings',
  'settings',
  'set',
  'get_log',
] as const;

/** Состояния журнала команд. */
export const COMMAND_STATES = [
  'queued',
  'running',
  'sent',
  'done',
  'rejected',
  'timeout',
] as const;
export type CommandState = (typeof COMMAND_STATES)[number];

/** Терминальные состояния: опрос журнала дальше бессмыслен. */
export const TERMINAL_COMMAND_STATES = ['sent', 'done', 'rejected', 'timeout'] as const;

/** Синтетическое состояние для 404: запись вытеснена из журнала (не «команда не выполнялась»). */
export const UNKNOWN_COMMAND_STATE = 'unknown' as const;

/** Идентификатор команды: прошивка отдаёт число, но строку тоже принимаем. */
export const commandIdSchema = z.union([z.number().finite(), z.string().regex(/^\d+$/)]);

const uptimeValueSchema = z.union([z.number(), z.string()]);
const nullableString = z.string().nullish();

/**
 * 200 application/json: команда принята в очередь.
 * `status` необязателен: старые/новые сборки могут не прислать его в JSON-ветке.
 */
export const commandAcceptedBodySchema = z
  .object({
    status: z.string().optional(),
    command: z.string().optional(),
    commandId: commandIdSchema.optional(),
    warnings: z.array(z.string()).optional(),
  })
  .passthrough();

/** 409 application/json: сработал предохранитель XKC-25. */
export const commandRejectedBodySchema = z
  .object({
    status: z.string().optional(),
    command: z.string().optional(),
    blockReasons: z.array(z.string()).optional(),
    warnings: z.array(z.string()).optional(),
  })
  .passthrough();

/** `GET /rest/commands/{id}` — запись журнала попытки. */
export const commandJournalEntrySchema = z
  .object({
    id: commandIdSchema,
    command: z.string(),
    state: z.string(),
    queued_at: uptimeValueSchema.nullish(),
    started_at: uptimeValueSchema.nullish(),
    finished_at: uptimeValueSchema.nullish(),
    ok: z.boolean().nullish(),
    reason: nullableString,
    confidence: nullableString,
  })
  .passthrough();

/** 400/404 журнала команд: `{error:"invalid_id"}` / `{error:"not_found"}`. */
export const commandErrorBodySchema = z
  .object({
    error: z.string().optional(),
  })
  .passthrough();

/** Числовой id из числа/строки; null — если значение не похоже на идентификатор. */
export function parseCommandId(value: unknown): number | null {
  if (typeof value === 'number') {
    return Number.isSafeInteger(value) && value >= 0 ? value : null;
  }
  if (typeof value === 'string') {
    const trimmed = value.trim();
    if (!/^\d+$/.test(trimmed)) return null;
    const parsed = Number(trimmed);
    return Number.isSafeInteger(parsed) ? parsed : null;
  }
  return null;
}

/** Приводит состояние журнала к нижнему регистру; пустое значение — `unknown`. */
export function normalizeCommandState(value: unknown): string {
  if (typeof value !== 'string') return UNKNOWN_COMMAND_STATE;
  const normalized = value.trim().toLowerCase();
  return normalized === '' ? UNKNOWN_COMMAND_STATE : normalized;
}

/** Терминальное ли состояние (знает только контрактные значения). */
export function isTerminalCommandState(state: string): boolean {
  return (TERMINAL_COMMAND_STATES as readonly string[]).includes(state);
}
