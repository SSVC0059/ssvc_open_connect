/**
 * Схемы `/rest/profiles*` (WP8, mcp-server-agents.md §12; контракт прошивки —
 * mcp-contract-notes.md §B.14, §D.6).
 *
 * Формы, которые нельзя путать:
 *  - `GET /rest/profiles` отдаёт **массив** метаданных, а не объект;
 *  - `GET /rest/profiles/active` — `{ "id": "p1" }` либо `404` (активного профиля нет);
 *  - `GET /rest/profiles/content?id=…` — полный файл профиля: `id`, `name`, `createdAt`
 *    и ключи наблюдателей (`ssvcSettings`, `alarmThresholds`, `telegram`, `vk`).
 *    Набор ключей зависит от прошивки и включённых подсистем, поэтому содержимое —
 *    `z.record(z.unknown())`, а не закрытый объект.
 *
 * Ответы записи неоднородны по контракту: `success` бывает и `true`, и строкой
 * (`"Profile content updated"`, `"Profile deleted"`), а у ошибок — `false` + `message`.
 * Поэтому поле описано объединением, а смысл проверяется в домене.
 */

import { z } from 'zod';

/**
 * Предел длины названия профиля.
 *
 * Прошивка (`ProfileHandler.cpp`) проверяет только непустоту: жёсткого предела в
 * контракте устройства нет. Сервер ограничивает 120 символами, чтобы название
 * оставалось читаемым в веб-интерфейсе контроллера и в ответах инструментов,
 * и чтобы в JSON-файл профиля не уезжала случайная простыня текста.
 */
export const PROFILE_NAME_MAX_LENGTH = 120;

/** Элемент списка `GET /rest/profiles`. Лишние поля прошивки не ломают разбор. */
export const profileMetaSchema = z
  .object({
    id: z.string().min(1),
    name: z.string(),
    createdAt: z.string().optional(),
    isApplied: z.boolean().optional(),
  })
  .passthrough();

export type ProfileMeta = z.infer<typeof profileMetaSchema>;

/** `GET /rest/profiles` — именно массив (`ProfileService::getProfileListAsJson`). */
export const profileListSchema = z.array(profileMetaSchema);

/** `GET /rest/profiles/active` — только идентификатор; 404 обрабатывается в домене. */
export const activeProfileSchema = z.object({ id: z.string().min(1) }).passthrough();

/** Полное содержимое профиля: произвольные ключи, заданные наблюдателями прошивки. */
export const profileContentSchema = z.record(z.unknown());

export type ProfileContent = z.infer<typeof profileContentSchema>;

/**
 * Ответ `POST /rest/profiles` и `POST /rest/profiles/copy`:
 * 201 `{success:true, id:"p2"}`. `id` необязателен в схеме, потому что при отказе
 * прошивка может прислать `{success:false}` без него; отсутствие id проверяет домен.
 */
export const profileCreateResponseSchema = z
  .object({
    success: z.union([z.boolean(), z.string()]).optional(),
    id: z.string().optional(),
    message: z.string().optional(),
    error: z.union([z.string(), z.boolean()]).optional(),
  })
  .passthrough();

export type ProfileCreateResponseBody = z.infer<typeof profileCreateResponseSchema>;

/**
 * Ответы остальных записей: переименование, содержимое, сохранение текущих
 * настроек, активация, удаление. Успех — `success` (true или строка), отказ —
 * `success:false` + `message`/`error` и код 4xx/5xx.
 */
export const profileWriteResponseSchema = z
  .object({
    success: z.union([z.boolean(), z.string()]).optional(),
    message: z.string().optional(),
    error: z.union([z.string(), z.boolean()]).optional(),
  })
  .passthrough();

export type ProfileWriteResponseBody = z.infer<typeof profileWriteResponseSchema>;
