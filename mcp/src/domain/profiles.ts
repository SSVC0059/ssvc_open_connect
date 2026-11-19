/**
 * Доменный слой профилей ректификации (WP8, mcp-server-agents.md §12).
 *
 * Контракт устройства (mcp-contract-notes.md §B.14, §D.6):
 *  - `GET    /rest/profiles`              → массив `{id,name,createdAt,isApplied}`;
 *  - `GET    /rest/profiles/active`       → `{id}` | 404;
 *  - `GET    /rest/profiles/content?id=…` → полный профиль;
 *  - `POST   /rest/profiles`              → 201 `{success:true,id}`;
 *  - `POST   /rest/profiles/copy`         → 201 `{success:true,id}` | 409 «source not found»;
 *  - `POST   /rest/profiles/content`      → 200 `{success:"Profile content updated"}`;
 *  - `POST   /rest/profiles/save`         → 200 `{success:true,…}`;
 *  - `POST   /rest/profiles/set-active`   → 200 (делает профиль активным и применяет его);
 *  - `PUT    /rest/profiles/meta`         → 200 `{success:"Profile updated"}`;
 *  - `DELETE /rest/profiles/delete`       → 200 | 404; тело `{id}` обязательно даже в DELETE
 *    (транспорт §2.4 ставит `Content-Length`, поэтому прошивка тело принимает).
 *
 * Почему домен выглядит именно так:
 *  1. `404` у профилей означает две разные вещи, и их нельзя путать:
 *     JSON-тело от `ProfileHandler` — «профиль не найден» (`not_found` с id),
 *     `text/plain "Not Found"` от `WebHandlers` — «маршрута нет в этой прошивке»
 *     (`feature_unsupported`, mcp-contract-notes.md §D.1). Различаем по Content-Type.
 *  2. `GET /rest/profiles/active` с `404` — не ошибка: активного профиля просто нет,
 *     наружу уходит `active_id:null` (§12).
 *  3. После записи, где это осмысленно (создание, копия, переименование, содержимое,
 *     сохранение текущих настроек), список перечитывается: наружу уходит фактическое
 *     состояние профиля, а не эхо аргументов. Ошибка перечитывания не превращает
 *     успешную запись в отказ — она попадает в `warning`.
 *  4. `set-active` — риск 2 (влияет на процесс): после вызова перечитывается
 *     `GET /rest/profiles/active`, и `applied_id` — это ответ устройства, а не наш `id`.
 */

import type { z } from 'zod';
import {
  activeProfileSchema,
  profileContentSchema,
  profileCreateResponseSchema,
  profileListSchema,
  profileWriteResponseSchema,
  PROFILE_NAME_MAX_LENGTH,
  type ProfileContent,
  type ProfileCreateResponseBody,
  type ProfileMeta,
  type ProfileWriteResponseBody,
} from '../device/schemas/profiles.js';
import { bodyForDetails, isJsonContentType } from '../device/schemas/common.js';
import type { DeviceClient, HttpResponse } from '../device/DeviceClient.js';
import { OcError, truncateBody } from '../errors.js';
import { parseJsonResponse, readJson } from './readJson.js';

export const PROFILES_PATH = '/rest/profiles';
export const ACTIVE_PROFILE_PATH = '/rest/profiles/active';
export const PROFILE_CONTENT_PATH = '/rest/profiles/content';
export const PROFILE_COPY_PATH = '/rest/profiles/copy';
export const PROFILE_SAVE_PATH = '/rest/profiles/save';
export const PROFILE_SET_ACTIVE_PATH = '/rest/profiles/set-active';
export const PROFILE_META_PATH = '/rest/profiles/meta';
export const PROFILE_DELETE_PATH = '/rest/profiles/delete';

/** Ключ очереди записей: профили меняются строго последовательно. */
export const PROFILES_WRITE_KEY = 'profiles';

/** Снимок списка профилей и активного профиля. */
export interface ProfilesSnapshot {
  profiles: ProfileMeta[];
  /** `null`, если `GET /rest/profiles/active` ответил 404 — активного профиля нет. */
  active_id: string | null;
  /** Время получения ответа MCP-сервером (ISO 8601). */
  observed_at: string;
}

export interface ProfileContentResult {
  id: string;
  /** Полное содержимое профиля как есть: набор ключей задаёт прошивка. */
  profile: ProfileContent;
  observed_at: string;
}

/** Общее состояние профиля после записи. */
export interface ProfileStateResult {
  profile: ProfileMeta | null;
  /** Причина, по которой состояние перечитать не удалось (иначе `null`). */
  warning: string | null;
  observed_at: string;
}

