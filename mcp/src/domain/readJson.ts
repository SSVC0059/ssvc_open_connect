/**
 * Чтение JSON-ответа контроллера для доменов WP5 (реле и аппаратная конфигурация).
 *
 * Почему отдельный помощник, а не `DeviceClient.get`: клиент валидирует схему, но
 * SPA-заглушку (`200 text/html` на неизвестный маршрут, mcp-server.md §2.5 п.16)
 * отдаёт как `device_error`, а для домена это `feature_unsupported`: маршрута нет
 * в этой прошивке, и «пустые данные» были бы ложью. Такой же разбор есть в WP4
 * (`src/domain/sensors.ts`), но он приватный; дублировать его третий раз в двух
 * доменах WP5 не нужно, поэтому общий помощник живёт здесь, рядом с доменами.
 *
 * Отдельно различаем:
 *  - `404` — маршрута нет в прошивке → `feature_unsupported`;
 *  - `200 text/html` — SPA-заглушка → `feature_unsupported`;
 *  - битый JSON или расхождение схемы → `device_error` с телом в details;
 *  - прочие статусы → маппинг транспорта (`errors.ts`).
 *
 * Тело ошибки усечено: целиком оно не нужно, а контекст забивает.
 */

import type { z } from 'zod';
import type { DeviceClient, HttpResponse } from '../device/DeviceClient.js';
import {
  bodyForDetails,
  headerValue,
  isJsonContentType,
  parseJsonBody,
} from '../device/schemas/common.js';
import { OcError, httpError } from '../errors.js';

/** GET JSON-ответа с проверкой статуса, Content-Type и схемы. */
export async function readJson<T>(
  client: DeviceClient,
  path: string,
  schema: z.ZodType<T>,
): Promise<T> {
  const response = await client.request('GET', path, undefined, {
    headers: { accept: 'application/json' },
  });
  return parseJsonResponse(response, schema, 'GET', path);
}

/**
 * Разбор ответа, который уже получен (нужен для PUT `/rest/oc/hardware-config`:
 * успех отдаёт ту же структуру, что GET).
 *
 * @throws OcError `feature_unsupported` (404/SPA), `device_error` (битый JSON или схема),
 *         `httpError`-коды для остальных статусов.
 */
export function parseJsonResponse<T>(
  response: HttpResponse,
  schema: z.ZodType<T>,
  method: string,
  path: string,
): T {
  if (response.status === 404) {
    throw new OcError('feature_unsupported', `Прошивка не отдаёт ${method} ${path}`, {
      hint:
        'Обновите прошивку OpenConnect: маршрут должен быть в сборке с реле PCF8574 и ' +
        'аппаратной конфигурацией. Повтор запроса не поможет.',
      details: { status: response.status, method, path },
    });
  }
  if (response.status < 200 || response.status >= 300) {
    throw httpError({ status: response.status, body: response.body, method, path });
  }

  const contentType = headerValue(response.headers, 'content-type') ?? null;
  if (!isJsonContentType(response.headers)) {
    throw new OcError(
      'feature_unsupported',
      `Ответ ${method} ${path} не является JSON: похоже на SPA-заглушку прошивки`,
      {
        hint:
          'Неизвестный маршрут прошивка отдаёт веб-страницей (200 text/html). Проверьте ' +
          `версию прошивки OpenConnect: в ней должен быть ${method} ${path}.`,
        details: {
          status: response.status,
          content_type: contentType,
          body: bodyForDetails(response.body),
        },
      },
    );
  }

  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) {
    throw new OcError('device_error', `Ответ ${method} ${path} не является корректным JSON`, {
      hint: 'Проверьте версию прошивки контроллера и повторите запрос.',
      details: {
        status: response.status,
        content_type: contentType,
        parse_error: parsed.error,
        body: bodyForDetails(response.body),
      },
    });
  }

  const result = schema.safeParse(parsed.value);
  if (!result.success) {
    throw new OcError('device_error', `Ответ ${method} ${path} не соответствует ожидаемой схеме`, {
      hint:
        'Расхождение контракта с прошивкой: проверьте версию контроллера и поля ответа ' +
        'в details.issues / details.body.',
      details: {
        status: response.status,
        issues: result.error.issues.map((issue) => ({
          path: issue.path.join('.'),
          message: issue.message,
        })),
        body: bodyForDetails(response.body),
      },
    });
  }
  return result.data;
}
