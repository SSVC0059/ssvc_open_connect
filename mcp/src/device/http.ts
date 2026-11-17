/**
 * Тонкий HTTP-слой к контроллеру поверх `node:http`.
 *
 * Зачем свой слой, а не fetch:
 *  - нужен предсказуемый общий таймаут (connect+read) и отмена по AbortSignal;
 *  - нужен интерфейс `HttpLike`, который в тестах подменяется двойником
 *    `test/fakeHttp.ts` без реальной сети;
 *  - тело ответа всегда строка (прошивка отвечает и JSON, и text/plain).
 */

import { request as httpRequest } from 'node:http';
import { request as httpsRequest } from 'node:https';
import { OcError } from '../errors.js';
import type { Logger } from '../logging.js';

export interface HttpRequestOptions {
  method: string;
  url: string;
  headers?: Record<string, string>;
  body?: string;
  timeoutMs?: number;
  signal?: AbortSignal;
  /** Предел размера ответа: защита от «бесконечного» CSV в памяти. */
  maxResponseBytes?: number;
}

export interface HttpResult {
  status: number;
  headers: Record<string, string>;
  body: string;
  ms: number;
}

export interface HttpLike {
  request(opts: HttpRequestOptions): Promise<HttpResult>;
}

export interface NodeHttpOptions {
  log?: Logger;
  /** По умолчанию 64 МБ: журналы ректификации бывают по несколько мегабайт. */
  maxResponseBytes?: number;
}

const DEFAULT_MAX_RESPONSE_BYTES = 64 * 1024 * 1024;

/** Сетевые коды, после которых повтор осмыслен. */
const NETWORK_ERROR_CODES = new Set([
  'ECONNREFUSED',
  'ECONNRESET',
  'EHOSTUNREACH',
  'ENETUNREACH',
  'ENOTFOUND',
  'EAI_AGAIN',
  'EPIPE',
  'ETIMEDOUT',
  'UND_ERR_CONNECT_TIMEOUT',
  'UND_ERR_SOCKET',
]);

export function isNetworkError(error: unknown): boolean {
  const code = (error as NodeJS.ErrnoException | undefined)?.code;
  return typeof code === 'string' && NETWORK_ERROR_CODES.has(code);
}

function normalizeHeaders(raw: NodeJS.IncomingHttpHeaders): Record<string, string> {
  const out: Record<string, string> = {};
  for (const [key, value] of Object.entries(raw)) {
    if (value === undefined) continue;
    out[key.toLowerCase()] = Array.isArray(value) ? value.join(', ') : value;
  }
  return out;
}

/**
 * Реализация `HttpLike` через node:http/node:https.
 * Соединение не переиспользуется (`agent: false`): ESP32 не любит «висящие» keep-alive сокеты,
 * а частота запросов у нас низкая.
 */
export function createNodeHttp(opts: NodeHttpOptions = {}): HttpLike {
  const maxResponseBytes = opts.maxResponseBytes ?? DEFAULT_MAX_RESPONSE_BYTES;

  return {
    request(input: HttpRequestOptions): Promise<HttpResult> {
      const started = Date.now();
      const url = new URL(input.url);
      const transport = url.protocol === 'https:' ? httpsRequest : httpRequest;
      const timeoutMs = input.timeoutMs ?? 10_000;
      const limit = input.maxResponseBytes ?? maxResponseBytes;

      return new Promise<HttpResult>((resolve, reject) => {
        let settled = false;
        const finish = (fn: () => void): void => {
          if (settled) return;
          settled = true;
          clearTimeout(timer);
          fn();
        };

        const req = transport(
          {
            protocol: url.protocol,
            hostname: url.hostname,
            port: url.port === '' ? undefined : url.port,
            path: `${url.pathname}${url.search}`,
            method: input.method.toUpperCase(),
            headers: input.headers,
            agent: false,
          },
          (res) => {
            const chunks: Buffer[] = [];
            let received = 0;
            res.on('data', (chunk: Buffer) => {
              received += chunk.length;
              if (received > limit) {
                res.destroy();
                finish(() =>
                  reject(
                    new OcError('device_error', 'Ответ устройства слишком большой', {
                      hint: 'Контроллер вернул больше данных, чем сервер готов принять.',
                      details: { method: input.method, path: url.pathname, limit_bytes: limit },
                    }),
                  ),
                );
                return;
              }
              chunks.push(chunk);
            });
            res.on('end', () => {
              finish(() =>
                resolve({
                  status: res.statusCode ?? 0,
                  headers: normalizeHeaders(res.headers),
                  body: Buffer.concat(chunks).toString('utf8'),
                  ms: Date.now() - started,
                }),
              );
            });
            res.on('error', (error) => {
              finish(() => reject(toTransportError(error, input.method, url.pathname, timeoutMs)));
            });
          },
        );

        // Общий таймаут запроса: и на подключение, и на чтение ответа.
        const timer = setTimeout(() => {
          req.destroy(
            Object.assign(new Error('request timeout'), { code: 'ETIMEDOUT' }),
          );
        }, timeoutMs);
        timer.unref?.();

        if (input.signal) {
          if (input.signal.aborted) {
            req.destroy(new Error('aborted'));
          } else {
            input.signal.addEventListener(
              'abort',
              () => {
                req.destroy(new Error('aborted'));
              },
              { once: true },
            );
          }
        }

        req.on('error', (error) => {
          finish(() => reject(toTransportError(error, input.method, url.pathname, timeoutMs)));
        });

        if (input.body !== undefined) req.write(input.body);
        req.end();
      });
    },
  };
}

function toTransportError(
  error: unknown,
  method: string,
  path: string,
  timeoutMs: number,
): OcError {
  if (error instanceof OcError) return error;
  const code = (error as NodeJS.ErrnoException | undefined)?.code;
  const details = { method, path, timeout_ms: timeoutMs, syscall_code: code ?? null };
  if (code === 'ETIMEDOUT' || (error instanceof Error && /timeout/i.test(error.message))) {
    return new OcError('device_timeout', 'Контроллер не ответил за отведённое время', {
      hint:
        'Проверьте, что контроллер включён и доступен по сети, затем повторите запрос. ' +
        'Для долгих операций (журналы, подсистемы) используйте больший таймаут.',
      retryable: true,
      details,
      cause: error,
    });
  }
  return new OcError('device_unreachable', 'Не удалось связаться с контроллером', {
    hint:
      'Проверьте SSVC_DEVICE_URL, питание и сеть контроллера. ' +
      'Сервис продолжает работать: повторите запрос, когда устройство станет доступно.',
    retryable: true,
    details,
    cause: error,
  });
}
