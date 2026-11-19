/**
 * Общий runtime MCP-сервера: конфиг, аудит, сырой лог, клиент устройства и кэш.
 *
 * Вынесен из `index.ts`, чтобы HTTP-транспорт и stdio-транспорт (DSH, локальные
 * MCP-клиенты) поднимали ровно один и тот же набор зависимостей и не расходились
 * в проверках конфигурации.
 */

import { AuditLog } from './audit.js';
import { applyDotEnv, loadConfig, type Config } from './config.js';
import { CapabilityCache } from './device/CapabilityCache.js';
import { DeviceClient } from './device/DeviceClient.js';
import { RawLogger } from './device/RawLogger.js';
import { createLogger, type Logger } from './logging.js';
import type { ToolContext } from './mcp/types.js';

export interface RuntimeOptions {
  /** Окружение; по умолчанию `process.env`. */
  env?: NodeJS.ProcessEnv;
  /** Путь к `.env`, если файл лежит не рядом с процессом. */
  envFile?: string;
  /** Куда писать структурный лог. Для stdio обязательно stderr, иначе протокол ломается. */
  logSink?: (line: string) => void;
  /** Транспорт: в stdio токен клиентов и пароль контроллера не обязательны. */
  transport?: 'http' | 'stdio';
}

export interface Runtime {
  cfg: Config;
  log: Logger;
  ctx: ToolContext;
  /** Закрывает клиент устройства, сырой лог и аудит. */
  close(): Promise<void>;
}

/** Собирает зависимости сервера. Бросает `ConfigError` при неполной конфигурации. */
export function createRuntime(opts: RuntimeOptions = {}): Runtime {
  const env = opts.env ?? process.env;
  const envFile = opts.envFile ?? '.env';
  // Окружение приоритетнее файла: контейнер задаёт значения через env_file/variables.
  applyDotEnv(env, envFile);
  const cfg = loadConfig(env, { transport: opts.transport });

  const log = createLogger({ level: cfg.logLevel, sink: opts.logSink });
  const audit = new AuditLog({ path: cfg.auditPath, log });
  const raw = new RawLogger({ mode: cfg.raw.mode, dir: cfg.raw.dir, maxMb: cfg.raw.maxMb, log });
  const client = new DeviceClient({ cfg, raw, log });
  const caps = new CapabilityCache(client, { log });
  const ctx: ToolContext = { cfg, client, caps, log, audit, raw };

  return {
    cfg,
    log,
    ctx,
    async close(): Promise<void> {
      await client.close();
      await raw.close();
      await audit.close();
    },
  };
}
