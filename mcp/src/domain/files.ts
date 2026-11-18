/**
 * Доменный слой файлов контроллера (WP9, mcp-server-agents.md §13; контракт
 * прошивки — mcp-contract-notes.md §B.15, §D.7).
 *
 * Особенности контракта, которые определяют реализацию:
 *
 *  1. `GET /rest/files` отдаёт дерево **массивами**, а имя узла — только базовое.
 *     Поэтому `oc_files_list` с `path` спускается по дереву на сервере, а не
 *     запрашивает у устройства «каталог»: отдельного эндпоинта для этого нет.
 *  2. `GET /rest/files{path}` отдаёт **октеты**, а не JSON. Строковое тело ответа
 *     (`HttpResponse.body`) — это UTF-8-декодирование; для бинарного файла оно
 *     необратимо, поэтому скачивание запрашивает `captureBytes:true` и пишет
 *     на диск именно байты.
 *  3. В прошивках до исправления matcher-а `GET /rest/files/*` мог быть перекрыт
 *     обработчиком дерева (маршруты регистрируются по порядку, mcp-contract-notes.md §H.1.2).
 *     Тогда вместо файла приходил JSON-массив дерева: это `feature_unsupported`
 *     с подсказкой обновить прошивку, а не «сохранённый файл», в котором лежит дерево.
 *     Проверка оставлена как защита от старых прошивок.
 *  4. DELETE и copy — записи, поэтому идут через `client.withWrite('files', …)`:
 *     один писатель на устройство, как требует контракт §2.4.
 *  5. Пути — только абсолютные внутри LittleFS (`/config/app.json`). `..`,
 *     управляющие символы, обратные слэши и пустые сегменты отклоняются
 *     **до** сети (`validation_failed`): обработчик файлов прошивки таких
 *     проверок не делает.
 *  6. Скачивание сохраняется только внутри рабочего каталога MCP-сервера:
 *     `save_dir` относительный, имя файла — последний сегмент пути, санитизация
 *     не даёт выйти из каталога даже при экзотическом имени.
 */

import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, isAbsolute, relative, resolve, sep } from 'node:path';
import type { DeviceClient, HttpResponse } from '../device/DeviceClient.js';
import {
  bodyForDetails,
  headerValue,
  isJsonContentType,
  parseJsonBody,
} from '../device/schemas/common.js';
import { fileTreeSchema, type FileTree, type FileTreeEntry } from '../device/schemas/files.js';
import { OcError, httpError, truncateBody } from '../errors.js';
import { looksLikeSpa, readJson } from './readJson.js';

export const FILES_PATH = '/rest/files';
export const FILES_COPY_PATH = '/rest/files/copy';

/** Каталог сохранения по умолчанию: относительный, внутри рабочего каталога сервера. */
export const DEFAULT_DOWNLOAD_DIR = 'logs/downloads';

/**
 * Бюджет одного скачивания. Файлы LittleFS небольшие (раздел на 16 МБ флеша),
 * но 60 с — верхняя граница, после которой лучше вернуть `device_timeout`,
 * чем держать вызов MCP-клиента (у SDK по умолчанию 60 с).
 */
export const DOWNLOAD_TIMEOUT_MS = 60_000;

/** Предел длины пути: имена LittleFS короткие, длиннее — почти наверняка ошибка. */
const MAX_PATH_LENGTH = 512;

/** Управляющие символы недопустимы в имени файла и ломают заголовки/логи. */
// eslint-disable-next-line no-control-regex
const CONTROL_CHARS = /[\u0000-\u001f\u007f]/;

export interface FileTreeSnapshot {
  /** Нормализованный путь, который запросили; `/` — корень. */
  path: string;
  /** Содержимое каталога: для корня — все узлы верхнего уровня. */
  tree: FileTree;
  observed_at: string;
}

export interface DownloadFileOptions {
  /** Каталог сохранения относительно `baseDir`; по умолчанию `logs/downloads`. */
  saveDir?: string;
  /** База для относительного `saveDir`; по умолчанию рабочий каталог процесса. */
  baseDir?: string;
  /** Таймаут HTTP-запроса, мс (в тестах уменьшается). */
  timeoutMs?: number;
}