export interface ProfileCreatedResult extends ProfileStateResult {
  id: string;
}

export interface ProfileAppliedResult {
  /** Фактический активный профиль из `GET /rest/profiles/active`. */
  applied_id: string | null;
  warning: string | null;
  observed_at: string;
}

export interface ProfileDeletedResult {
  deleted_id: string;
  /** true — профиля нет в перечитанном списке (удаление подтверждено устройством). */
  verified_absent: boolean;
  warning: string | null;
  observed_at: string;
}

/**
 * Проверяет идентификатор локально: пустая строка или не строка — `validation_failed`
 * без запроса к контроллеру (прошивка на пустой id отвечает 400, но лишний сетевой
 * вызов ни к чему).
 */
export function requireProfileId(value: unknown, field = 'id'): string {
  if (typeof value !== 'string' || value.trim() === '') {
    throw new OcError('validation_failed', `Поле ${field} должно быть непустой строкой`, {
      hint:
        'Идентификатор профиля — строка из oc_profiles_list, например p1. ' +
        'Пустой идентификатор отклоняется локально, запрос к контроллеру не отправляется.',
      details: { reason: 'invalid_argument', field },
    });
  }
  return value.trim();
}

/**
 * Проверяет название профиля. Прошивка отвергает только пустое имя; предел
 * `PROFILE_NAME_MAX_LENGTH` — серверный (объяснение — в schemas/profiles.ts).
 */
export function requireProfileName(value: unknown, field = 'name'): string {
  if (typeof value !== 'string') {
    throw new OcError('validation_failed', `Поле ${field} должно быть строкой`, {
      hint: 'Передайте отображаемое название профиля строкой.',
      details: { reason: 'invalid_argument', field },
    });
  }
  const name = value.trim();
  if (name === '') {
    throw new OcError('validation_failed', 'Название профиля не может быть пустым', {
      hint: 'Передайте название, по которому профиль будет узнаваем: например «Спирт-сырец».',
      details: { reason: 'invalid_argument', field },
    });
  }
  if (name.length > PROFILE_NAME_MAX_LENGTH) {
    throw new OcError(
      'validation_failed',
      `Название профиля длиннее ${PROFILE_NAME_MAX_LENGTH} символов`,
      {
        hint:
          `Сократите название до ${PROFILE_NAME_MAX_LENGTH} символов: прошивка жёсткого предела ` +
          'не задаёт, но длинное имя нечитаемо в интерфейсе контроллера. Запрос не отправлялся.',
        details: {
          reason: 'invalid_argument',
          field,
          max_length: PROFILE_NAME_MAX_LENGTH,
          length: name.length,
        },
      },
    );
  }
  return name;
}

/** Содержимое профиля — только JSON-объект (прошивка отвергает массивы и скаляры). */
export function requireProfileContent(value: unknown, field = 'content'): ProfileContent {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) {
    throw new OcError('validation_failed', 'Поле content должно быть JSON-объектом', {
      hint:
        'Передайте содержимое профиля объектом: { "ssvcSettings": { … }, "alarmThresholds": { … } }. ' +
        'Массив или скаляр прошивка отвергнет с 400.',
      details: { reason: 'invalid_argument', field },
    });
  }
  return value as ProfileContent;
}

/**
 * `GET /rest/profiles` + `GET /rest/profiles/active`.
 *
 * `404` активного профиля — штатное состояние (`active_id:null`), а не отказ:
 * на устройстве может не быть применённого профиля.
 */
export async function listProfiles(client: DeviceClient): Promise<ProfilesSnapshot> {
  const profiles = await readProfileList(client);
  const activeId = await readActiveProfileId(client);
  return { profiles, active_id: activeId, observed_at: new Date().toISOString() };
}

/** Только список профилей: используется и для чтения, и для перечитывания после записи. */
export async function readProfileList(client: DeviceClient): Promise<ProfileMeta[]> {
  return readJson(client, PROFILES_PATH, profileListSchema);
}

/** `GET /rest/profiles/active`: 404 → `null`, остальные отказы — ошибками транспорта. */
export async function readActiveProfileId(client: DeviceClient): Promise<string | null> {
  const response = await client.request('GET', ACTIVE_PROFILE_PATH, undefined, {
    headers: { accept: 'application/json' },
  });
  if (response.status === 404) return null;
  return parseJsonResponse(response, activeProfileSchema, 'GET', ACTIVE_PROFILE_PATH).id;
}

