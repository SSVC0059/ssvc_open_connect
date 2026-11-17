/**
 * ЕДИНСТВЕННЫЙ владелец разбора JSON-ответов контроллера и распознавания SPA-заглушки.
 *
 * Почему отдельный помощник, а не `DeviceClient.get`: клиент валидирует схему, но
 * SPA-заглушку (`200 text/html` на неизвестный маршрут, mcp-server.md §2.5 п.16)
 * отдаёт как `device_error`, а для домена это «маршрута нет в этой прошивке» —
 * `feature_unsupported`: пустые данные были бы ложью.
 *
 * Инвариант один на все домены (WP1, WP3, WP4, WP5, WP6, WP7, WP8, WP9):
 *  - `404` (и явно перечисленные статусы) — маршрута нет → `feature_unsupported`;
 *  - `200 text/html` / HTML-тело — SPA-заглушка → `feature_unsupported`;
 *  - битый JSON или расхождение схемы → `device_error` с телом в `details`;
 *  - прочие статусы → маппинг транспорта (`httpError` из `errors.ts`).
 *
 * Домены не переписывают этот разбор заново: они передают свои сообщения/подсказки
 * через `JsonReadOptions` (текст остаётся доменным, поведение — общим), а если
 * ответ нельзя разобрать как JSON вообще (CSV журнала, бинарный файл, ответ
 * команды) — пользуются `looksLikeSpa`/`isJsonContentType` из этого же модуля.
 *
 * Записи (`PUT`/`POST`) идут через `parseJsonResponse`: статус уже получен, но
 * проверки те же — успешный ответ обязан быть JSON ожидаемой формы.
 */

import type { z } from 'zod';
import type { DeviceClient, HttpResponse } from '../device/DeviceClient.js';
import {
  bodyForDetails,
  headerValue,
  isJsonContentType,
  parseJsonBody,
} from '../device/schemas/common.js';
import { OcError, httpError, type ErrorCode } from '../errors.js';

/** Переопределение текста отказа: домен знает свою терминологию, разбор — общий. */
export interface JsonReadMessageOverride {
  message?: string;
  hint?: string;
  /** Код ошибки, если домену нужен не общий (например `device_error` вместо `feature_unsupported`). */
  code?: ErrorCode;
}

export interface JsonReadOptions {
  /** Метод для сообщений и `details`; по умолчанию `GET`. */
  method?: string;
  /** Статусы «маршрута нет» помимо 404 (например 501 у `/rest/oc/snapshot`). */
  unsupportedStatuses?: number[];
  /** 404/`unsupportedStatuses`: по умолчанию `feature_unsupported`. */
  notFound?: JsonReadMessageOverride;
  /** Не-JSON ответ (SPA-заглушка): по умолчанию `feature_unsupported`. */
  notJson?: JsonReadMessageOverride;
  /** Тело не разобралось как JSON: по умолчанию `device_error`. */
  invalidJson?: JsonReadMessageOverride;
  /** Тело — JSON, но не той формы: по умолчанию `device_error`. */
  schemaMismatch?: JsonReadMessageOverride;
}

const DEFAULT_NOT_FOUND_HINT =
  'Обновите прошивку OpenConnect: маршрута нет в этой сборке. Повтор запроса не поможет.';

const DEFAULT_NOT_JSON_HINT =
  'Неизвестный маршрут прошивка отдаёт веб-страницей (200 text/html). Проверьте версию ' +
  'прошивки OpenConnect: в ней должен быть запрошенный маршрут.';

const DEFAULT_INVALID_JSON_HINT = 'Проверьте версию прошивки контроллера и повторите запрос.';

const DEFAULT_SCHEMA_HINT =
  'Расхождение контракта с прошивкой: проверьте версию контроллера и поля ответа ' +
  'в details.issues / details.body.';

/**
 * Похож ли ответ на SPA-заглушку прошивки: `text/html` в заголовке либо HTML в теле.
 *
 * Нужен там, где ответ в принципе не JSON (CSV журнала, бинарный файл, текст команды):
 * код 200 сам по себе не отличит страницу от данных.
 */
export function looksLikeSpa(headers: Record<string, string>, body: string): boolean {
  const contentType = (headerValue(headers, 'content-type') ?? '').toLowerCase();
  if (contentType.includes('text/html')) return true;
  const head = body.trimStart().slice(0, 64).toLowerCase();
  return head.startsWith('<!doctype') || head.startsWith('<html');
}

/** `GET` JSON-ответа с проверкой статуса, Content-Type и схемы. */
export async function readJson<T>(
  client: DeviceClient,
  path: string,
  schema: z.ZodType<T>,
  opts: JsonReadOptions = {},
): Promise<T> {
  const response = await client.request('GET', path, undefined, {
    headers: { accept: 'application/json' },
  });
  return parseJsonResponse(response, schema, opts.method ?? 'GET', path, opts);
}

/**
 * Разбор ответа, который уже получен (нужен для `PUT`/`POST`: успех отдаёт ту же
 * структуру, что GET).
 *
 * @throws OcError `feature_unsupported` (404/SPA), `device_error` (битый JSON или схема),
 *         `httpError`-коды для остальных статусов.
 */
export function parseJsonResponse<T>(
  response: HttpResponse,
  schema: z.ZodType<T>,
  method: string,
  path: string,
  opts: JsonReadOptions = {},
): T {
  const unsupportedStatuses = opts.unsupportedStatuses ?? [];
  if (response.status === 404 || unsupportedStatuses.includes(response.status)) {
    throw new OcError(
      opts.notFound?.code ?? 'feature_unsupported',
      opts.notFound?.message ?? `Прошивка не отдаёт ${method} ${path}`,
      {
        hint: opts.notFound?.hint ?? DEFAULT_NOT_FOUND_HINT,
        details: { status: response.status, method, path },
      },
    );
  }
  if (response.status < 200 || response.status >= 300) {
    throw httpError({ status: response.status, body: response.body, method, path });
  }

  const contentType = headerValue(response.headers, 'content-type') ?? null;
  if (!isJsonContentType(response.headers)) {
    throw new OcError(
      opts.notJson?.code ?? 'feature_unsupported',
      opts.notJson?.message ??
        `Ответ ${method} ${path} не является JSON: похоже на SPA-заглушку прошивки`,
      {
        hint: opts.notJson?.hint ?? DEFAULT_NOT_JSON_HINT,
        details: {
          status: response.status,
          method,
          path,
          content_type: contentType,
          body: bodyForDetails(response.body),
        },
      },
    );
  }

  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) {
    throw new OcError(
      opts.invalidJson?.code ?? 'device_error',
      opts.invalidJson?.message ?? `Ответ ${method} ${path} не является корректным JSON`,
      {
        hint: opts.invalidJson?.hint ?? DEFAULT_INVALID_JSON_HINT,
        details: {
          status: response.status,
          method,
          path,
          content_type: contentType,
          parse_error: parsed.error,
          body: bodyForDetails(response.body),
        },
      },
    );
  }

  const result = schema.safeParse(parsed.value);
  if (!result.success) {
    throw new OcError(
      opts.schemaMismatch?.code ?? 'device_error',
      opts.schemaMismatch?.message ?? `Ответ ${method} ${path} не соответствует ожидаемой схеме`,
      {
        hint: opts.schemaMismatch?.hint ?? DEFAULT_SCHEMA_HINT,
        details: {
          status: response.status,
          method,
          path,
          issues: result.error.issues.map((issue) => ({
            path: issue.path.join('.'),
            message: issue.message,
          })),
          body: bodyForDetails(response.body),
        },
      },
    );
  }
  return result.data;
}