/** Результат скачивания: содержимое файла наружу не отдаётся. */
export interface DownloadFileResult {
  /** Путь на контроллере. */
  path: string;
  /** Абсолютный путь сохранённого файла на сервере. */
  saved_path: string;
  /** Размер ровно в байтах (а не в символах строки). */
  bytes: number;
  /** Content-Type ответа устройства (для диагностики). */
  content_type: string | null;
  observed_at: string;
}

export interface CopyFileResult {
  source: string;
  destination: string;
  observed_at: string;
}

export interface DeleteFileResult {
  path: string;
  /** Подтверждено ли отсутствие файла перечитанным деревом. */
  verified_absent: boolean;
  /** Заполняется, если проверить дерево после удаления не удалось. */
  warning: string | null;
  observed_at: string;
}

export interface DeleteFileInput {
  path: unknown;
}

export interface CopyFileInput {
  source: unknown;
  destination: unknown;
}

/**
 * Проверяет путь к файлу на контроллере.
 *
 * @throws OcError `validation_failed` — путь не абсолютный, содержит `..`,
 *         управляющие символы, обратный слэш, пустой сегмент, заканчивается
 *         слэшем (это каталог) или совпадает с корнем.
 */
export function validateFilePath(raw: unknown, field = 'path'): string {
  return normalizePath(raw, field, { directory: false });
}

/**
 * Проверяет путь к каталогу. Корень `/` допустим, завершающий слэш снимается:
 * список каталога — операция сервера над деревом, а не запрос к прошивке.
 */
export function validateDirectoryPath(raw: unknown, field = 'path'): string {
  return normalizePath(raw, field, { directory: true });
}

function normalizePath(
  raw: unknown,
  field: string,
  opts: { directory: boolean },
): string {
  if (typeof raw !== 'string') {
    throw new OcError('validation_failed', `Поле ${field} должно быть строкой с путём`, {
      hint: 'Передайте путь на контроллере, например "/config/app.json".',
      details: { field, type: typeof raw },
    });
  }

  let value = raw.trim();
  if (value === '') {
    throw new OcError('validation_failed', 'Путь не может быть пустым', {
      hint: 'Укажите абсолютный путь на контроллере, например "/config/app.json".',
      details: { field },
    });
  }
  if (value.length > MAX_PATH_LENGTH) {
    throw new OcError('validation_failed', `Путь длиннее ${MAX_PATH_LENGTH} символов`, {
      hint: 'Проверьте путь: имена файлов LittleFS короткие.',
      details: { field, length: value.length, max_length: MAX_PATH_LENGTH },
    });
  }
  if (!value.startsWith('/')) {
    throw new OcError('validation_failed', 'Путь на контроллере должен начинаться с «/»', {
      hint: 'Прошивка ждёт абсолютный путь внутри LittleFS, например "/config/app.json".',
      details: { field, path: value },
    });
  }
  if (CONTROL_CHARS.test(value)) {
    throw new OcError('validation_failed', 'Путь содержит управляющие символы', {
      hint: 'Уберите из пути символы перевода строки, табуляции и другие управляющие коды.',
      details: { field },
    });
  }
  if (value.includes('\\')) {
    throw new OcError('validation_failed', 'Путь не должен содержать обратный слэш', {
      hint: 'Разделитель пути на контроллере — только «/».',
      details: { field, path: value },
    });
  }

  if (opts.directory) {
    if (value === '/') return '/';
    if (value.endsWith('/')) value = value.slice(0, -1);
  } else {
    if (value === '/') {
      throw new OcError('validation_failed', 'Корень файловой системы — не файл', {
        hint: 'Укажите путь к файлу, например "/config/app.json".',
        details: { field, path: value },
      });
    }
    if (value.endsWith('/')) {
      throw new OcError('validation_failed', 'Путь заканчивается слэшем: это каталог, а не файл', {
        hint: 'Уберите завершающий «/» или укажите файл внутри каталога.',
        details: { field, path: value },
      });
    }
  }

  const segments = value.slice(1).split('/');
  for (const segment of segments) {
    if (segment === '') {
      throw new OcError('validation_failed', 'Путь содержит пустой сегмент', {
        hint: 'Уберите двойные слэши из пути.',
        details: { field, path: value },
      });
    }
    if (segment === '.' || segment === '..') {
      throw new OcError('validation_failed', 'Путь не должен содержать «.» или «..»', {
        hint:
          'Выход за пределы каталога запрещён: прошивка не проверяет такие пути, ' +
          'поэтому сервер отклоняет их до запроса.',
        details: { field, path: value },
      });
    }
  }

  return value;
}

