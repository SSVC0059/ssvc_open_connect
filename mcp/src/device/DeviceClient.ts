/**
 * DeviceClient — ядро надёжности (контракт §2.4, §3.5).
 *
 * Что гарантирует доменам:
 *  - авторизация: ленивый signIn, кэш JWT, один повтор после 401;
 *    404 на verifyAuthorization = security off (запросы анонимно);
 *  - ретраи: только GET (сеть, 502–504, 503), экспоненциальный backoff, 3 попытки;
 *    POST/PUT/DELETE не повторяются никогда;
 *  - запись: `withWrite` сериализует изменения (один писатель на устройство);
 *  - ошибки: 401/403/404/503 → коды §2.1, тело в `details.body` (усечённое);
 *  - сырой лог: каждый обмен уходит в RawLogger согласно SSVC_RAW_LOG;
 *  - валидация: ответ разбирается zod-схемой, расхождение → `device_error` с телом.
 */

import { z } from 'zod';
import type { Config } from '../config.js';
import { OcError, httpError, truncateBody } from '../errors.js';
import { redactText, type Logger } from '../logging.js';
import { AuthSession, type AuthStats } from './AuthSession.js';
import { createNodeHttp, isNetworkError, type HttpLike, type HttpResult } from './http.js';
import type { RawSink } from './RawLogger.js';
import { WriteQueue } from './WriteQueue.js';
import {
  bodyForDetails,
  headerValue,
  isTextPlain,
  parseJsonBody,
  textResponseSchema,
} from './schemas/common.js';

export interface RequestOptions {
  query?: Record<string, string | number | boolean | undefined>;
  timeoutMs?: number;
  /** Без Authorization (signIn и анонимные проверки). */
  anonymous?: boolean;
  /** CSV/текст как есть. */
  rawBody?: string;
  headers?: Record<string, string>;
  /** Отключить single-flight для GET (редкие случаи: статус передачи журнала). */
  noSingleFlight?: boolean;
  /**
   * Запросить сырые байты ответа (`HttpResponse.bodyBytes`).
   * Нужно только скачиванию файлов WP9: `body` — UTF-8-строка и бинарный файл
   * через неё не восстановить.
   */
  captureBytes?: boolean;
}

export interface HttpResponse {
  status: number;
  headers: Record<string, string>;
  body: string;
  /** Сырые байты ответа; заполняется только при `captureBytes:true`. */
  bodyBytes?: Buffer;
  ms: number;
}

export interface CommandAccepted {
  accepted: true;
  command: string;
  warnings: string[];
  raw: string;
}

export interface DeviceClientOptions {
  cfg: Config;
  http?: HttpLike;
  raw?: RawSink;
  log?: Logger;
  now?: () => number;
  /** Настройки ретраев (в тестах уменьшаются, чтобы не ждать). */
  retry?: { attempts?: number; baseDelayMs?: number; maxDelayMs?: number };
  auth?: AuthSession;
}

export interface DeviceClientStats {
  requests: number;
  retries: number;
  authRetries: number;
  writeOperations: number;
  errors: number;
  lastStatus: number | null;
  lastRequestAt: string | null;
}

const USER_AGENT = 'ssvc-openconnect-mcp/0.1.0';

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => {
    const timer = setTimeout(resolve, ms);
    timer.unref?.();
  });
}

export class DeviceClient {
  readonly deviceUrl: string;
  readonly auth: AuthSession;
  private readonly http: HttpLike;
  private readonly raw?: RawSink;
  private readonly log?: Logger;
  private readonly now: () => number;
  private readonly retryAttempts: number;
  private readonly retryBaseDelayMs: number;
  private readonly retryMaxDelayMs: number;
  private readonly defaultTimeoutMs: number;
  private readonly writeQueue: WriteQueue;
  private readonly inflight = new Map<string, Promise<HttpResponse>>();
  private statsValue: DeviceClientStats = {
    requests: 0,
    retries: 0,
    authRetries: 0,
    writeOperations: 0,
    errors: 0,
    lastStatus: null,
    lastRequestAt: null,
  };

  constructor(opts: DeviceClientOptions) {
    this.deviceUrl = opts.cfg.device.url;
    this.log = opts.log;
    this.raw = opts.raw;
    this.now = opts.now ?? Date.now;
    this.retryAttempts = Math.max(1, opts.retry?.attempts ?? 3);
    this.retryBaseDelayMs = Math.max(0, opts.retry?.baseDelayMs ?? 200);
    this.retryMaxDelayMs = Math.max(this.retryBaseDelayMs, opts.retry?.maxDelayMs ?? 2_000);
    this.defaultTimeoutMs = opts.cfg.device.timeoutMs;
    this.writeQueue = new WriteQueue(opts.log);
    const http = opts.http ?? createNodeHttp({ log: opts.log });
    this.http = http;
    this.auth =
      opts.auth ??
      new AuthSession({
        http,
        baseUrl: opts.cfg.device.url,
        user: opts.cfg.device.user,
        password: opts.cfg.device.password,
        timeoutMs: opts.cfg.device.timeoutMs,
        log: opts.log,
        raw: opts.raw,
        now: this.now,
        userAgent: USER_AGENT,
      });
  }

