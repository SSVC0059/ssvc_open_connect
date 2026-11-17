/**
 * Общие схемы ответов контроллера (контракт §2.8: zod 3, `.passthrough()` там,
 * где прошивка может добавить поля).
 *
 * Принцип: обязательными делаем только те поля, без которых домен не может
 * работать; всё остальное — `.optional()` и `.passthrough()`. Расхождение схемы
 * превращается в `OcError('device_error')` с телом ответа в `details` — так
 * расхождение видно сразу, а не растворяется в «undefined».
 */

import { z } from 'zod';

/** Разбор тела ответа как JSON с русской диагностикой. */
export type JsonParseResult = { ok: true; value: unknown } | { ok: false; error: string };

export function parseJsonBody(text: string): JsonParseResult {
  const trimmed = text.trim();
  if (trimmed === '') return { ok: false, error: 'пустой ответ' };
  try {
    return { ok: true, value: JSON.parse(trimmed) as unknown };
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : String(error) };
  }
}

/** Произвольный JSON-объект: для «сырых» вложенных структур прошивки. */
export const jsonObjectSchema = z.record(z.unknown());

/** Единый формат ошибки устройства: у прошивки встречаются разные поля. */
export const deviceErrorBodySchema = z
  .object({
    error: z.union([z.string(), z.boolean(), jsonObjectSchema]).optional(),
    message: z.string().optional(),
    code: z.string().optional(),
    status: z.string().optional(),
    blockReasons: z.array(z.string()).optional(),
    warnings: z.array(z.string()).optional(),
  })
  .passthrough();

/** Ответ text/plain, нормализованный в объект: `{raw}`. */
export const textResponseSchema = z.object({ raw: z.string() });

/** GET /rest/features → карта булевых флагов. */
export const featureFlagsSchema = z.record(z.boolean());

/** Ответы вида `{"supported": false}` на GPIO-сборке — не ошибка, а «функция недоступна». */
export const supportFlagSchema = z.object({ supported: z.boolean() }).passthrough();

/**
 * Код версии API: `major * 100 + minor` («1.7» → 107, «1.10» → 110).
 * Сравнение строкой здесь ошибочно: «1.10» < «1.7» лексикографически.
 */
export function apiVersionCode(api: string | null | undefined): number {
  if (api === null || api === undefined) return 0;
  const match = /^(\d+)(?:\.(\d+))?/.exec(api.trim());
  if (match === null) return 0;
  const major = Number.parseInt(match[1] ?? '0', 10);
  const minor = Number.parseInt(match[2] ?? '0', 10);
  return major * 100 + minor;
}

/** Сравнение версий API: true, если `api` не ниже `required`. */
export function apiAtLeast(api: string | null | undefined, required: string): boolean {
  return apiVersionCode(api) >= apiVersionCode(required);
}

/** Нормализация заголовков: Node отдаёт их в нижнем регистре, но не гарантирует. */
export function headerValue(
  headers: Record<string, string>,
  name: string,
): string | undefined {
  const target = name.toLowerCase();
  for (const [key, value] of Object.entries(headers)) {
    if (key.toLowerCase() === target) return value;
  }
  return undefined;
}

/** `text/plain` ответ команды: 200 «Command accepted» — это успех. */
export function isTextPlain(headers: Record<string, string>): boolean {
  const contentType = headerValue(headers, 'content-type') ?? '';
  return contentType.toLowerCase().includes('text/plain');
}

export function isJsonContentType(headers: Record<string, string>): boolean {
  const contentType = headerValue(headers, 'content-type') ?? '';
  return contentType.toLowerCase().includes('json');
}

/** Фрагмент тела для details: не тянем в контекст мегабайты. */
export function bodyForDetails(text: string, max = 2_000): string {
  return text.length <= max ? text : `${text.slice(0, max)}… (усечено)`;
}