/** Кодирует путь для URL, не трогая разделители: `/a b/c#d` → `/a%20b/c%23d`. */
function encodePathForUrl(path: string): string {
  return path
    .split('/')
    .map((segment, index) => (index === 0 ? '' : encodeURIComponent(segment)))
    .join('/');
}

/** Ищет узел по полному пути в дереве `GET /rest/files`; `null` — нет такого. */
export function findEntry(tree: FileTree, path: string): FileTreeEntry | null {
  const segments = path.split('/').filter((segment) => segment !== '');
  if (segments.length === 0) return null;

  let entries: FileTree | undefined = tree;
  let found: FileTreeEntry | null = null;
  for (const segment of segments) {
    if (entries === undefined) return null;
    const next: FileTreeEntry | undefined = entries.find(
      (entry: FileTreeEntry) => entry.name === segment,
    );
    if (next === undefined) return null;
    found = next;
    entries = next.children;
  }
  return found;
}

/**
 * Читает дерево файлов; при `path` — содержимое указанного каталога.
 *
 * @throws OcError `validation_failed` (путь), `not_found` (каталога нет или это файл),
 *         `feature_unsupported` (маршрута/SPA), `device_error` (схема).
 */
export async function listFiles(
  client: DeviceClient,
  opts: { path?: unknown } = {},
): Promise<FileTreeSnapshot> {
  const requested =
    opts.path === undefined || opts.path === null
      ? '/'
      : validateDirectoryPath(opts.path);

  const tree = await readJson(client, FILES_PATH, fileTreeSchema);
  const observedAt = new Date().toISOString();

  if (requested === '/') return { path: '/', tree, observed_at: observedAt };

  const node = findEntry(tree, requested);
  if (node === null) {
    throw new OcError('not_found', `Каталог «${requested}» не найден на контроллере`, {
      hint:
        'Путь проверяется по фактическому дереву LittleFS. Вызовите oc_files_list без ' +
        'path и найдите существующий каталог.',
      details: { path: requested },
    });
  }
  if (node.type !== 'directory') {
    throw new OcError('validation_failed', `«${requested}» — файл, а не каталог`, {
      hint:
        'oc_files_list показывает содержимое каталогов. Чтобы получить файл, используйте ' +
        'oc_file_download.',
      details: { path: requested, type: node.type },
    });
  }

  return { path: requested, tree: node.children ?? [], observed_at: observedAt };
}

/**
 * Скачивает файл с контроллера и сохраняет байты на диск.
 *
 * Содержимое файла в ответ инструмента не попадает: только путь и размер —
 * файлы бывают бинарными и крупными, контекст модели они не нужны.
 *
 * @throws OcError `validation_failed` (путь/каталог сохранения), `not_found`,
 *         `feature_unsupported` (SPA или перекрытый маршрут скачивания),
 *         `device_timeout`, `device_error`.
 */
export async function downloadFile(
  client: DeviceClient,
  rawPath: unknown,
  opts: DownloadFileOptions = {},
): Promise<DownloadFileResult> {
  const path = validateFilePath(rawPath);
  const saveDir = resolveSaveDir(opts.saveDir, opts.baseDir ?? process.cwd());
  const savedPath = ensureInsideDir(saveDir, sanitizeFileName(path));

  const response = await client.request('GET', `${FILES_PATH}${encodePathForUrl(path)}`, undefined, {
    headers: { accept: 'application/octet-stream, application/json, text/plain, */*' },
    timeoutMs: opts.timeoutMs ?? DOWNLOAD_TIMEOUT_MS,
    // Единственный вызов, которому нужны сырые байты: строка ответа для бинарного
    // файла уже испорчена UTF-8-декодированием.
    captureBytes: true,
  });

  const bytes = downloadBytes(path, response);
  const contentType = headerValue(response.headers, 'content-type') ?? null;

  await mkdir(saveDir, { recursive: true });
  await writeFile(savedPath, bytes);

  return {
    path,
    saved_path: savedPath,
    bytes: bytes.length,
    content_type: contentType,
    observed_at: new Date().toISOString(),
  };
}