/** Полное содержимое профиля; 404 — «профиль не найден», а не «маршрута нет». */
export async function getProfileContent(
  client: DeviceClient,
  id: unknown,
): Promise<ProfileContentResult> {
  const profileId = requireProfileId(id);
  const response = await client.request('GET', PROFILE_CONTENT_PATH, undefined, {
    query: { id: profileId },
    headers: { accept: 'application/json' },
  });
  if (response.status === 404) {
    throw profileNotFound(profileId, 'GET', PROFILE_CONTENT_PATH, response.body);
  }
  const profile = parseJsonResponse(
    response,
    profileContentSchema,
    'GET',
    PROFILE_CONTENT_PATH,
  );
  return { id: profileId, profile, observed_at: new Date().toISOString() };
}

/**
 * Создаёт профиль. `content` необязателен: без него прошивка подставит текущие
 * настройки наблюдателей (см. `_createOrUpdateProfile`).
 */
export async function createProfile(
  client: DeviceClient,
  opts: { name: unknown; content?: unknown },
): Promise<ProfileCreatedResult> {
  const name = requireProfileName(opts.name);
  const payload: Record<string, unknown> = { name };
  if (opts.content !== undefined && opts.content !== null) {
    payload.content = requireProfileContent(opts.content);
  }

  const createdId = await client.withWrite(PROFILES_WRITE_KEY, async () => {
    const response = await client.request('POST', PROFILES_PATH, payload, {});
    const body = parseWriteResponse(response, profileCreateResponseSchema, {
      method: 'POST',
      path: PROFILES_PATH,
    });
    const id = body.id;
    if (typeof id !== 'string' || id.trim() === '') {
      throw new OcError('device_error', 'Контроллер не вернул идентификатор созданного профиля', {
        hint:
          'Проверьте oc_profiles_list: возможно, профиль создан, но ответ прошивки пришёл без id. ' +
          'Повторное создание может сделать дубликат.',
        details: {
          method: 'POST',
          path: PROFILES_PATH,
          body: bodyForDetails(JSON.stringify(body)),
        },
      });
    }
    return id.trim();
  });

  const state = await describeProfile(client, createdId);
  return { id: createdId, ...state };
}

/** Копия профиля: 409 у прошивки означает «источник не найден» (не guard_blocked). */
export async function copyProfile(
  client: DeviceClient,
  opts: { sourceId: unknown; newName: unknown },
): Promise<ProfileCreatedResult> {
  const sourceId = requireProfileId(opts.sourceId, 'source_id');
  const newName = requireProfileName(opts.newName, 'new_name');

  const createdId = await client.withWrite(PROFILES_WRITE_KEY, async () => {
    const response = await client.request(
      'POST',
      PROFILE_COPY_PATH,
      { sourceId, newName },
      {},
    );
    const body = parseWriteResponse(response, profileCreateResponseSchema, {
      method: 'POST',
      path: PROFILE_COPY_PATH,
      conflictSourceId: sourceId,
    });
    const id = body.id;
    if (typeof id !== 'string' || id.trim() === '') {
      throw new OcError('device_error', 'Контроллер не вернул идентификатор копии профиля', {
        hint:
          'Проверьте oc_profiles_list: возможно, копия создана, но ответ прошивки пришёл без id.',
        details: {
          source_id: sourceId,
          method: 'POST',
          path: PROFILE_COPY_PATH,
          body: bodyForDetails(JSON.stringify(body)),
        },
      });
    }
    return id.trim();
  });

  const state = await describeProfile(client, createdId);
  return { id: createdId, ...state };
}

/** Переименование: `PUT /rest/profiles/meta` с `{id,name}`. */
export async function renameProfile(
  client: DeviceClient,
  opts: { id: unknown; name: unknown },
): Promise<{ id: string; state: ProfileStateResult }> {
  const id = requireProfileId(opts.id);
  const name = requireProfileName(opts.name);

  await client.withWrite(PROFILES_WRITE_KEY, async () => {
    const response = await client.request('PUT', PROFILE_META_PATH, { id, name }, {});
    const body = parseWriteResponse(response, profileWriteResponseSchema, {
      method: 'PUT',
      path: PROFILE_META_PATH,
      notFoundId: id,
    });
    assertWriteSuccess(body, 'PUT', PROFILE_META_PATH);
    return true;
  });

  return { id, state: await describeProfile(client, id) };
}

/**
 * Замена содержимого: `POST /rest/profiles/content` с `{id,content}`.
 *
 * Важная деталь прошивки (`ProfileService::updateProfileContent`): переданный объект
 * **сливается** с файлом профиля по верхним ключам, а не заменяет файл целиком;
 * ключ со значением `null` удаляет соответствующую секцию. Инструмент отдаёт
 * описание §12 дословно («заменяет содержимое»), но агент должен знать, что
 * непереданные секции остаются прежними.
 */
