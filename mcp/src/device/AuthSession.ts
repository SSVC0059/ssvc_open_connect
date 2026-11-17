/**
 * Аутентификация на контроллере: ленивый signIn, кэш JWT в памяти, один повтор после 401.
 *
 * Контракт §2.4/§3.5:
 *  - ленивый `POST /rest/signIn` при первом запросе, JWT кэшируется в памяти;
 *  - `404` на `GET /rest/verifyAuthorization` = security выключена на прошивке,
 *    токен не нужен, все запросы идут анонимно;
 *  - `401` от API → проверка `verifyAuthorization` и ровно один повтор после повторного входа.
 *
 * JWT прошивки не содержит `exp`, поэтому «протухание» определяется только ответом 401.
 */

import { OcError } from '../errors.js';
import type { Logger } from '../logging.js';
import { httpError, truncateBody } from '../errors.js';
import type { HttpLike } from './http.js';
import type { RawLogInput, RawSink } from './RawLogger.js';

export interface AuthSessionOptions {
  http: HttpLike;
  baseUrl: string;
  user: string;
  password: string;
  timeoutMs: number;
  log?: Logger;
  raw?: RawSink;
  now?: () => number;
  /** Общий User-Agent для запросов авторизации. */
  userAgent?: string;
}

export type VerifyStatus = 'ok' | 'invalid' | 'security_off' | 'unreachable';

export interface VerifyResult {
  status: VerifyStatus;
  httpStatus?: number;
  error?: string;
}

/** Что делать DeviceClient после 401. */
export type UnauthorizedOutcome = 'retry' | 'security_off' | 'no_retry';

export interface AuthStats {
  authenticated: boolean;
  securityOff: boolean;
  signIns: number;
  relogins: number;
  tokenAgeMs: number | null;
  lastSignInAt: string | null;
}

interface SignInResponse {
  access_token?: unknown;
  token?: unknown;
}

export class AuthSession {
  private readonly http: HttpLike;
  private readonly baseUrl: string;
  private readonly user: string;
  private readonly password: string;
  private readonly timeoutMs: number;
  private readonly log?: Logger;
  private readonly raw?: RawSink;
  private readonly now: () => number;
  private readonly userAgent: string;

  private token: string | null = null;
  private tokenAt: number | null = null;
  private securityOff = false;
  private inflight: Promise<string | null> | null = null;
  private signInCount = 0;
  private reloginCount = 0;

  constructor(opts: AuthSessionOptions) {
    this.http = opts.http;
    this.baseUrl = opts.baseUrl.replace(/\/+$/, '');
    this.user = opts.user;
    this.password = opts.password;
    this.timeoutMs = opts.timeoutMs;
    this.log = opts.log;
    this.raw = opts.raw;
    this.now = opts.now ?? Date.now;
    this.userAgent = opts.userAgent ?? 'ssvc-openconnect-mcp/0.1.0';
  }

  /** Текущий токен без сетевого вызова. */
  get cachedToken(): string | null {
    return this.token;
  }

  get isSecurityOff(): boolean {
    return this.securityOff;
  }

  stats(): AuthStats {
    return {
      authenticated: this.token !== null,
      securityOff: this.securityOff,
      signIns: this.signInCount,
      relogins: this.reloginCount,
      tokenAgeMs: this.tokenAt === null ? null : Math.max(0, this.now() - this.tokenAt),
      lastSignInAt: this.tokenAt === null ? null : new Date(this.tokenAt).toISOString(),
    };
  }

  /**
   * Возвращает токен для запроса (или null, если security на прошивке выключена).
   * Одновременные вызовы разделяют один signIn (single-flight).
   */
  async ensureToken(): Promise<string | null> {
    if (this.token !== null) return this.token;
    if (this.securityOff) return null;
    if (this.inflight === null) {
      this.inflight = this.signIn().finally(() => {
        this.inflight = null;
      });
    }
    return this.inflight;
  }