/** Проверяет ответ скачивания и возвращает ровно те байты, что пришли. */
function downloadBytes(path: string, response: HttpResponse): Buffer {
  if (response.status === 404) {
    throw new OcError('not_found', `Файл «${path}» не найден на контроллере`, {
      hint:
        'Прошивка отвечает так и на каталог. Проверьте путь в oc_files_list: имя ' +
        'должно совпадать с деревом LittleFS.',
      details: { path, status: response.status, body: truncateBody(response.body) },
    });
  }
  if (response.status === 400) {
    throw new OcError('validation_failed', 'Контроллер отклонил путь к файлу', {
      hint: 'Путь должен быть непустым и абсолютным, например "/config/app.json".',
      details: { path, status: response.status, body: truncateBody(response.body) },
    });
  }
  if (response.status < 200 || response.status >= 300) {
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'GET',
      path: `${FILES_PATH}${path}`,
    });
  }

  const contentType = (headerValue(response.headers, 'content-type') ?? '').toLowerCase();
  // Распознавание «вместо файла пришла веб-страница» — общее (`looksLikeSpa`).
  if (looksLikeSpa(response.headers, response.body)) {
    throw new OcError(
      'feature_unsupported',
      'Вместо файла контроллер вернул веб-страницу: маршрут скачивания недоступен',
      {
        hint:
          'Похоже на SPA-заглушку прошивки: неизвестный GET она отдаёт как 200 text/html. ' +
          'Проверьте версию OpenConnect и доступность GET /rest/files.',
        details: {
          path,
          status: response.status,
          content_type: contentType,
          body: bodyForDetails(response.body),
        },
      },
    );
  }

  const bytes = response.bodyBytes ?? Buffer.from(response.body, 'utf8');
  if (looksLikeFileTree(response.headers, bytes)) {
    throw new OcError(
      'feature_unsupported',
      'Маршрут скачивания файла перекрыт обработчиком дерева: прошивка вернула список файлов',
      {
        hint:
          'Так ведут себя прошивки до исправления matcher-а (GET /rest/files перехватывал ' +
          'GET /rest/files/<path>). В текущей ветке маршрут исправлен: обновите прошивку ' +
          'контроллера, после чего скачивание заработает. Повтор запроса на старой прошивке не поможет.',
        details: {
          path,
          status: response.status,
          content_type: contentType,
          body: bodyForDetails(response.body),
        },
      },
    );
  }
  return bytes;
}

/**
 * Отличает JSON-дерево файлов от настоящего файла.
 *
 * Нужно только для диагностики перекрытого маршрута: массив объектов
 * `{name, type:"file"|"directory"}` — это ответ `GET /rest/files`, а не файл.
 * Пустой массив деревом не считаем, чтобы не отвергать пустой `.json`-файл.
 *
 * Разбор байтов — общий `parseJsonBody`/`isJsonContentType` (`device/schemas/common.ts`):
 * своего `JSON.parse` в домене нет.
 */
function looksLikeFileTree(headers: Record<string, string>, bytes: Buffer): boolean {
  if (!isJsonContentType(headers)) return false;
  if (bytes.length === 0 || bytes.length > 2_000_000) return false;
  const parsed = parseJsonBody(bytes.toString('utf8'));
  if (!parsed.ok) return false;
  const value = parsed.value;
  if (!Array.isArray(value) || value.length === 0) return false;
  return value.every((item) => {
    if (typeof item !== 'object' || item === null) return false;
    const record = item as Record<string, unknown>;
    return (
      typeof record.name === 'string' &&
      (record.type === 'file' || record.type === 'directory')
    );
  });
}

