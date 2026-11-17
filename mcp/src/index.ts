/**
 * Точка входа сервиса: конфиг → зависимости → HTTP → ожидание сигналов.
 *
 * Порядок важен: `.env` читается до `loadConfig`, обязательные переменные проверяются
 * до открытия порта («открытый порт без токена» недопустим, mcp-server.md §3.9).
 *
 * WP11 подключает сюда собранный каталог инструментов (`src/tools/index.ts`): достаточно
 * передать массив в `startService`. Сейчас здесь подключён домен WP1 (информация,
 * возможности, состояние и телеметрия); сервер уже отдаёт его `tools/list`/`tools/call`.
 *
 * Инструменты WP1 собираются из тех же `ToolDef`, что регистрируют `registerInfoTools` и
 * `registerTelemetryTools`: сервер создаётся на сессию внутри `http/app.ts`, поэтому
 * `startService` принимает список определений, а не готовый `McpServer`.
 */

import { pathToFileURL } from 'node:url';
import { AuditLog } from './audit.js';
import { applyDotEnv, ConfigError, loadConfig, type Config } from './config.js';
import { DeviceClient } from './device/DeviceClient.js';
import { CapabilityCache } from './device/CapabilityCache.js';
import { RawLogger } from './device/RawLogger.js';
import { createApp, type AppHandle } from './http/app.js';
import { createLogger, type Logger } from './logging.js';
import type { ToolContext, ToolDef } from './mcp/types.js';
import { infoTools } from './tools/info.tools.js';
import { telemetryTools } from './tools/telemetry.tools.js';

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
  const env = opts.env ?? process.env;
  // Окружение приоритетнее файла: контейнер задаёт значения через env_file/variables.
  const envFile = opts.envFile ?? '.env';
  const applied = applyDotEnv(env, envFile);
  const cfg = loadConfig(env);

  const log = createLogger({ level: cfg.logLevel });
  if (applied.applied.length > 0) {
    // Имена переменных безопасны; значения (секреты) не логируем.
    log.debug('Прочитан файл конфигурации', { file: envFile, keys: applied.applied });
  }
  log.info('Запуск MCP-сервера OpenConnect', {
    device: cfg.device.url,
    bind: cfg.mcp.host,
    port: cfg.mcp.port,
    raw_log: cfg.raw.mode,
    allow_destructive: cfg.allowDestructive,
    allow_debug: cfg.allowDebug,
  });

  const audit = new AuditLog({ path: cfg.auditPath, log });
  const raw = new RawLogger({
    mode: cfg.raw.mode,
    dir: cfg.raw.dir,
    maxMb: cfg.raw.maxMb,
    log,
  });
  const client = new DeviceClient({ cfg, raw, log });
  const caps = new CapabilityCache(client, { log });

  const ctx: ToolContext = { cfg, client, caps, log, audit, raw };
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
      await client.close();
      await raw.close();
      await audit.close();
    },
  };
}

async function main(): Promise<void> {
  let service: StartedService;
  try {
    // Домен WP1: информация, возможности, состояние/телеметрия и диагностика сервера.
    const tools: ToolDef[] = [...infoTools(), ...telemetryTools()];
    service = await startService({ tools });
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
const invokedDirectly =
  process.argv[1] !== undefined && import.meta.url === pathToFileURL(process.argv[1]).href;
if (invokedDirectly) {
  void main();
}
