/**
 * Единая модель ошибок MCP-сервера (контракт §2.1).
 *
 * Сообщения и подсказки — по-русски, коды и ключи details — латиницей.
 * Ошибка — это данные: клиент получает `{ok:false, error:{code,message,hint,retryable,details}}`,
 * а не текст исключения.
 */

export type ErrorCode =
  | 'device_unreachable'
  | 'device_timeout'
  | 'auth_failed'
  | 'auth_required'
  | 'forbidden_admin_required'
  | 'feature_unsupported'
  | 'queue_full'
  | 'guard_blocked'
  | 'validation_failed'
  | 'partial_apply'
  | 'busy'
  | 'not_found'
  | 'confirmation_required'
  | 'disabled_by_policy'
  | 'unsupported_operation'
  | 'device_error'
  | 'internal_error';

export interface ErrorEnvelope {
  ok: false;
  error: {
    code: ErrorCode;
    message: string;
    hint?: string;
    retryable: boolean;
    details?: Record<string, unknown>;
  };
}

export interface OcErrorOptions {
  hint?: string;
  retryable?: boolean;
  details?: Record<string, unknown>;
  httpStatus?: number;
  cause?: unknown;
}

/** Коды, повтор которых бессмысленен без действий человека. */
const NON_RETRYABLE: ReadonlySet<ErrorCode> = new Set<ErrorCode>([
  'auth_failed',
  'auth_required',
  'forbidden_admin_required',
  'feature_unsupported',
  'guard_blocked',
  'validation_failed',
  'partial_apply',
  'not_found',
  'confirmation_required',
  'disabled_by_policy',
  'unsupported_operation',
  'internal_error',
]);

/** Коды, которые по смыслу повторяемы (сеть, занятость, очередь). */
const RETRYABLE: ReadonlySet<ErrorCode> = new Set<ErrorCode>([
  'device_unreachable',
  'device_timeout',
  'queue_full',
  'busy',
]);

export function defaultRetryable(code: ErrorCode): boolean {
  return RETRYABLE.has(code) && !NON_RETRYABLE.has(code);
}

export class OcError extends Error {
  readonly code: ErrorCode;
  readonly hint?: string;
  readonly retryable: boolean;
  readonly details?: Record<string, unknown>;
  readonly httpStatus?: number;

  constructor(code: ErrorCode, message: string, opts: OcErrorOptions = {}) {
    super(message, opts.cause === undefined ? undefined : { cause: opts.cause });
    this.name = 'OcError';
    this.code = code;
    this.hint = opts.hint;
    this.retryable = opts.retryable ?? defaultRetryable(code);
    this.details = opts.details;
    this.httpStatus = opts.httpStatus;
  }

  toEnvelope(): ErrorEnvelope {
    return fail(this.code, this.message, {
      hint: this.hint,
      retryable: this.retryable,
      details: this.details,
    });
  }
}

export function fail(
  code: ErrorCode,
  message: string,
  opts: Omit<OcErrorOptions, 'httpStatus' | 'cause'> = {},
): ErrorEnvelope {
  const error: ErrorEnvelope['error'] = {
    code,
    message,
    retryable: opts.retryable ?? defaultRetryable(code),
  };
  if (opts.hint !== undefined) error.hint = opts.hint;
  if (opts.details !== undefined) error.details = opts.details;
  return { ok: false, error };
}

/** Успешный результат инструмента: `{ok:true, ...поля}`. */
export type OkResult = { ok: true } & Record<string, unknown>;

/** Приводит любое исключение к конверту ошибки, не теряя причину. */
export function errorEnvelope(error: unknown): ErrorEnvelope {
  if (error instanceof OcError) return error.toEnvelope();
  if (error instanceof Error) {
    return fail('internal_error', `Внутренняя ошибка MCP-сервера: ${error.message}`, {
      hint: 'Это дефект сервера, а не отказ контроллера. Сообщите текст ошибки разработчику.',
      details: { name: error.name },
    });
  }
  return fail('internal_error', 'Внутренняя ошибка MCP-сервера: неизвестное исключение', {
    hint: 'Это дефект сервера, а не отказ контроллера. Сообщите текст ошибки разработчику.',
    details: { value: String(error) },
  });
}

/** Тело ответа попадает в details усечённым: целиком оно нужно редко, а контекст забивает. */
export const MAX_DETAILS_BODY = 2_000;

export function truncateBody(body: string, max = MAX_DETAILS_BODY): string {
  if (body.length <= max) return body;
  return `${body.slice(0, max)}… (усечено, всего ${body.length} символов)`;
}

export interface HttpErrorInput {
  status: number;
  body: string;
  method: string;
  path: string;
}

const HTTP_STATUS_CODES: Readonly<Record<number, ErrorCode>> = {
  400: 'validation_failed',
  401: 'auth_failed',
  403: 'forbidden_admin_required',
  404: 'not_found',
  409: 'guard_blocked',
  429: 'busy',
  500: 'device_error',
  501: 'unsupported_operation',
  502: 'device_unreachable',
  503: 'busy',
  504: 'device_unreachable',
};

const HTTP_STATUS_HINTS: Readonly<Record<number, string>> = {
  400: 'Контроллер отклонил параметры запроса. Проверьте значения и повторите.',
  401: 'Проверьте SSVC_USER и SSVC_PASSWORD: контроллер не принял учётные данные. ' +
    'На не-secure сборке прошивки аутентификация выключена — тогда проверьте, что пароль не задан лишним.',
  403: 'Нужна учётная запись администратора: часть эндпоинтов доступна только admin.',
  404: 'Эндпоинт или объект не найден. Проверьте версию прошивки и запрошенный идентификатор.',
  409: 'Контроллер отклонил операцию из-за текущего состояния. Проверьте предохранители и повторите.',
  429: 'Слишком много запросов к контроллеру. Повторите позже.',
  500: 'Внутренняя ошибка прошивки. Повторите запрос; если повторяется — проверьте журнал контроллера.',
  501: 'Операция не реализована в этой версии прошивки контроллера.',
  502: 'Контроллер вернул некорректный ответ. Повторите запрос.',
  503: 'Контроллер занят: очередь команд переполнена или подсистема недоступна. Повторите позже.',
  504: 'Контроллер не ответил вовремя. Проверьте связь и повторите запрос.',
};

/**
 * Маппинг HTTP-ответа устройства в OcError (контракт §2.1).
 * Тело ответа сохраняется в `details.body` в усечённом виде — по нему видно, что именно ответила прошивка.
 */
export function httpError({ status, body, method, path }: HttpErrorInput): OcError {
  let code: ErrorCode = HTTP_STATUS_CODES[status] ?? 'device_error';
  // Отказ очереди команд — отдельный код: он ретраится, но с задержкой.
  if (status === 503 && /queue/i.test(body)) code = 'queue_full';
  if (status === 409 && /blockReasons/i.test(body)) code = 'guard_blocked';

  const hint =
    HTTP_STATUS_HINTS[status] ??
    (status >= 500
      ? 'Контроллер вернул ошибку. Повторите запрос; если повторяется — проверьте журнал устройства.'
      : 'Контроллер отклонил запрос. Проверьте параметры и состояние устройства.');

  return new OcError(code, `Контроллер ответил ${status} на ${method} ${path}`, {
    hint,
    httpStatus: status,
    retryable: status >= 500 || status === 429,
    details: {
      status,
      method,
      path,
      body: truncateBody(body),
    },
  });
}
