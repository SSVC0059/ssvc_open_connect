/**
 * Реестр инструментов (контракт §2.2, §2.3).
 *
 * Задача реестра — одна: превратить `ToolDef` домена в инструмент MCP с единым
 * конвертом результата. Никакой доменной логики здесь нет.
 *
 * Конверт (§2.1):
 *  - успех  → `content:[{type:'text', text:<JSON>}]` + `structuredContent = {ok:true, ...}`
 *             (`isError` не выставляется);
 *  - отказ  → тот же текст + `isError:true`, чтобы клиент видел ошибку, а не «успешный» текст.
 *             `structuredContent` с конвертом ошибки добавляется только инструментам без
 *             `outputSchema` — см. пояснение в `toCallToolResult`.
 *
 * Про `outputSchema` (проверено test/registry.test.ts, вывод — в README):
 *  - без `outputSchema` SDK не трогает `structuredContent`: любые поля проходят как есть;
 *  - с `outputSchema` сервер проверяет `structuredContent`, но только у успешных ответов
 *    (при `isError:true` проверка пропускается); клиент SDK проверяет наличие схемы
 *    строже и валидирует `structuredContent`, даже если это ответ с ошибкой.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { errorEnvelope } from '../errors.js';
import type { ToolContext, ToolDef, ToolResult } from './types.js';

/** Текстовая часть результата: тот же JSON, что и в `structuredContent`. */
function textOf(result: ToolResult): string {
  return JSON.stringify(result);
}

/**
 * Приводит конверт инструмента к ответу MCP.
 * Ошибка всегда помечается `isError:true` — иначе клиент примет её за данные.
 *
 * `structuredContent` у отказа отдаём только инструментам **без** `outputSchema`:
 * сервер пропускает проверку вывода при `isError`, а клиент SDK — нет, и конверт
 * ошибки (другая форма) был бы отвергнут как несоответствие схеме. У инструментов
 * со схемой код ошибки остаётся в текстовой части (`content[0].text` — тот же JSON).
 */
export function toCallToolResult(
  result: ToolResult,
  opts: { hasOutputSchema?: boolean } = {},
): CallToolResult {
  const withStructured = opts.hasOutputSchema !== true || result.ok === true;
  if (result.ok === false) {
    return {
      content: [{ type: 'text', text: textOf(result) }],
      ...(withStructured
        ? { structuredContent: result as unknown as Record<string, unknown> }
        : {}),
      isError: true,
    };
  }
  return {
    content: [{ type: 'text', text: textOf(result) }],
    structuredContent: result as unknown as Record<string, unknown>,
  };
}

/**
 * Выполняет инструмент и конвертирует результат/исключение.
 * Неизвестные исключения не глотаем: они становятся `internal_error` с русским текстом,
 * а причина уходит в лог сервера.
 */
export async function runTool(
  tool: ToolDef,
  ctx: ToolContext,
  args: Record<string, any>,
): Promise<CallToolResult> {
  const log = ctx.log.child({ tool: tool.name });
  const started = Date.now();
  log.debug('Вызов инструмента', { tool: tool.name });

  let result: ToolResult;
  try {
    result = await tool.handler(args, ctx);
  } catch (error) {
    log.error('Инструмент завершился исключением', { tool: tool.name, error });
    result = errorEnvelope(error);
  }

  const durationMs = Date.now() - started;
  if (result.ok === false) {
    log.warn('Инструмент вернул отказ', {
      tool: tool.name,
      code: result.error.code,
      duration_ms: durationMs,
    });
  } else {
    log.debug('Инструмент выполнен', { tool: tool.name, duration_ms: durationMs });
  }
  return toCallToolResult(result, { hasOutputSchema: tool.outputSchema !== undefined });
}

/**
 * Регистрирует инструменты в `McpServer`.
 * Повторное имя — дефект сборки реестра (WP11), поэтому бросаем на старте, а не в рантайме.
 */
export function registerTools(server: McpServer, ctx: ToolContext, tools: ToolDef[]): void {
  const seen = new Set<string>();
  for (const tool of tools) {
    if (seen.has(tool.name)) {
      throw new Error(`Инструмент «${tool.name}» зарегистрирован дважды: имена должны быть уникальны.`);
    }
    seen.add(tool.name);

    // Без outputSchema поле не передаём вовсе: `undefined` в конфиге SDK трактует как «схемы нет».
    const config =
      tool.outputSchema === undefined
        ? {
            title: tool.title,
            description: tool.description,
            inputSchema: tool.inputSchema,
            annotations: tool.annotations,
          }
        : {
            title: tool.title,
            description: tool.description,
            inputSchema: tool.inputSchema,
            outputSchema: tool.outputSchema,
            annotations: tool.annotations,
          };

    // SDK сам валидирует аргументы по inputSchema и приводит их к объекту.
    server.registerTool(tool.name, config, async (args: unknown) => {
      const record = typeof args === 'object' && args !== null ? (args as Record<string, any>) : {};
      return runTool(tool, ctx, record);
    });
  }
}
