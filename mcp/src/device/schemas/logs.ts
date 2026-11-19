/**
 * Схемы журналов ректификации (WP7, mcp-server-agents.md §11).
 *
 * Контракт прошивки (mcp-contract-notes.md §B.16, §D.8):
 *  - `GET /rest/logs` → `{status,total,received[,files][,error]}`;
 *  - `GET /rest/logs/{id}` → 202 с тем же снимком прогресса либо 200 `text/csv`;
 *  - `DELETE /rest/logs` → `{status}`; ошибки — `{error:"log_transfer_busy"|…}`.
 *
 * Схема намеренно мягкая: обязателен только `status` (без него нельзя понять
 * состояние сессии), остальные поля необязательны и приводятся к нулю/пустому
 * массиву в домене. Так новая прошивка может добавить поля, не ломая сервер.
 */

import { z } from 'zod';

/** Наружные состояния передачи по прошивке (`HandlerRegistrator.cpp:319-335`). */
export const DEVICE_LOG_STATUSES = ['idle', 'list', 'receiving', 'completed', 'error'] as const;
export type DeviceLogStatus = (typeof DEVICE_LOG_STATUSES)[number];

/**
 * `busy` — не состояние прошивки, а ответ `409 log_transfer_busy`.
 * Инструмент `oc_logs_list` отдаёт его как успех (mcp-server-agents.md §11).
 */
export const LOGS_STATUSES = [...DEVICE_LOG_STATUSES, 'busy'] as const;
export type LogsStatus = (typeof LOGS_STATUSES)[number];

/** Снимок передачи: `GET /rest/logs`, `GET /rest/logs/{id}` (202) и тело ошибок. */
export const logsSnapshotSchema = z
  .object({
    status: z.string().min(1),
    total: z.number().int().nonnegative().optional(),
    received: z.number().int().nonnegative().optional(),
    files: z.array(z.string()).optional(),
    error: z.string().optional(),
  })
  .passthrough();

export type LogsSnapshotBody = z.infer<typeof logsSnapshotSchema>;

/** Тело отказа `/rest/logs*`: поле `error` — машинный код (mcp-contract-notes.md §D.8). */
export const logsErrorBodySchema = z
  .object({
    error: z.string().optional(),
    feature: z.string().optional(),
    required_api: z.string().optional(),
    device_api: z.string().optional(),
  })
  .passthrough();

export type LogsErrorBody = z.infer<typeof logsErrorBodySchema>;

/** Ответ `DELETE /rest/logs` — новое состояние сессии (по контракту `idle`). */
export const logsCancelResponseSchema = z
  .object({
    status: z.string().min(1),
  })
  .passthrough();

export type LogsCancelResponseBody = z.infer<typeof logsCancelResponseSchema>;
