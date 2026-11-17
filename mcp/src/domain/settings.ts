/**
 * Доменный слой настроек SSVC (WP3, mcp-server-agents.md §7).
 *
 * Контракт устройства (mcp-contract-notes.md §B.5, §D.2):
 *  - `GET /rest/settings` → 200 `{ "ssvcSettings": { ... } }` — обёртка обязательна;
 *  - `PUT /rest/settings` — тело ПЛОСКИЙ объект без обёртки; ответ 200/400/503.
 *
 * Три правила, которые определяют реализацию (mcp-server.md §2.5 п.4–5):
 *  1. `503 partial:true` — это НЕ ошибка инструмента: часть SET уже ушла в контроллер,
 *     поэтому наружу уходит `ok:true, partial:true` с подсказкой, и PUT не повторяется;
 *  2. `ignored`/`skipped` доносятся до агента явно, а не растворяются в «успехе»;
 *  3. неизвестные ключи не валидируются локально: их отбракует прошивка и вернёт в
 *     `ignored` — так новый параметр новой прошивки не потребует правки MCP-сервера.
 *
 * Транспорт сам ставит `Content-Type: application/json` и `Content-Length`
 * (chunked прошивка не принимает), домен ничего не дублирует.
 */

import type { DeviceClient, HttpResponse } from '../device/DeviceClient.js';
import {
  bodyForDetails,
  headerValue,
  isJsonContentType,
  parseJsonBody,
} from '../device/schemas/common.js';
import {
  settingsEnvelopeSchema,
  settingsWriteResponseSchema,
  type SsvcSettings,
  type SettingsWriteResponseBody,
} from '../device/schemas/settings.js';
import { OcError, httpError } from '../errors.js';

export const SETTINGS_PATH = '/rest/settings';

/** Подсказка при частичном применении: повтор всего тела запрещён. */
export const PARTIAL_HINT =
  'Часть настроек применена, повторять весь запрос нельзя: часть SET-пакетов уже ушла ' +
  'в контроллер. Перечитайте настройки (oc_get_settings) и отправьте только те поля, ' +
  'которые действительно не применились.';

export interface SettingsSnapshot {
  /** Только содержимое `ssvcSettings`: обёртка наружу не выходит. */
  settings: SsvcSettings;
  /** Время получения ответа MCP-сервером (ISO 8601): прошивка своего времени не даёт. */
  observed_at: string;
}

export interface UpdateSettingsOptions {
  /**
   * «Всё или ничего»: непустые `ignored`/`skipped` или `partial:true` превращаются
   * в `validation_failed` с перечнем проблем.
   */
  strict?: boolean;
}

/** Отчёт PUT до перечитывания настроек. */
interface SettingsWriteOutcome {
  accepted: number;
  ignored: string[];
  skipped: string[];
  failed: string[];
  partial: boolean;
  message: string | null;
}

/** Результат `oc_update_settings`: отчёт контроллера + перечитанные настройки. */
export interface SettingsUpdateResult extends SettingsWriteOutcome {
  /** Перечитанные `GET /rest/settings`; `null`, если перечитать не удалось. */
  applied: SsvcSettings | null;
  /** Причина, по которой `applied === null` (иначе `null`). */
  applied_error: string | null;
}

/**
 * Читает `GET /rest/settings` и разворачивает обёртку.
 *
 * SPA-заглушка (`200 text/html`) — не успех: неизвестный маршрут прошивка отдаёт
 * веб-страницей (mcp-server.md §2.5 п.16), поэтому такой ответ становится
 * `feature_unsupported`, а не «пустыми настройками».
 */
