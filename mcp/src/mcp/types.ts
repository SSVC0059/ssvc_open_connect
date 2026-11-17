/**
 * Контракт инструмента MCP (договор между WP0 и доменными пакетами, §2.2).
 *
 * Домены импортируют отсюда только типы и `defineTool`; реестр их регистрирует.
 * `inputSchema` — именно `z.ZodRawShape` (объект полей), а не `z.object(...)`:
 * SDK сам превращает shape в JSON Schema и валидирует аргументы.
 */

import type { z } from 'zod';
import type { ToolAnnotations } from '@modelcontextprotocol/sdk/types.js';
import type { Config } from '../config.js';
import type { DeviceClient } from '../device/DeviceClient.js';
import type { CapabilityCache } from '../device/CapabilityCache.js';
import type { RawLogger } from '../device/RawLogger.js';
import type { Logger } from '../logging.js';
import type { AuditLog } from '../audit.js';
import type { ErrorEnvelope } from '../errors.js';

/** Всё, что нужно обработчику инструмента. Создаётся на сессию. */
export interface ToolContext {
  cfg: Config;
  client: DeviceClient;
  caps: CapabilityCache;
  log: Logger;
  audit: AuditLog;
  raw: RawLogger;
}

/** Паспорт подключённого контроллера — без секретов. */
export interface DeviceMeta {
  url: string;
  firmware: string;
  ssvcVersion: string;
  api: string;
  apiCode: number;
  compatibility: 'unknown' | 'too_old' | 'supported';
}

export type ToolResult = ({ ok: true } & Record<string, unknown>) | ErrorEnvelope;

export interface ToolDef {
  /** oc_<домен>_<действие>, snake_case, латиницей. */
  name: string;
  /** По-русски, до 60 символов. */
  title: string;
  /** По-русски, 1–2 предложения: что делает и чего не делает. */
  description: string;
  inputSchema: z.ZodRawShape;
  /**
   * По умолчанию не задаём (см. §2.3): без схемы SDK не проверяет `structuredContent`,
   * и конверт ошибки может быть отдан вместе со `structuredContent`.
   */
  outputSchema?: z.ZodRawShape;
  annotations: ToolAnnotations;
  handler: (args: Record<string, any>, ctx: ToolContext) => Promise<ToolResult>;
}

/** Регистр имени инструмента: только латиница, цифры и подчёркивания. */
const TOOL_NAME_RE = /^oc_[a-z0-9]+(?:_[a-z0-9]+)*$/;

/**
 * Проверяет и возвращает определение инструмента.
 * Ошибки здесь — дефект кода домена, поэтому бросаем исключение на старте сервиса,
 * а не отдаём клиенту.
 */
export function defineTool(def: ToolDef): ToolDef {
  if (!TOOL_NAME_RE.test(def.name)) {
    throw new Error(
      `Некорректное имя инструмента "${def.name}": ожидается oc_<домен>_<действие> латиницей в snake_case.`,
    );
  }
  if (def.title.trim() === '' || def.title.length > 60) {
    throw new Error(
      `Инструмент ${def.name}: title должен быть непустым и не длиннее 60 символов (сейчас ${def.title.length}).`,
    );
  }
  if (def.description.trim() === '') {
    throw new Error(`Инструмент ${def.name}: description обязателен (по-русски, 1–2 предложения).`);
  }
  for (const [key, schema] of Object.entries(def.inputSchema)) {
    const candidate = schema as { safeParse?: unknown; _def?: unknown };
    if (typeof candidate !== 'object' || candidate === null || typeof candidate.safeParse !== 'function') {
      throw new Error(
        `Инструмент ${def.name}: поле inputSchema.${key} должно быть zod-схемой (z.string(), z.number(), …).`,
      );
    }
  }
  return def;
}
