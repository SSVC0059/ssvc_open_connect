/**
 * stdio-транспорт MCP: для клиентов, которые сами запускают сервер (DSH, Claude
 * Desktop, IDE). В отличие от HTTP здесь нет порта и Bearer-токена: процесс
 * общается с клиентом по stdin/stdout, а весь лог уходит в stderr.
 *
 * `SSVC_MCP_TOKEN` в stdio не нужен и не обязателен: доступ ограничен самим
 * фактом запуска процесса клиентом. Пароль контроллера тоже можно не задавать,
 * если на прошивке выключена авторизация.
 */

import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { createMcpServer } from './mcp/server.js';
import { createRuntime, type Runtime, type RuntimeOptions } from './runtime.js';
import { allTools, registerSessionCatalog } from './tools/index.js';

export interface StartedStdioService {
  runtime: Runtime;
  server: ReturnType<typeof createMcpServer>;
  stop(): Promise<void>;
}

/**
 * Запускает MCP-сервер поверх stdin/stdout.
 *
 * Важно: `process.stdout` принадлежит протоколу. Любой `console.log` или логгер
 * с sink по умолчанию делает поток невалидным, поэтому sink принудительно stderr.
 */
export async function startStdioService(
  opts: RuntimeOptions = {},
): Promise<StartedStdioService> {
  const runtime = createRuntime({
    ...opts,
    // stdio-режим не требует ни токена клиентов, ни порта: транспорт задаём жёстко.
    transport: 'stdio',
    logSink: opts.logSink ?? ((line: string) => process.stderr.write(`${line}\n`)),
  });

  runtime.log.info('Запуск MCP-сервера OpenConnect (stdio)', {
    device: runtime.cfg.device.url,
    authenticated: runtime.cfg.device.password !== '',
    allow_destructive: runtime.cfg.allowDestructive,
  });

  const server = createMcpServer({ ctx: runtime.ctx, tools: [...allTools] });
  registerSessionCatalog(server, runtime.ctx);

  const transport = new StdioServerTransport();
  transport.onerror = (error: Error) => {
    runtime.log.warn('Ошибка stdio-транспорта MCP', { error: error.message });
  };
  await server.connect(transport);

  let stopping = false;
  const stop = async (): Promise<void> => {
    if (stopping) return;
    stopping = true;
    await server.close();
    await runtime.close();
  };

  return { runtime, server, stop };
}