export async function getSettings(client: DeviceClient): Promise<SettingsSnapshot> {
  const response = await client.request('GET', SETTINGS_PATH, undefined, {
    headers: { accept: 'application/json' },
  });

  if (response.status === 404) {
    throw new OcError('feature_unsupported', 'Прошивка не отдаёт настройки SSVC (/rest/settings)', {
      hint:
        'Обновите прошивку OpenConnect: маршрут GET/PUT /rest/settings появился в версии с ' +
        'настройками SSVC. Повтор запроса не поможет.',
      details: { status: response.status, path: SETTINGS_PATH },
    });
  }
  if (response.status < 200 || response.status >= 300) {
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'GET',
      path: SETTINGS_PATH,
    });
  }

  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) {
    const contentType = headerValue(response.headers, 'content-type') ?? null;
    if (!isJsonContentType(response.headers)) {
      throw new OcError(
        'feature_unsupported',
        'Ответ /rest/settings не является JSON: похоже на SPA-заглушку прошивки',
        {
          hint:
            'Неизвестный маршрут прошивка отдаёт веб-страницей (200 text/html). Проверьте ' +
            'версию прошивки OpenConnect: в ней должен быть GET /rest/settings.',
          details: {
            status: response.status,
            content_type: contentType,
            parse_error: parsed.error,
            body: bodyForDetails(response.body),
          },
        },
      );
    }
    throw new OcError('device_error', 'Ответ /rest/settings не является корректным JSON', {
      hint: 'Проверьте версию прошивки контроллера и повторите запрос.',
      details: {
        status: response.status,
        content_type: contentType,
        parse_error: parsed.error,
        body: bodyForDetails(response.body),
      },
    });
  }

  const envelope = settingsEnvelopeSchema.safeParse(parsed.value);
  if (!envelope.success) {
    throw new OcError(
      'device_error',
      'Ответ /rest/settings не содержит объект ssvcSettings ожидаемой формы',
      {
        hint:
          'Расхождение контракта с прошивкой: ожидался { "ssvcSettings": { ... } }. ' +
          'Проверьте версию контроллера и тело ответа в details.',
        details: {
          status: response.status,
          issues: envelope.error.issues.map((issue) => ({
            path: issue.path.join('.'),
            message: issue.message,
          })),
          body: bodyForDetails(response.body),
        },
      },
    );
  }

  return {
    settings: envelope.data.ssvcSettings,
    observed_at: new Date().toISOString(),
  };
}

/**
 * Частично обновляет настройки: одно тело — один PUT, без повторов.
 *
 * Запись идёт через `client.withWrite('settings', …)`: один писатель на устройство,
 * чтобы частичный батч не перемешался с другой записью. После успеха (и после
 * частичного применения) настройки перечитываются — `applied` показывает фактическое
 * состояние, а не то, что мы отправили.
 *
 * @throws OcError `validation_failed` (400 прошивки или strict), `busy`/прочие коды
 *         транспорта; при `503 partial:true` исключение НЕ бросается.
 */
export async function updateSettings(
  client: DeviceClient,
  settings: Record<string, unknown>,
  opts: UpdateSettingsOptions = {},
): Promise<SettingsUpdateResult> {
  const outcome = await client.withWrite('settings', () => putSettings(client, settings));

  // strict проверяем до перечитывания: отчёт контроллера уже содержит причину отказа.
  if (opts.strict === true) assertStrict(outcome);

  const applied = await reReadSettings(client);
  return { ...outcome, ...applied };
}

/** Один PUT. Разбор 200/503 partial/400 без ретраев (запись не повторяется никогда). */
async function putSettings(
  client: DeviceClient,
  settings: Record<string, unknown>,
): Promise<SettingsWriteOutcome> {
  const response = await client.request('PUT', SETTINGS_PATH, settings, {});
  const parsed = parseJsonBody(response.body);

  if (response.status === 503 && parsed.ok) {
    const partial = settingsWriteResponseSchema.safeParse(parsed.value);
    if (partial.success && partial.data.partial === true) {
      return toOutcome(partial.data, true);
    }
    // 503 без partial — обычная занятость контроллера: ниже httpError → busy.
  }

  if (response.status < 200 || response.status >= 300) {
    if (response.status === 400) throw invalidSettingsError(response, parsed);
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'PUT',
      path: SETTINGS_PATH,
    });
  }

  if (!parsed.ok) {
    throw new OcError('device_error', 'Ответ на PUT /rest/settings не является JSON', {
      hint:
        'Похоже, прошивка не знает маршрута и вернула веб-страницу. Проверьте версию прошивки ' +
        'контроллера.',
      details: {
        status: response.status,
        content_type: headerValue(response.headers, 'content-type') ?? null,
        parse_error: parsed.error,
        body: bodyForDetails(response.body),
      },
    });
  }

  const parsedBody = settingsWriteResponseSchema.safeParse(parsed.value);
  if (!parsedBody.success) {
    throw new OcError(
      'device_error',
      'Ответ на PUT /rest/settings не соответствует ожидаемой схеме',
      {
        hint:
          'Ожидался объект { success, accepted, ignored, skipped }. Проверьте версию прошивки ' +
          'и тело ответа в details.',
        details: {
          status: response.status,
          issues: parsedBody.error.issues.map((issue) => ({
            path: issue.path.join('.'),
            message: issue.message,
          })),
          body: bodyForDetails(response.body),
        },
      },
    );
  }
  if (parsedBody.data.success === false) {
    throw new OcError('device_error', 'Контроллер сообщил об отказе без partial и без кода 400', {
      hint: 'Проверьте тело ответа в details: прошивка не применила настройки.',
      details: { status: response.status, body: bodyForDetails(response.body) },
    });
  }

  return toOutcome(parsedBody.data, parsedBody.data.partial === true);
}

