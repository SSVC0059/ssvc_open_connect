/**
 * Создание MCP-сервера на сессию (контракт §2.2, §3.4).
 *
 * Одна сессия — один `McpServer` и один транспорт: состояние подписок и уведомлений
 * привязано к сессии, поэтому режим stateful. Контекст инструментов (`ToolContext`)
 * общий на процесс: контроллер один, `DeviceClient`/кэш/аудит — тоже.
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { registerTools } from './registry.js';
import type { ToolContext, ToolDef } from './types.js';

export const SERVER_NAME = 'ssvc-openconnect-mcp';
export const SERVER_VERSION = '0.1.0';

/** Инструкции видны клиенту в ответе `initialize`; пишем по-русски и без секретов. */
export const SERVER_INSTRUCTIONS =
  'MCP-сервер контроллера OpenConnect (SSVC). Инструменты oc_* читают состояние и ' +
  'управляют процессом ректификации через REST контроллера. Один сервер — один контроллер: ' +
  'параметра «device» нет. Опасные действия требуют confirm:true, разрушительные — ' +
  'дополнительно reason и разрешены только при SSVC_ALLOW_DESTRUCTIVE=1. ' +
  'Команды процесса асинхронны: успешный ответ означает «принято в очередь», а не «выполнено».';

export interface CreateMcpServerOptions {
  ctx: ToolContext;
  /** Инструменты домена; WP0 регистрирует пустой список, WP11 подключает собранный каталог. */
  tools?: ToolDef[];
  version?: string;
}

export function createMcpServer(opts: CreateMcpServerOptions): McpServer {
  const server = new McpServer(
    {
      name: SERVER_NAME,
      title: 'MCP-сервер OpenConnect',
      version: opts.version ?? SERVER_VERSION,
    },
    { instructions: SERVER_INSTRUCTIONS },
  );
  registerTools(server, opts.ctx, opts.tools ?? []);
  return server;
}
