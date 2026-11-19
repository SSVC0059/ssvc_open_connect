#!/usr/bin/env node
/**
 * Точка входа сервиса: конфиг → зависимости → HTTP → ожидание сигналов.
 *
 * Порядок важен: `.env` читается до `loadConfig`, обязательные переменные проверяются
 * до открытия порта («открытый порт без токена» недопустим, mcp-server.md §3.9).
 *
 * WP11 подключает здесь собранный каталог (`src/tools/index.ts`): `allTools` — это все
 * инструменты всех доменов одним массивом, а промпты и ресурсы подключаются в
 * `registerSessionCatalog` там же (её вызывает HTTP-слой на каждую сессию).
 * Сервер создаётся на сессию внутри `http/app.ts`, поэтому `startService` принимает
 * список определений, а не готовый `McpServer`.
 */

import { realpathSync } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { ConfigError, type Config } from './config.js';
import { createApp, type AppHandle } from './http/app.js';
import type { Logger } from './logging.js';
import { createRuntime } from './runtime.js';
import { startStdioService } from './stdio.js';
import type { ToolContext, ToolDef } from './mcp/types.js';
import { allTools } from './tools/index.js';

export interface StartServiceOptions {
  /** Окружение; по умолчанию `process.env`. */
  env?: NodeJS.ProcessEnv;
  /** Инструменты доменов: WP11 передаёт собранный каталог. */
  tools?: ToolDef[];
  /** Путь к `.env`, если файл лежит не рядом с процессом. */
  envFile?: string;
}

export interface StartedService {
  cfg: Config;
  log: Logger;
  ctx: ToolContext;
  app: AppHandle;
  address: { host: string; port: number };
  /** Останавливает сервис: сессии, HTTP, аудит, сырые логи. */
  stop(): Promise<void>;
}

/**
 * Собирает и запускает сервис. Бросает `ConfigError` с русским сообщением,
 * если конфигурация неполная.
 */
export async function startService(opts: StartServiceOptions = {}): Promise<StartedService> {
  const runtime = createRuntime({ env: opts.env, envFile: opts.envFile });
  const { cfg, log, ctx } = runtime;

  log.info('Запуск MCP-сервера OpenConnect', {
    device: cfg.device.url,
    bind: cfg.mcp.host,
    port: cfg.mcp.port,
    raw_log: cfg.raw.mode,
    allow_destructive: cfg.allowDestructive,
  });

  const app = await createApp({ cfg, ctx, tools: opts.tools ?? [] });
  const address = await app.listen();

  return {
    cfg,
    log,
    ctx,
    app,
    address,
    async stop(): Promise<void> {
      log.info('Остановка MCP-сервера');
      await app.close();
      await runtime.close();
    },
  };
}

/** Запускает stdio-режим для клиентов, которые сами спавнят процесс (DSH, IDE). */
async function mainStdio(): Promise<void> {
  let service: Awaited<ReturnType<typeof startStdioService>>;
  try {
    service = await startStdioService();
  } catch (error) {
    if (error instanceof ConfigError) {
      process.stderr.write(`[ssvc-openconnect-mcp] Ошибка конфигурации: ${error.message}\n`);
    } else {
      const message = error instanceof Error ? error.message : String(error);
      process.stderr.write(`[ssvc-openconnect-mcp] Не удалось запустить stdio-сервис: ${message}\n`);
    }
    process.exitCode = 1;
    return;
  }

  let stopping = false;
  const shutdown = (reason: string): void => {
    if (stopping) return;
    stopping = true;
    service.runtime.log.info('Завершение stdio-сервиса', { reason });
    void service
      .stop()
      .then(() => process.exit(0))
      .catch((error: unknown) => {
        service.runtime.log.error('Ошибка при остановке stdio-сервиса', { error });
        process.exit(1);
      });
  };
  process.on('SIGINT', () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));
  // DSH/IDE закрывают stdin, когда сессия завершена: не оставляем висящий процесс.
  process.stdin.on('end', () => shutdown('stdin closed'));

  service.runtime.log.info('Сервис готов', { transport: 'stdio' });
}

async function main(): Promise<void> {
  let service: StartedService;
  try {
    // Один каталог на все домены WP1–WP9: список инструментов, ресурсы и промпты
    // собирает `src/tools/index.ts`, включая проверку уникальности имён на старте.
    service = await startService({ tools: [...allTools] });
  } catch (error) {
    if (error instanceof ConfigError) {
      process.stderr.write(`[ssvc-openconnect-mcp] Ошибка конфигурации: ${error.message}\n`);
    } else {
      const message = error instanceof Error ? error.message : String(error);
      process.stderr.write(`[ssvc-openconnect-mcp] Не удалось запустить сервис: ${message}\n`);
    }
    process.exitCode = 1;
    return;
  }

  let stopping = false;
  const shutdown = (signal: string): void => {
    if (stopping) return;
    stopping = true;
    service.log.info('Получен сигнал завершения', { signal });
    void service
      .stop()
      .then(() => process.exit(0))
      .catch((error: unknown) => {
        service.log.error('Ошибка при остановке сервиса', { error });
        process.exit(1);
      });
  };
  process.on('SIGINT', () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));

  service.log.info('Сервис готов', {
    url: `http://${service.address.host}:${service.address.port}${'/mcp'}`,
    healthz: '/healthz',
    readyz: '/readyz',
  });
}

// Запускаем только при прямом вызове (`node dist/index.js`), не при импорте в тестах.
// Сравниваем реальные пути: при установке через symlink (`/usr/local/bin/ssvc-mcp`)
// `process.argv[1]` — это ссылка, а `import.meta.url` уже указывает на цель.
function isInvokedDirectly(): boolean {
  const entry = process.argv[1];
  if (entry === undefined) return false;
  try {
    return realpathSync(entry) === realpathSync(fileURLToPath(import.meta.url));
  } catch {
    return import.meta.url === pathToFileURL(entry).href;
  }
}

if (isInvokedDirectly()) {
  const useStdio =
    process.argv.includes('--stdio') || process.env.SSVC_MCP_TRANSPORT === 'stdio';
  void (useStdio ? mainStdio() : main());
}