/**
 * Копирует файл внутри контроллера: `POST /rest/files/copy` → `201` без тела.
 *
 * @throws OcError `validation_failed` (пути), `not_found` (источника нет),
 *         `device_error` (не удалось создать назначение), `feature_unsupported`.
 */
export async function copyFile(
  client: DeviceClient,
  input: CopyFileInput,
): Promise<CopyFileResult> {
  const source = validateFilePath(input.source, 'source');
  const destination = validateFilePath(input.destination, 'destination');

  return client.withWrite('files', async () => {
    const response = await client.request('POST', FILES_COPY_PATH, { source, destination }, {
      headers: { accept: 'application/json, text/plain' },
    });

    if (response.status === 200 || response.status === 201 || response.status === 202) {
      return { source, destination, observed_at: new Date().toISOString() };
    }
    if (response.status === 404) {
      throw new OcError('not_found', `Источник «${source}» не найден на контроллере`, {
        hint:
          'Прошивка копирует только существующий файл. Проверьте source в oc_files_list: ' +
          'каталог источником не является.',
        details: {
          source,
          destination,
          status: response.status,
          body: truncateBody(response.body),
        },
      });
    }
    if (response.status === 400) {
      throw new OcError('validation_failed', 'Контроллер отклонил параметры копирования', {
        hint: 'Нужны непустые source и destination — абсолютные пути внутри LittleFS.',
        details: {
          source,
          destination,
          status: response.status,
          body: truncateBody(response.body),
        },
      });
    }
    if (response.status === 500) {
      throw new OcError('device_error', 'Контроллер не смог создать файл назначения', {
        hint:
          'Проверьте свободное место и права на LittleFS: прошивка отвечает 500, если ' +
          'не удалось открыть destination на запись.',
        details: {
          source,
          destination,
          status: response.status,
          body: truncateBody(response.body),
        },
      });
    }
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'POST',
      path: FILES_COPY_PATH,
    });
  });
}

/**
 * Удаляет файл и проверяет отсутствие перечитанным деревом.
 *
 * Ответ `204` — «удалено», но прошивка может ответить и `200`; после удаления
 * читаем `GET /rest/files`, чтобы вернуть доказательство `verified_absent`,
 * а не только код ответа.
 *
 * @throws OcError `validation_failed` (путь), `not_found`, `device_error`.
 */
export async function deleteFile(
  client: DeviceClient,
  input: DeleteFileInput,
): Promise<DeleteFileResult> {
  const path = validateFilePath(input.path);

  return client.withWrite('files', async () => {
    const response = await client.request('DELETE', `${FILES_PATH}${encodePathForUrl(path)}`, undefined, {
      headers: { accept: 'application/json, text/plain, */*' },
    });

    if (
      response.status !== 200 &&
      response.status !== 202 &&
      response.status !== 204
    ) {
      if (response.status === 404) {
        throw new OcError('not_found', `Файл «${path}» не найден на контроллере`, {
          hint: 'Проверьте путь в oc_files_list: удалять можно только существующий файл.',
          details: { path, status: response.status, body: truncateBody(response.body) },
        });
      }
      if (response.status === 400) {
        throw new OcError('validation_failed', 'Контроллер отклонил путь к файлу', {
          hint: 'Путь должен быть непустым и абсолютным, например "/config/app.json".',
          details: { path, status: response.status, body: truncateBody(response.body) },
        });
      }
      if (response.status === 500) {
        throw new OcError('device_error', 'Контроллер не смог удалить файл', {
          hint:
            'Прошивка отвечает 500, если LittleFS не дала удалить файл. Проверьте, что это ' +
            'файл (не каталог) и что файловая система не смонтирована только для чтения.',
          details: { path, status: response.status, body: truncateBody(response.body) },
        });
      }
      throw httpError({
        status: response.status,
        body: response.body,
        method: 'DELETE',
        path: `${FILES_PATH}${path}`,
      });
    }

    const observedAt = new Date().toISOString();
    let verifiedAbsent = false;
    let warning: string | null = null;
    try {
      const tree = await readJson(client, FILES_PATH, fileTreeSchema);
      verifiedAbsent = findEntry(tree, path) === null;
      if (!verifiedAbsent) {
        warning =
          `Удаление вернуло ${response.status}, но файл «${path}» всё ещё виден в дереве ` +
          'контроллера: возможно, прошивка не применила изменение сразу.';
      }
    } catch (error) {
      // Удаление уже произошло: ошибка проверки не должна выглядеть как отказ удаления.
      warning =
        'Файл удалён, но проверить дерево файлов после удаления не удалось: ' +
        (error instanceof Error ? error.message : String(error));
    }

    return { path, verified_absent: verifiedAbsent, warning, observed_at: observedAt };
  });
}