  getStats(): DeviceClientStats & { auth: AuthStats; writes: { size: number; completed: number } } {
    return {
      ...this.statsValue,
      auth: this.auth.stats(),
      writes: { size: this.writeQueue.size, completed: this.writeQueue.completedCount },
    };
  }

  /** Строит полный URL с query-параметрами (undefined-значения пропускаются). */
  buildUrl(path: string, query?: RequestOptions['query']): string {
    const normalizedPath = path.startsWith('/') ? path : `/${path}`;
    const params = new URLSearchParams();
    for (const [key, value] of Object.entries(query ?? {})) {
      if (value === undefined) continue;
      params.append(key, typeof value === 'boolean' ? String(value) : String(value));
    }
    const suffix = params.size > 0 ? `?${params.toString()}` : '';
    return `${this.deviceUrl}${normalizedPath}${suffix}`;
  }

  async get<T>(path: string, schema: z.ZodType<T>, opts: RequestOptions = {}): Promise<T> {
    const response = await this.fetchText('GET', path, undefined, opts);
    this.ensureOk(response, 'GET', path);
    return this.parseResponse<T>(response, schema, 'GET', path);
  }

  async post<T>(
    path: string,
    body: unknown,
    schema: z.ZodType<T>,
    opts: RequestOptions = {},
  ): Promise<T> {
    const response = await this.request('POST', path, body, opts);
    this.ensureOk(response, 'POST', path);
    return this.parseResponse<T>(response, schema, 'POST', path);
  }

  async put<T>(
    path: string,
    body: unknown,
    schema: z.ZodType<T>,
    opts: RequestOptions = {},
  ): Promise<T> {
    const response = await this.request('PUT', path, body, opts);
    this.ensureOk(response, 'PUT', path);
    return this.parseResponse<T>(response, schema, 'PUT', path);
  }

  async del<T>(
    path: string,
    body: unknown,
    schema: z.ZodType<T>,
    opts: RequestOptions = {},
  ): Promise<T> {
    const response = await this.request('DELETE', path, body, opts);
    this.ensureOk(response, 'DELETE', path);
    return this.parseResponse<T>(response, schema, 'DELETE', path);
  }

  /**
   * POST /rest/commands с разбором 200/409/503/501.
   * 200 = команда принята в очередь (не выполнена) — так и отдаём наружу.
   */
  async sendCommand(name: string, parameters = ''): Promise<CommandAccepted> {
    const path = '/rest/commands';
    const response = await this.request('POST', path, { commands: name, parameters });
    const body = response.body;
    const baseDetails = { command: name, status: response.status, body: truncateBody(body) };

    switch (response.status) {
      case 200:
      case 201:
      case 202: {
        const parsed = parseJsonBody(body);
        const warnings: string[] = [];
        if (parsed.ok && typeof parsed.value === 'object' && parsed.value !== null) {
          const candidate = (parsed.value as { warnings?: unknown }).warnings;
          if (Array.isArray(candidate)) {
            warnings.push(...candidate.filter((item): item is string => typeof item === 'string'));
          }
        }
        this.log?.info('Команда принята контроллером', { command: name, warnings: warnings.length });
        return { accepted: true, command: name, warnings, raw: body };
      }
      case 409: {
        const parsed = parseJsonBody(body);
        const value = parsed.ok ? parsed.value : null;
        const record = typeof value === 'object' && value !== null ? (value as Record<string, unknown>) : null;
        const blockReasons = Array.isArray(record?.blockReasons)
          ? (record?.blockReasons as unknown[]).filter((item): item is string => typeof item === 'string')
          : null;
        if (blockReasons !== null) {
          const reasonsText = blockReasons.length > 0 ? blockReasons.join('; ') : 'причина не указана';
          throw new OcError(
            'guard_blocked',
            `Контроллер отклонил команду «${name}»: сработал предохранитель`,
            {
              hint:
                'Устраните причины блокировки (blockReasons) и повторите команду: ' +
                'проверьте датчики уровня XKC-25, их калибровку и аппаратные сбои.',
              details: { ...baseDetails, blockReasons, reasons_text: reasonsText },
            },
          );
        }
        throw new OcError(
          'feature_unsupported',
          `Команда «${name}» не поддерживается этой версией прошивки SSVC`,
          {
            hint:
              'Обновите прошивку SSVC до версии с нужной командой (или не используйте её). ' +
              'Повтор запроса не поможет: ограничение версии UART API.',
            details: baseDetails,
          },
        );
      }
      case 501:
        throw new OcError(
          'unsupported_operation',
          `Команда «${name}» не реализована в этой прошивке`,
          {
            hint: 'Проверьте версию прошивки контроллера: команда появится только в новой версии.',
            details: baseDetails,
          },
        );
      case 503:
        if (/queue/i.test(body) || /queue/i.test(headerValue(response.headers, 'x-reason') ?? '')) {
          throw new OcError('queue_full', 'Очередь команд контроллера переполнена', {
            hint: 'Подождите несколько секунд и повторите: очередь разгрузится сама.',
            retryable: true,
            details: baseDetails,
          });
        }
        throw new OcError('busy', 'Контроллер занят и не принял команду', {
          hint: 'Повторите запрос через несколько секунд.',
          retryable: true,
          details: baseDetails,
        });
      default:
        throw httpError({ status: response.status, body, method: 'POST', path });
    }
  }