/**
 * 400 прошивки: `{ "<key>": "Invalid value" }` (или `{"error": "Body must be a JSON object"}`).
 * Ошибка 400 означает, что не применено НИЧЕГО: прошивка отбрасывает батч целиком.
 */
function invalidSettingsError(
  response: HttpResponse,
  parsed: ReturnType<typeof parseJsonBody>,
): OcError {
  const fields: Record<string, string> = {};
  if (parsed.ok && isPlainObject(parsed.value)) {
    for (const [key, value] of Object.entries(parsed.value)) {
      fields[key] = typeof value === 'string' ? value : describeFieldValue(value);
    }
  }
  const invalidKeys = Object.keys(fields).filter((key) => key !== 'error');
  const message =
    invalidKeys.length > 0
      ? `Контроллер отклонил настройки: ${invalidKeys.join(', ')}`
      : 'Контроллер отклонил тело запроса настроек';

  return new OcError('validation_failed', message, {
    hint:
      'Проверьте типы и диапазоны отклонённых полей (details.fields). При 400 прошивка ' +
      'отбрасывает весь батч: ни одна настройка не применена, можно исправить значения и ' +
      'повторить запрос целиком.',
    details: {
      status: response.status,
      fields,
      invalid_keys: invalidKeys,
      body: bodyForDetails(response.body),
    },
  });
}

/** strict:true → любая неполнота становится validation_failed с перечнем проблем. */
function assertStrict(outcome: SettingsWriteOutcome): void {
  if (
    outcome.ignored.length === 0 &&
    outcome.skipped.length === 0 &&
    outcome.partial === false
  ) {
    return;
  }

  const problems: string[] = [];
  if (outcome.ignored.length > 0) problems.push(`ignored: ${outcome.ignored.join(', ')}`);
  if (outcome.skipped.length > 0) problems.push(`skipped: ${outcome.skipped.join(', ')}`);
  if (outcome.failed.length > 0) problems.push(`failed: ${outcome.failed.join(', ')}`);
  if (outcome.partial) problems.push('partial: часть настроек не попала в очередь');

  throw new OcError(
    'validation_failed',
    `Настройки приняты не полностью (strict:true): ${problems.join('; ')}`,
    {
      hint:
        'strict означает «всё или ничего». Уберите неизвестные ключи (ignored) и параметры, ' +
        'не поддерживаемые версией API (skipped), затем повторите. Если partial:true — часть ' +
        'SET уже ушла: весь запрос повторять нельзя, перечитайте oc_get_settings и отправьте ' +
        'только недостающие поля.',
      details: {
        reason: 'strict',
        accepted: outcome.accepted,
        ignored: outcome.ignored,
        skipped: outcome.skipped,
        failed: outcome.failed,
        partial: outcome.partial,
      },
    },
  );
}

/** Перечитывание настроек: ошибка чтения не должна превращать запись в «неудачную». */
async function reReadSettings(
  client: DeviceClient,
): Promise<Pick<SettingsUpdateResult, 'applied' | 'applied_error'>> {
  try {
    const snapshot = await getSettings(client);
    return { applied: snapshot.settings, applied_error: null };
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    return { applied: null, applied_error: message };
  }
}

/** Нормализация отчёта прошивки: массивы приводятся к строкам, отсутствующие — к пустым. */
function toOutcome(body: SettingsWriteResponseBody, partial: boolean): SettingsWriteOutcome {
  return {
    accepted: body.accepted ?? 0,
    ignored: toStringList(body.ignored),
    skipped: toStringList(body.skipped),
    failed: toStringList(body.failed),
    partial,
    message: typeof body.message === 'string' ? body.message : null,
  };
}

function toStringList(value: unknown): string[] {
  if (!Array.isArray(value)) return [];
  return value.map((item) => (typeof item === 'string' ? item : describeFieldValue(item)));
}

function describeFieldValue(value: unknown): string {
  if (value === null) return 'null';
  if (typeof value === 'string') return value;
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  try {
    return JSON.stringify(value);
  } catch {
    return String(value);
  }
}

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}