  /** Логин по логину/паролю. При 404 считает security выключенной. */
  async signIn(): Promise<string | null> {
    const path = '/rest/signIn';
    const started = this.now();
    const body = JSON.stringify({ username: this.user, password: this.password });
    let result;
    try {
      result = await this.http.request({
        method: 'POST',
        url: `${this.baseUrl}${path}`,
        headers: {
          'content-type': 'application/json',
          accept: 'application/json',
          'user-agent': this.userAgent,
          connection: 'close',
        },
        body,
        timeoutMs: this.timeoutMs,
      });
    } catch (error) {
      this.recordRaw(path, 'POST', { request: body, error, started });
      throw error;
    }
    this.recordRaw(path, 'POST', {
      request: body,
      status: result.status,
      response: result.body,
      started,
    });

    if (result.status === 404) {
      // На не-secure сборке прошивки эндпоинта логина нет: работаем анонимно.
      this.securityOff = true;
      this.token = null;
      this.tokenAt = null;
      this.log?.warn('Прошивка без аутентификации: работаем без токена', { path });
      return null;
    }
    if (result.status === 401 || result.status === 403) {
      throw new OcError('auth_failed', 'Контроллер отклонил учётные данные', {
        hint:
          'Проверьте SSVC_USER и SSVC_PASSWORD в .env: обычно нужна учётная запись admin. ' +
          'После изменения пароля перезапустите сервис.',
        httpStatus: result.status,
        details: { status: result.status, body: truncateBody(result.body) },
      });
    }
    if (result.status < 200 || result.status >= 300) {
      throw httpError({ status: result.status, body: result.body, method: 'POST', path });
    }

    let parsed: SignInResponse;
    try {
      parsed = JSON.parse(result.body) as SignInResponse;
    } catch {
      throw new OcError('auth_failed', 'Контроллер вернул некорректный ответ на вход', {
        hint: 'Проверьте версию прошивки контроллера: ожидался JSON с полем access_token.',
        details: { body: truncateBody(result.body) },
      });
    }
    const token = parsed.access_token ?? parsed.token;
    if (typeof token !== 'string' || token.trim() === '') {
      throw new OcError('auth_failed', 'Контроллер не выдал access_token', {
        hint: 'Проверьте версию прошивки и учётные данные контроллера.',
        details: { body: truncateBody(result.body) },
      });
    }

    this.token = token;
    this.tokenAt = this.now();
    this.securityOff = false;
    this.signInCount += 1;
    this.log?.info('Вход на контроллер выполнен', { user: this.user });
    return this.token;
  }

  /** Проверка токена: 200 — ok, 401 — invalid, 404 — security off. */
  async verify(): Promise<VerifyResult> {
    const path = '/rest/verifyAuthorization';
    const token = this.token;
    const headers: Record<string, string> = {
      accept: 'application/json, text/plain',
      'user-agent': this.userAgent,
      connection: 'close',
    };
    if (token !== null) headers.authorization = `Bearer ${token}`;

    const started = this.now();
    let result;
    try {
      result = await this.http.request({
        method: 'GET',
        url: `${this.baseUrl}${path}`,
        headers,
        timeoutMs: this.timeoutMs,
      });
    } catch (error) {
      this.recordRaw(path, 'GET', { error, started });
      return {
        status: 'unreachable',
        error: error instanceof Error ? error.message : String(error),
      };
    }
    this.recordRaw(path, 'GET', {
      status: result.status,
      response: result.body,
      started,
    });

    if (result.status === 404) {
      this.securityOff = true;
      this.token = null;
      this.tokenAt = null;
      this.log?.warn('verifyAuthorization вернул 404: security на прошивке выключена');
      return { status: 'security_off', httpStatus: 404 };
    }
    if (result.status === 401 || result.status === 403) {
      return { status: 'invalid', httpStatus: result.status };
    }
    if (result.status >= 200 && result.status < 300) {
      return { status: 'ok', httpStatus: result.status };
    }
    return { status: 'unreachable', httpStatus: result.status, error: truncateBody(result.body) };
  }

  /** Сбрасывает кэш токена (без сетевого вызова). */
  invalidate(): void {
    this.token = null;
    this.tokenAt = null;
  }

  /** Полный сброс состояния: например, после смены пароля или рестарта контроллера. */
  reset(): void {
    this.invalidate();
    this.securityOff = false;
  }

  /**
   * Обработка 401 от API: проверяем, не выключена ли security, иначе — повторный вход.
   * Возвращает решение для DeviceClient.
   */
  async handleUnauthorized(): Promise<UnauthorizedOutcome> {
    if (this.securityOff) return 'security_off';
    const verdict = await this.verify();
    if (verdict.status === 'security_off') return 'security_off';
    if (verdict.status === 'invalid') {
      this.invalidate();
      try {
        this.reloginCount += 1;
        await this.signIn();
        return 'retry';
      } catch (error) {
        this.log?.error('Повторный вход после 401 не удался', { error });
        throw error;
      }
    }
    return 'no_retry';
  }

  private recordRaw(
    path: string,
    method: string,
    data: {
      request?: unknown;
      response?: unknown;
      status?: number;
      error?: unknown;
      started: number;
    },
  ): void {
    if (this.raw === undefined) return;
    const input: RawLogInput = {
      method,
      path,
      ms: this.now() - data.started,
    };
    if (data.status !== undefined) input.status = data.status;
    if (data.request !== undefined) input.request = data.request;
    if (data.response !== undefined) input.response = data.response;
    if (data.error !== undefined) input.error = data.error;
    void this.raw.record(input);
  }
}