  /** Один писатель на устройство: записи выполняются строго последовательно. */
  async withWrite<T>(key: string, fn: () => Promise<T>): Promise<T> {
    return this.writeQueue.run(key, async () => {
      this.statsValue.writeOperations += 1;
      return fn();
    });
  }

  /**
   * Сырой запрос — для CSV, 202-поллинга и нестандартных случаев.
   * Не бросает на HTTP-статусах: возвращает их как есть (кроме сетевых ошибок).
   */
  async request(
    method: string,
    path: string,
    body?: unknown,
    opts: RequestOptions = {},
  ): Promise<HttpResponse> {
    const upper = method.toUpperCase();
    const canRetry = upper === 'GET';
    const attempts = canRetry ? this.retryAttempts : 1;
    const timeoutMs = opts.timeoutMs ?? this.defaultTimeoutMs;
    const url = this.buildUrl(path, opts.query);

    const payload =
      opts.rawBody !== undefined
        ? opts.rawBody
        : body === undefined
          ? undefined
          : JSON.stringify(body);

    const headers: Record<string, string> = {
      accept: 'application/json, text/plain',
      'user-agent': USER_AGENT,
      connection: 'close',
      ...(opts.headers ?? {}),
    };
    if (payload !== undefined && headerValue(headers, 'content-type') === undefined) {
      headers['content-type'] = opts.rawBody !== undefined ? 'text/plain; charset=utf-8' : 'application/json';
    }

    // Авторизация: один раз на запрос, ленивый signIn внутри AuthSession.
    let token: string | null = null;
    if (opts.anonymous !== true) {
      token = await this.auth.ensureToken();
    }
    let authRetried = false;

    for (let attempt = 1; attempt <= attempts; attempt += 1) {
      const started = this.now();
      let result: HttpResult;
      try {
        result = await this.http.request({
          method: upper,
          url,
          headers: this.withAuth(headers, token),
          ...(payload === undefined ? {} : { body: payload }),
          timeoutMs,
          ...(opts.captureBytes === true ? { captureBytes: true } : {}),
        });
      } catch (error) {
        const failure =
          error instanceof OcError
            ? error
            : new OcError('device_unreachable', 'Не удалось связаться с контроллером', {
                retryable: true,
                details: { method: upper, path, error: String(error) },
              });
        this.statsValue.errors += 1;
        this.recordRaw({ method: upper, path, request: payload, error: failure, started });
        if (canRetry && attempt < attempts && isNetworkError(error)) {
          this.statsValue.retries += 1;
          this.log?.warn('Повтор GET после сетевой ошибки', {
            path,
            attempt,
            error: redactText(failure.message),
          });
          await delay(this.backoff(attempt));
          continue;
        }
        throw failure;
      }

      this.statsValue.requests += 1;
      this.statsValue.lastStatus = result.status;
      this.statsValue.lastRequestAt = new Date(this.now()).toISOString();

      // 401: проверяем security off или повторяем вход один раз.
      if (result.status === 401 && opts.anonymous !== true && !authRetried) {
        authRetried = true;
        this.statsValue.authRetries += 1;
        const outcome = await this.auth.handleUnauthorized();
        if (outcome === 'retry') {
          token = this.auth.cachedToken;
          this.recordRaw({
            method: upper,
            path,
            status: result.status,
            response: result.body,
            request: payload,
            started,
          });
          this.log?.info('Повтор запроса после повторного входа', { path });
          attempt -= 1; // повтор не расходует попытку ретрая
          continue;
        }
        if (outcome === 'security_off') {
          token = null;
          this.recordRaw({
            method: upper,
            path,
            status: result.status,
            response: result.body,
            request: payload,
            started,
          });
          this.log?.info('Security на прошивке выключена: повтор без токена', { path });
          attempt -= 1;
          continue;
        }
      }

      this.recordRaw({
        method: upper,
        path,
        status: result.status,
        response: result.body,
        request: payload,
        started,
      });

      if (canRetry && attempt < attempts && this.isRetryableStatus(result.status)) {
        this.statsValue.retries += 1;
        this.log?.warn('Повтор GET после ответа устройства', { path, attempt, status: result.status });
        await delay(this.backoff(attempt));
        continue;
      }

      return {
        status: result.status,
        headers: result.headers,
        body: result.body,
        ...(result.bodyBytes === undefined ? {} : { bodyBytes: result.bodyBytes }),
        ms: result.ms,
      };
    }

    // Недостижимо: цикл либо вернёт результат, либо бросит.
    throw new OcError('device_unreachable', 'Не удалось выполнить запрос к контроллеру', {
      retryable: true,
      details: { method: upper, path, attempts },
    });
  }