export async function updateProfileContent(
  client: DeviceClient,
  opts: { id: unknown; content: unknown },
): Promise<{ id: string; state: ProfileStateResult }> {
  const id = requireProfileId(opts.id);
  const content = requireProfileContent(opts.content);

  await client.withWrite(PROFILES_WRITE_KEY, async () => {
    const response = await client.request('POST', PROFILE_CONTENT_PATH, { id, content }, {});
    const body = parseWriteResponse(response, profileWriteResponseSchema, {
      method: 'POST',
      path: PROFILE_CONTENT_PATH,
      notFoundId: id,
    });
    assertWriteSuccess(body, 'POST', PROFILE_CONTENT_PATH);
    return true;
  });

  return { id, state: await describeProfile(client, id) };
}

/**
 * Сохраняет действующие настройки контроллера в профиль (риск 1).
 * Это перезапись содержимого профиля — на живом стенде выполнять осознанно.
 */
export async function saveCurrentSettings(
  client: DeviceClient,
  opts: { id: unknown },
): Promise<{ id: string; state: ProfileStateResult }> {
  const id = requireProfileId(opts.id);

  await client.withWrite(PROFILES_WRITE_KEY, async () => {
    const response = await client.request('POST', PROFILE_SAVE_PATH, { id }, {});
    const body = parseWriteResponse(response, profileWriteResponseSchema, {
      method: 'POST',
      path: PROFILE_SAVE_PATH,
      notFoundId: id,
    });
    assertWriteSuccess(body, 'POST', PROFILE_SAVE_PATH);
    return true;
  });

  return { id, state: await describeProfile(client, id) };
}

/**
 * Делает профиль активным и применяет его настройки к контроллеру (риск 2).
 *
 * Наружу уходит `applied_id` из `GET /rest/profiles/active` — фактическое состояние
 * устройства, а не наш аргумент. Если прошивка сообщила успех, но активным оказался
 * другой профиль (или активного нет), это попадает в `warning`, а не в отказ:
 * запись уже произошла, и повторять её нельзя.
 */
export async function applyProfile(
  client: DeviceClient,
  opts: { id: unknown },
): Promise<ProfileAppliedResult> {
  const id = requireProfileId(opts.id);

  await client.withWrite(PROFILES_WRITE_KEY, async () => {
    const response = await client.request(
      'POST',
      PROFILE_SET_ACTIVE_PATH,
      { id },
      {},
    );
    const body = parseWriteResponse(response, profileWriteResponseSchema, {
      method: 'POST',
      path: PROFILE_SET_ACTIVE_PATH,
      notFoundId: id,
    });
    assertWriteSuccess(body, 'POST', PROFILE_SET_ACTIVE_PATH);
    return true;
  });

  try {
    const appliedId = await readActiveProfileId(client);
    let warning: string | null = null;
    if (appliedId === null) {
      warning =
        'Контроллер сообщил об успехе, но активный профиль не найден: проверьте oc_profiles_list.';
    } else if (appliedId !== id) {
      warning =
        `Активным оказался профиль «${appliedId}», а не «${id}»: ` +
        'проверьте oc_profiles_list и настройки контроллера.';
    }
    return { applied_id: appliedId, warning, observed_at: new Date().toISOString() };
  } catch (error) {
    return {
      applied_id: null,
      warning: `Профиль применён, но перечитать активный профиль не удалось: ${messageOf(error)}`,
      observed_at: new Date().toISOString(),
    };
  }
}

/**
 * Удаляет профиль (риск 3): тело `{id}` в DELETE, после чего список перечитывается
 * и отсутствие профиля подтверждается фактически.
 */
export async function deleteProfile(
  client: DeviceClient,
  opts: { id: unknown },
): Promise<ProfileDeletedResult> {
  const id = requireProfileId(opts.id);

  await client.withWrite(PROFILES_WRITE_KEY, async () => {
    const response = await client.request('DELETE', PROFILE_DELETE_PATH, { id }, {});
    const body = parseWriteResponse(response, profileWriteResponseSchema, {
      method: 'DELETE',
      path: PROFILE_DELETE_PATH,
      notFoundId: id,
    });
    assertWriteSuccess(body, 'DELETE', PROFILE_DELETE_PATH);
    return true;
  });

  try {
    const profiles = await readProfileList(client);
    const stillPresent = profiles.some((profile) => profile.id === id);
    return {
      deleted_id: id,
      verified_absent: !stillPresent,
      warning: stillPresent
        ? `Профиль «${id}» всё ещё есть в списке после удаления: проверьте oc_profiles_list.`
        : null,
      observed_at: new Date().toISOString(),
    };
  } catch (error) {
    return {
      deleted_id: id,
      verified_absent: false,
      warning: `Профиль удалён, но перечитать список не удалось: ${messageOf(error)}`,
      observed_at: new Date().toISOString(),
    };
  }
}