/**
 * Имя сохраняемого файла: последний сегмент пути с санитизацией.
 *
 * `../../evil` → `evil`, `..\..\evil.txt` → `evil.txt`: каталоги отбрасываются,
 * управляющие символы и запрещённые в Windows знаки удаляются, ведущие точки —
 * тоже. Пустой результат — `download.bin`.
 */
export function sanitizeFileName(path: string): string {
  let name = path.replace(/\\/g, '/');
  name = name.slice(name.lastIndexOf('/') + 1);

  name = name
    .replace(CONTROL_CHARS, '')
    .replace(/[<>:"|?*]/g, '')
    .trim()
    .replace(/^\.+/, '')
    .replace(/[. ]+$/, '');

  if (name === '' || name === '.' || name === '..') return 'download.bin';
  if (name.length > 120) {
    const dot = name.lastIndexOf('.');
    const extension = dot > 0 && name.length - dot <= 12 ? name.slice(dot) : '';
    name = `${name.slice(0, Math.max(1, 120 - extension.length))}${extension}`;
  }
  return name;
}

/**
 * `save_dir` — только относительный путь внутри рабочего каталога сервера.
 * Абсолютные пути и выход через `..` запрещены: скачанный файл не должен
 * оказаться вне каталога MCP-сервера.
 */
function resolveSaveDir(saveDir: string | undefined, baseDir: string): string {
  const requested = typeof saveDir === 'string' ? saveDir.trim() : DEFAULT_DOWNLOAD_DIR;
  if (requested === '') {
    throw new OcError('validation_failed', 'Каталог сохранения не может быть пустым', {
      hint: `Передайте относительный путь, например ${DEFAULT_DOWNLOAD_DIR}, или не указывайте save_dir.`,
      details: { save_dir: saveDir ?? null },
    });
  }
  if (CONTROL_CHARS.test(requested)) {
    throw new OcError('validation_failed', 'Каталог сохранения содержит управляющие символы', {
      hint: 'Уберите из save_dir управляющие коды.',
      details: { save_dir: requested },
    });
  }
  if (isAbsolute(requested)) {
    throw new OcError('validation_failed', 'save_dir должен быть относительным путём', {
      hint:
        'Абсолютные пути запрещены: файлы сохраняются только внутри рабочего каталога ' +
        `MCP-сервера. Используйте, например, ${DEFAULT_DOWNLOAD_DIR}.`,
      details: { save_dir: requested },
    });
  }

  const base = resolve(baseDir);
  const target = resolve(base, requested);
  const rel = relative(base, target);
  if (rel !== '' && (rel === '..' || rel.startsWith(`..${sep}`) || isAbsolute(rel))) {
    throw new OcError('validation_failed', 'save_dir выходит за пределы рабочего каталога', {
      hint:
        'Файлы нельзя сохранять вне каталога MCP-сервера. Укажите относительный путь ' +
        `внутри него, например ${DEFAULT_DOWNLOAD_DIR}.`,
      details: { save_dir: requested },
    });
  }
  return target;
}

/** Страховка от экзотических имён: итоговый путь обязан лежать прямо в `saveDir`. */
export function ensureInsideDir(saveDir: string, fileName: string): string {
  const candidate = resolve(saveDir, fileName);
  if (dirname(candidate) !== resolve(saveDir)) {
    const fallback = resolve(saveDir, 'download.bin');
    throw new OcError('validation_failed', 'Имя файла выводит за пределы каталога сохранения', {
      hint: 'Имя файла на контроллере содержит недопустимые символы. Укажите другой save_dir.',
      details: { save_dir: saveDir, file_name: fileName, fallback_path: fallback },
    });
  }
  return candidate;
}