  /** Закрывает клиент: сбрасывает single-flight и ждёт очередь записей. */
  async close(): Promise<void> {
    await this.writeQueue.drain();
    this.inflight.clear();
  }

  private withAuth(headers: Record<string, string>, token: string | null): Record<string, string> {
    if (token === null) return headers;
    return { ...headers, authorization: `Bearer ${token}` };
  }

  private backoff(attempt: number): number {
    return Math.min(this.retryMaxDelayMs, this.retryBaseDelayMs * 2 ** (attempt - 1));
  }

  private isRetryableStatus(status: number): boolean {
    // 503 у устройства — это либо queue_full, либо занятость; GET безопасно повторить.
    return status === 502 || status === 503 || status === 504;
  }

  /**
   * GET-обёртка с single-flight: одновременные вызовы одного URL дают один запрос.
   * Хранится именно текст ответа, разбор схемой — индивидуально для каждого вызова.
   */
  private async fetchText(
    method: string,
    path: string,
    body: unknown,
    opts: RequestOptions,
  ): Promise<HttpResponse> {
    if (method !== 'GET' || opts.noSingleFlight === true) {
      return this.request(method, path, body, opts);
    }
    const key = `${method} ${this.buildUrl(path, opts.query)}`;
    const existing = this.inflight.get(key);
    if (existing !== undefined) return existing;
    const task = this.request(method, path, body, opts).finally(() => {
      this.inflight.delete(key);
    });
    this.inflight.set(key, task);
    return task;
  }

  /** HTTP-статус → OcError по контракту §2.1. */
  private ensureOk(response: HttpResponse, method: string, path: string): void {
    if (response.status >= 200 && response.status < 300) return;
    this.statsValue.errors += 1;
    throw httpError({ status: response.status, body: response.body, method, path });
  }

  /** Разбор ответа домена: JSON + zod, иначе `device_error` с телом. */
  private parseResponse<T>(
    response: HttpResponse,
    schema: z.ZodType<T>,
    method: string,
    path: string,
  ): T {
    const text = response.body;
    // text/plain с единственным полем raw: домен так просил (нестандартные случаи).
    if (isTextPlain(response.headers) && text.trim() !== '' && !text.trim().startsWith('{')) {
      const asRaw = textResponseSchema.safeParse({ raw: text });
      const parsedRaw = asRaw.success ? schema.safeParse(asRaw.data) : null;
      if (parsedRaw?.success === true) return parsedRaw.data;
    }

    const parsed = parseJsonBody(text);
    if (!parsed.ok) {
      throw new OcError('device_error', `Ответ устройства на ${method} ${path} не является JSON`, {
        hint:
          'Скорее всего прошивка другой версии или эндпоинт вернул текст. ' +
          'Включите SSVC_RAW_LOG=all и посмотрите сырой обмен.',
        details: {
          status: response.status,
          parse_error: parsed.error,
          body: bodyForDetails(text),
        },
      });
    }
    const result = schema.safeParse(parsed.value);
    if (!result.success) {
      throw new OcError(
        'device_error',
        `Ответ устройства на ${method} ${path} не соответствует ожидаемой схеме`,
        {
          hint:
            'Расхождение контракта с прошивкой: проверьте версию контроллера и поля ответа ' +
            'в details.issues / details.body.',
          details: {
            status: response.status,
            issues: result.error.issues.map((issue) => ({
              path: issue.path.join('.'),
              message: issue.message,
            })),
            body: bodyForDetails(text),
          },
        },
      );
    }
    return result.data;
  }

  private recordRaw(input: {
    method: string;
    path: string;
    request?: unknown;
    response?: unknown;
    status?: number;
    error?: unknown;
    started: number;
  }): void {
    if (this.raw === undefined) return;
    void this.raw.record({
      method: input.method,
      path: input.path,
      status: input.status,
      ms: this.now() - input.started,
      request: input.request,
      response: input.response,
      error: input.error,
    });
  }
}