/** Перечитывает список и достаёт метаданные профиля: это фактическое состояние. */
async function describeProfile(
  client: DeviceClient,
  id: string,
): Promise<ProfileStateResult> {
  try {
    const profiles = await readProfileList(client);
    const profile = profiles.find((item) => item.id === id) ?? null;
    return {
      profile,
      warning:
        profile === null
          ? `Профиль «${id}» не найден в перечитанном списке: проверьте oc_profiles_list.`
          : null,
      observed_at: new Date().toISOString(),
    };
  } catch (error) {
    return {
      profile: null,
      warning: `Не удалось перечитать список профилей: ${messageOf(error)}`,
      observed_at: new Date().toISOString(),
    };
  }
}

interface WriteParseOptions {
  method: string;
  path: string;
  /** 404 с JSON-телом = «профиль не найден»; id попадает в details. */
  notFoundId?: string;
  /** 409 у `/rest/profiles/copy` = «исходный профиль не найден». */
  conflictSourceId?: string;
}

/**
 * Разбор ответа записи.
 *
 * 404 двоякий (mcp-contract-notes.md §D.1): JSON от обработчика профилей — «объект
 * не найден», `text/plain "Not Found"` — маршрута нет в прошивке. Второй случай
 * отдаём как `feature_unsupported`, чтобы агент не искал несуществующий профиль.
 */
function parseWriteResponse<T>(
  response: HttpResponse,
  schema: z.ZodType<T>,
  opts: WriteParseOptions,
): T {
  if (response.status === 404) {
    if (opts.notFoundId !== undefined && isJsonContentType(response.headers)) {
      throw profileNotFound(opts.notFoundId, opts.method, opts.path, response.body);
    }
    throw routeNotSupported(response, opts.method, opts.path);
  }
  if (response.status === 409 && opts.conflictSourceId !== undefined) {
    throw new OcError(
      'not_found',
      `Исходный профиль «${opts.conflictSourceId}» не найден на контроллере`,
      {
        hint:
          'Копировать можно только существующий профиль: возьмите source_id из oc_profiles_list. ' +
          'Прошивка отвечает 409, когда источника нет.',
        details: {
          source_id: opts.conflictSourceId,
          status: response.status,
          method: opts.method,
          path: opts.path,
          body: truncateBody(response.body),
        },
      },
    );
  }
  return parseJsonResponse(response, schema, opts.method, opts.path);
}

/** `not_found` профиля: id обязательно в details, чтобы агент не гадал, что удалили. */
function profileNotFound(
  id: string,
  method: string,
  path: string,
  body: string,
): OcError {
  return new OcError('not_found', `Профиль «${id}» не найден на контроллере`, {
    hint:
      'Проверьте идентификатор в oc_profiles_list: профиль мог быть удалён. ' +
      'Пустой id отклоняется локально и до устройства не доходит.',
    details: { id, status: 404, method, path, body: truncateBody(body) },
  });
}

/** 404 без JSON-тела: маршрута нет в прошивке, а не «профиль не найден». */
function routeNotSupported(
  response: HttpResponse,
  method: string,
  path: string,
): OcError {
  return new OcError('feature_unsupported', `Прошивка не отдаёт ${method} ${path}`, {
    hint:
      'Обновите прошивку OpenConnect: маршруты профилей появляются в сборке с ProfileService. ' +
      'Повтор запроса не поможет.',
    details: {
      status: response.status,
      method,
      path,
      content_type: response.headers['content-type'] ?? null,
      body: bodyForDetails(response.body),
    },
  });
}

/** `success:false` при 2xx — отказ прошивки без кода ошибки; не выдаём за успех. */
function assertWriteSuccess(
  body: ProfileWriteResponseBody | ProfileCreateResponseBody,
  method: string,
  path: string,
): void {
  if (body.success === false) {
    throw new OcError('device_error', `Контроллер не выполнил ${method} ${path}`, {
      hint:
        'Прошивка вернула успешный HTTP-код, но success:false. Проверьте тело ответа в details ' +
        'и состояние профилей (oc_profiles_list).',
      details: {
        method,
        path,
        message: body.message ?? null,
        error: body.error ?? null,
      },
    });
  }
}

function messageOf(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
