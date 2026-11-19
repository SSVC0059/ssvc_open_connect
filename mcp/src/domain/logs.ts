/**
 * Доменный слой журналов ректификации (WP7, mcp-server-agents.md §11).
 *
 * Главная особенность контракта — **одна stateful-сессия передачи на устройство**
 * (mcp-server.md §2.5 п.6): `GET /rest/logs` из `idle` не просто читает снимок, а
 * ставит контроллеру команду «пришли список», `GET /rest/logs/{id}` запускает
 * передачу файла и может отвечать `202` минутами, `DELETE` освобождает сессию.
 *
 * Отсюда три решения:
 *  1. скачивание целиком идёт через `client.withWrite('logs', …)` — второй
 *     параллельный скачивающий вызов не должен влезть в чужую сессию;
 *  2. `202` — не ошибка, а прогресс: пауза 700 мс и повтор до 200 либо исчерпания бюджета;
 *  3. CSV живёт только в памяти на время сохранения: наружу уходят путь, размер,
 *     число строк и первые 20 строк. Полный журнал в ответ инструмента не попадает.
 *
 * Лимит размера ответа задан транспортом (`src/device/http.ts`, по умолчанию 64 МБ);
 * прошивка ограничивает один журнал 64 КБ (mcp-contract-notes.md §C.8), поэтому
 * отдельный предел на этот эндпоинт не нужен — и в контекст модели CSV не попадает.
 *
 * `DELETE /rest/logs` намеренно НЕ оборачивается в `withWrite('logs')`: отмена должна
 * выполняться, пока скачивание висит в ожидании 202. Иначе она стояла бы в очереди
 * за тем самым вызовом, который требуется прервать.
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
import {
  logsCancelResponseSchema,
  logsErrorBodySchema,
  logsSnapshotSchema,
  type LogsSnapshotBody,
} from '../device/schemas/logs.js';
import { OcError, httpError, truncateBody } from '../errors.js';
import { looksLikeSpa, parseJsonResponse } from './readJson.js';

export const LOGS_PATH = '/rest/logs';

/** Каталог сохранения по умолчанию: относительный, внутри рабочего каталога сервера. */
export const DEFAULT_SAVE_DIR = 'logs/downloads';

/**
 * Суммарный бюджет ожидания готового CSV (202 → повтор).
 *
 * 45 с, а не 90: MCP-клиенты (SDK по умолчанию) обрывают вызов через 60 с, и
 * при бюджете 90 с инструмент мог ответить уже после обрыва — клиент получал бы
 * таймаут транспорта вместо понятного `device_timeout`. Прошивка ограничивает
 * объём журнала, поэтому 45 с хватает с запасом.
 */
export const DOWNLOAD_TIMEOUT_MS = 45_000;

/** Пауза между повторами `202`: прошивка отдаёт журнал кусками. */
export const DOWNLOAD_POLL_MS = 700;

/** В ответе — только первые строки: контекст модели не должен съедать журнал. */
export const PREVIEW_LINES = 20;
export const PREVIEW_LINE_MAX = 200;

export interface LogsListOptions {
  /** Принудительно попросить список заново (`?refresh=1`). */
  refresh?: boolean;
  /**
   * Внутренний путь: только снимок текущей сессии (`?statusOnly=1`),
   * без новой команды контроллеру. Сильнее `refresh`.
   */
  statusOnly?: boolean;
}

/** Снимок состояния сессии журналов в форме, удобной агенту. */
export interface LogsSnapshot {
  /** `idle` | `list` | `receiving` | `completed` | `error` | `busy`. */
  status: string;
  total: number;
  received: number;
  files: string[];
  /** Машинный код ошибки прошивки, если он был в снимке. */
  error: string | null;
  observed_at: string;
}

export interface LogsCancelResult {
  status: string;
  observed_at: string;
}

export interface DownloadLogOptions {
  /** Каталог сохранения относительно `baseDir`; по умолчанию `logs/downloads`. */
  saveDir?: string;
  /** База для относительного `saveDir`; по умолчанию рабочий каталог процесса. */
  baseDir?: string;
  /** Суммарный таймаут ожидания 200, мс (в тестах уменьшается). */
  timeoutMs?: number;
  /** Пауза между повторами 202, мс (в тестах уменьшается). */
  pollMs?: number;
  /** Источник времени (в тестах можно подменить). */
  now?: () => number;
}

/** Результат скачивания: CSV целиком наружу не отдаётся. */
export interface DownloadLogResult {
  saved_path: string;
  bytes: number;
  lines: number;
  preview: string[];
  elapsed_s: number;
}

/**
 * Читает снимок сессии журналов.
 *
 * `GET /rest/logs` без параметров из `idle` ставит контроллеру запрос списка,
 * поэтому первый ответ обычно `receiving`, а список появляется следующим вызовом
 * (mcp-contract-notes.md §F.11). При `409 log_transfer_busy` это не отказ
 * инструмента: возвращаем `status:"busy"` и `ok:true` на уровне инструмента.
 */
export async function listLogs(
  client: DeviceClient,
  opts: LogsListOptions = {},
): Promise<LogsSnapshot> {
  const query =
    opts.statusOnly === true
      ? { statusOnly: 1 }
      : opts.refresh === true
        ? { refresh: 1 }
        : undefined;

  const response = await client.request('GET', LOGS_PATH, undefined, {
    query,
    headers: { accept: 'application/json' },
  });
  const observedAt = new Date().toISOString();

  if (response.status === 409) {
    const failure = explainLogsFailure(response.body);
    if (failure?.error === 'log_transfer_busy') return busySnapshot('log_transfer_busy', observedAt);
    if (failure?.error === 'api_version_unsupported') {
      throw unsupportedLogsError(response, 'GET', LOGS_PATH, failure);
    }
    // Прочие 409 — общая матрица (`httpError` → guard_blocked), успех не выдумываем.
  }
  if (response.status === 503) {
    const failure = explainLogsFailure(response.body);
    if (failure?.error === 'queue_full') throw queueFullError(response, 'GET', LOGS_PATH);
    if (failure?.error === 'log_heap_low') throw heapLowError(response, 'GET', LOGS_PATH);
  }

  const body = parseJsonResponse(response, logsSnapshotSchema, 'GET', LOGS_PATH);
  return toSnapshot(body, observedAt);
}

/**
 * Отменяет текущую передачу: `DELETE /rest/logs` → `{status}`.
 *
 * Без `withWrite` — см. комментарий в шапке файла: отмена должна обгонять
 * зависшее скачивание, а не ждать его.
 */
export async function cancelLogs(client: DeviceClient): Promise<LogsCancelResult> {
  const response = await client.request('DELETE', LOGS_PATH, undefined, {
    headers: { accept: 'application/json' },
  });

  if (response.status === 409) {
    const failure = explainLogsFailure(response.body);
    if (failure?.error === 'api_version_unsupported') {
      throw unsupportedLogsError(response, 'DELETE', LOGS_PATH, failure);
    }
  }
  if (response.status === 503) {
    const failure = explainLogsFailure(response.body);
    if (failure?.error === 'queue_full') throw queueFullError(response, 'DELETE', LOGS_PATH);
    if (failure?.error === 'log_heap_low') throw heapLowError(response, 'DELETE', LOGS_PATH);
  }

  const body = parseJsonResponse(response, logsCancelResponseSchema, 'DELETE', LOGS_PATH);
  return { status: body.status, observed_at: new Date().toISOString() };
}

/**
 * Скачивает журнал по номеру процесса и сохраняет его в файл.
 *
 * Цикл: `GET /rest/logs/{id}` → `202` → пауза → повтор, пока не `200`; бюджет —
 * `timeoutMs` (по умолчанию 45 с). На `200` тело — CSV: считаем размер, строки и первые 20 строк,
 * пишем файл. Имя берётся из `Content-Disposition`, иначе `<id>.csv`; путь всегда
 * остаётся внутри `saveDir`.
 *
 * @throws OcError `validation_failed` (неверный id/каталог), `busy` (409 busy,
 *         503 heap_low), `queue_full`, `device_timeout` (202 не успел),
 *         `feature_unsupported` (старая прошивка), `not_found`, `device_error`.
 */
export async function downloadLog(
  client: DeviceClient,
  processId: number,
  opts: DownloadLogOptions = {},
): Promise<DownloadLogResult> {
  const id = normalizeProcessId(processId);
  const timeoutMs = normalizeTimeout(opts.timeoutMs);
  const pollMs = Math.max(0, opts.pollMs ?? DOWNLOAD_POLL_MS);
  const now = opts.now ?? Date.now;
  const saveDir = resolveSaveDir(opts.saveDir, opts.baseDir ?? process.cwd());

  // Один скачивающий вызов на устройство: вторая сессия передачи не создаётся.
  return client.withWrite('logs', async () => {
    const started = now();
    const transfer = await receiveCsv(client, id, { timeoutMs, pollMs, now, started });
    return saveCsv(transfer, id, saveDir, now() - started);
  });
}

interface ReceiveOptions {
  timeoutMs: number;
  pollMs: number;
  now: () => number;
  started: number;
}

interface ReceivedCsv {
  csv: string;
  /** Имя из `Content-Disposition` (уже разобрано, но ещё не санитизировано). */
  fileName: string | null;
  received: number;
  total: number;
}

/** Поллинг 202 до готового CSV. Прогресс последнего 202 попадает в ошибку таймаута. */
async function receiveCsv(
  client: DeviceClient,
  id: number,
  opts: ReceiveOptions,
): Promise<ReceivedCsv> {
  const path = `${LOGS_PATH}/${id}`;
  const deadline = opts.started + opts.timeoutMs;
  let last: LogsSnapshotBody | null = null;

  for (;;) {
    const remaining = deadline - opts.now();
    if (remaining <= 0) throw transferTimeoutError(id, opts.timeoutMs, last);

    // Остаток бюджета ограничивает и сам HTTP-запрос: зависший сокет не должен
    // растянуть ожидание на неопределённое время.
    const response = await client.request('GET', path, undefined, {
      headers: { accept: 'text/csv, application/json' },
      timeoutMs: Math.max(1, remaining),
    });

    if (response.status === 200) {
      const csv = csvBody(response);
      return {
        csv,
        fileName: filenameFromContentDisposition(
          headerValue(response.headers, 'content-disposition'),
        ),
        received: last?.received ?? 0,
        total: last?.total ?? 0,
      };
    }

    if (response.status === 202) {
      const progress = parseJsonResponse(response, logsSnapshotSchema, 'GET', path);
      // Контроллер отдаёт терминальный отказ тоже кодом 202 с `status:"error"`
      // (docs/develop/api/logs.md): err_mnt/err_nf/err_num/err_int/timeout.
      // Без этой ветки такой ответ висел бы до общего таймаута.
      if (progress.status === 'error') throw transferStateError(id, progress);
      last = progress;
      const left = deadline - opts.now();
      if (left <= 0) throw transferTimeoutError(id, opts.timeoutMs, last);
      await sleep(Math.min(opts.pollMs, left));
      continue;
    }

    throw downloadFailure(response, id);
  }
}

/** 200 на `/rest/logs/{id}` обязан быть CSV: JSON/HTML означают чужой ответ. */
function csvBody(response: HttpResponse): string {
  const contentType = (headerValue(response.headers, 'content-type') ?? '').toLowerCase();
  if (isJsonContentType(response.headers)) {
    throw new OcError('device_error', 'Контроллер вернул JSON вместо CSV журнала', {
      hint:
        'Прошивка ответила не файлом журнала. Проверьте версию контроллера и повторите ' +
        'скачивание; тело ответа — в details.',
      details: {
        status: response.status,
        content_type: contentType,
        body: bodyForDetails(response.body),
      },
    });
  }
  // Распознавание SPA-заглушки общее (`looksLikeSpa`): Content-Type text/html или HTML в теле.
  if (looksLikeSpa(response.headers, response.body)) {
    throw new OcError(
      'feature_unsupported',
      'Вместо CSV контроллер вернул веб-страницу: маршрут скачивания журнала недоступен',
      {
        hint:
          'Похоже на SPA-заглушку старой прошивки. Обновите OpenConnect: скачивание журналов ' +
          'требует API 1.10 и возможности get_log.',
        details: {
          status: response.status,
          content_type: contentType,
          body: bodyForDetails(response.body),
        },
      },
    );
  }
  return response.body;
}

/** Отказ скачивания: коды §D.8 разбираются по `error`, остальное — общей матрицей. */
function downloadFailure(response: HttpResponse, id: number): OcError {
  const failure = explainLogsFailure(response.body);

  if (response.status === 404) {
    return new OcError('not_found', `Журнал процесса ${id} не найден`, {
      hint:
        'Проверьте номер процесса в oc_logs_list. Если список пуст, журналов на контроллере ' +
        'ещё нет; если прошивка старая, скачивание журналов может быть недоступно.',
      details: {
        status: response.status,
        process_id: id,
        body: truncateBody(response.body),
      },
    });
  }
  if (response.status === 400) {
    return new OcError(
      'validation_failed',
      `Контроллер отклонил номер процесса ${id}`,
      {
        hint: 'Номер процесса — целое число ≥ 1 из списка oc_logs_list.',
        details: {
          status: response.status,
          process_id: id,
          error: failure?.error ?? null,
          body: truncateBody(response.body),
        },
      },
    );
  }
  if (response.status === 409) {
    if (failure?.error === 'log_transfer_busy') {
      return busyTransferError(response, id);
    }
    if (failure?.error === 'api_version_unsupported') {
      return unsupportedLogsError(response, 'GET', `${LOGS_PATH}/${id}`, failure);
    }
  }
  if (response.status === 503) {
    if (failure?.error === 'queue_full') {
      return queueFullError(response, 'GET', `${LOGS_PATH}/${id}`);
    }
    if (failure?.error === 'log_heap_low') {
      return heapLowError(response, 'GET', `${LOGS_PATH}/${id}`);
    }
  }
  return httpError({
    status: response.status,
    body: response.body,
    method: 'GET',
    path: `${LOGS_PATH}/${id}`,
  });
}

function transferTimeoutError(
  id: number,
  timeoutMs: number,
  last: LogsSnapshotBody | null,
): OcError {
  return new OcError(
    'device_timeout',
    `Передача журнала процесса ${id} не завершилась за ${Math.round(timeoutMs / 1000)} с`,
    {
      hint:
        'Передача на устройстве могла остаться активной. Вызовите oc_logs_cancel, чтобы ' +
        'освободить сессию передачи, затем повторите скачивание.',
      retryable: true,
      details: {
        process_id: id,
        timeout_ms: timeoutMs,
        last_status: last?.status ?? null,
        last_received: last?.received ?? null,
        last_total: last?.total ?? null,
      },
    },
  );
}

/**
 * Терминальный отказ `202 {status:"error", error:"…"}` (docs/develop/api/logs.md).
 * Коды прошивки: `err_num`, `err_mnt`, `err_nf`, `err_int`, `timeout`.
 */
function transferStateError(id: number, snapshot: LogsSnapshotBody): OcError {
  const deviceError = snapshot.error ?? 'unknown';
  const details = {
    process_id: id,
    device_status: snapshot.status,
    device_error: deviceError,
    received: snapshot.received ?? null,
    total: snapshot.total ?? null,
  };

  if (deviceError === 'err_nf' || deviceError === 'err_num') {
    return new OcError('not_found', `Журнал процесса ${id} не найден на контроллере`, {
      hint:
        'Проверьте номер процесса в oc_logs_list. Журнал мог быть удалён или ещё не создан ' +
        'на карте памяти контроллера.',
      details,
    });
  }
  if (deviceError === 'timeout') {
    return new OcError('device_timeout', 'Контроллер прервал передачу журнала по таймауту', {
      hint:
        'Контроллер не присылал данные дольше 10 секунд. Вызовите oc_logs_cancel, чтобы ' +
        'освободить сессию, и повторите скачивание.',
      retryable: true,
      details,
    });
  }
  if (deviceError === 'err_mnt') {
    return new OcError(
      'device_error',
      'Контроллер не смог прочитать журнал: ошибка файловой системы (err_mnt)',
      {
        hint:
          'Проверьте SD-карту контроллера: она не смонтирована или недоступна. Пока файловая ' +
          'система не восстановлена, список и скачивание журналов работать не будут.',
        details,
      },
    );
  }
  return new OcError('device_error', 'Контроллер сообщил об ошибке передачи журнала', {
    hint:
      'Прошивка вернула терминальный код ошибки. Проверьте details.device_error и состояние ' +
      'контроллера; при необходимости повторите скачивание.',
    details,
  });
}

function busyTransferError(response: HttpResponse, id: number): OcError {
  return new OcError('busy', 'Передача журнала уже идёт', {
    hint:
      'Дождитесь завершения текущей передачи или вызовите oc_logs_cancel, чтобы прервать ' +
      'её, затем повторите скачивание.',
    retryable: true,
    details: {
      status: response.status,
      process_id: id,
      error: 'log_transfer_busy',
      body: truncateBody(response.body),
    },
  });
}

function heapLowError(response: HttpResponse, method: string, path: string): OcError {
  return new OcError('busy', 'На контроллере недостаточно памяти для журнала', {
    hint:
      'Повторите позже: контроллеру не хватает кучи под передачу. Если передача зависла, ' +
      'вызовите oc_logs_cancel и освободите сессию.',
    retryable: true,
    details: {
      status: response.status,
      method,
      path,
      error: 'log_heap_low',
      body: truncateBody(response.body),
    },
  });
}

function queueFullError(response: HttpResponse, method: string, path: string): OcError {
  return new OcError('queue_full', 'Очередь контроллера переполнена', {
    hint: 'Подождите несколько секунд и повторите: очередь команд разгрузится сама.',
    retryable: true,
    details: {
      status: response.status,
      method,
      path,
      error: 'queue_full',
      body: truncateBody(response.body),
    },
  });
}

function unsupportedLogsError(
  response: HttpResponse,
  method: string,
  path: string,
  failure: LogsFailure | null,
): OcError {
  const required = failure?.required_api ?? '1.10';
  const deviceApi = failure?.device_api ?? null;
  return new OcError('feature_unsupported', 'Прошивка не поддерживает журналы ректификации', {
    hint:
      `Скачивание журналов требует API ${required}` +
      (deviceApi === null ? '' : `, контроллер отвечает ${deviceApi}`) +
      '. Обновите прошивку SSVC; повтор запроса не поможет.',
    details: {
      status: response.status,
      method,
      path,
      error: 'api_version_unsupported',
      feature: failure?.feature ?? 'get_log',
      required_api: required,
      device_api: deviceApi,
      body: truncateBody(response.body),
    },
  });
}

interface LogsFailure {
  error?: string;
  feature?: string;
  required_api?: string;
  device_api?: string;
}

/** Код ошибки прошивки из JSON или text/plain тела. */
function explainLogsFailure(body: string): LogsFailure | null {
  const parsed = parseJsonBody(body);
  if (parsed.ok && typeof parsed.value === 'object' && parsed.value !== null) {
    const candidate = logsErrorBodySchema.safeParse(parsed.value);
    if (candidate.success) return candidate.data;
    const raw = parsed.value as Record<string, unknown>;
    if (typeof raw.error === 'string') return { error: raw.error };
    return null;
  }
  // Часть ответов прошивки приходит текстом; код всё равно нужно распознать.
  for (const code of ['log_transfer_busy', 'log_heap_low', 'queue_full', 'api_version_unsupported']) {
    if (body.includes(code)) return { error: code };
  }
  return null;
}

function toSnapshot(body: LogsSnapshotBody, observedAt: string): LogsSnapshot {
  return {
    status: body.status,
    total: body.total ?? 0,
    received: body.received ?? 0,
    files: body.files ?? [],
    error: body.error ?? null,
    observed_at: observedAt,
  };
}

function busySnapshot(error: string, observedAt: string): LogsSnapshot {
  return { status: 'busy', total: 0, received: 0, files: [], error, observed_at: observedAt };
}

function normalizeProcessId(value: unknown): number {
  if (typeof value !== 'number' || !Number.isInteger(value) || value < 1) {
    throw new OcError('validation_failed', 'Номер процесса должен быть целым числом ≥ 1', {
      hint: 'Возьмите номер из oc_logs_list: файл «12.CSV» — это процесс 12.',
      details: { process_id: value ?? null },
    });
  }
  return value;
}

function normalizeTimeout(value: number | undefined): number {
  const timeout = value ?? DOWNLOAD_TIMEOUT_MS;
  if (!Number.isFinite(timeout) || timeout <= 0) {
    throw new OcError('validation_failed', 'Таймаут скачивания должен быть положительным', {
      details: { timeout_ms: value ?? null },
    });
  }
  return timeout;
}

/**
 * `save_dir` — только относительный путь внутри рабочего каталога сервера
 * (mcp-server-agents.md §11: «вне репозитория не создавать»).
 */
function resolveSaveDir(saveDir: string | undefined, baseDir: string): string {
  const requested = typeof saveDir === 'string' ? saveDir.trim() : DEFAULT_SAVE_DIR;
  if (requested === '') {
    throw new OcError('validation_failed', 'Каталог сохранения не может быть пустым', {
      hint: `Передайте относительный путь, например ${DEFAULT_SAVE_DIR}, или не указывайте save_dir.`,
      details: { save_dir: saveDir ?? null },
    });
  }
  // Та же проверка, что в files.resolveSaveDir: путь не должен нести управляющие
  // символы — иначе ОС/терминал получают неожиданные последовательности.
  // eslint-disable-next-line no-control-regex
  if (/[\u0000-\u001f\u007f]/.test(requested)) {
    throw new OcError('validation_failed', 'Каталог сохранения содержит управляющие символы', {
      hint: 'Уберите из save_dir управляющие коды.',
      details: { save_dir: requested },
    });
  }
  if (isAbsolute(requested)) {
    throw new OcError('validation_failed', 'save_dir должен быть относительным путём', {
      hint:
        'Абсолютные пути запрещены: журналы сохраняются только внутри рабочего каталога ' +
        `MCP-сервера. Используйте, например, ${DEFAULT_SAVE_DIR}.`,
      details: { save_dir: requested },
    });
  }

  const base = resolve(baseDir);
  const target = resolve(base, requested);
  const rel = relative(base, target);
  if (rel !== '' && (rel === '..' || rel.startsWith(`..${sep}`) || isAbsolute(rel))) {
    throw new OcError('validation_failed', 'save_dir выходит за пределы рабочего каталога', {
      hint:
        'Журналы нельзя сохранять вне каталога MCP-сервера. Укажите относительный путь ' +
        `внутри него, например ${DEFAULT_SAVE_DIR}.`,
      details: { save_dir: requested },
    });
  }
  return target;
}

/** Имя файла из `Content-Disposition` (`filename` или RFC 5987 `filename*`). */
function filenameFromContentDisposition(header: string | undefined): string | null {
  if (header === undefined || header.trim() === '') return null;

  const extended = /(?:^|;)\s*filename\*\s*=\s*([^;]+)/i.exec(header);
  if (extended !== null) {
    let value = extended[1].trim();
    value = value.replace(/^UTF-8''/i, '');
    if (value.startsWith('"') && value.endsWith('"')) value = value.slice(1, -1);
    try {
      return decodeURIComponent(value);
    } catch {
      return value;
    }
  }

  const quoted = /(?:^|;)\s*filename\s*=\s*"([^"]*)"/i.exec(header);
  if (quoted !== null) return quoted[1];

  const bare = /(?:^|;)\s*filename\s*=\s*([^;]+)/i.exec(header);
  return bare === null ? null : bare[1].trim();
}

/**
 * Санитизация имени: убираем каталоги, управляющие символы и запрещённые в Windows
 * знаки, ведущие точки; расширение всегда `.csv`. `../../evil.csv` → `evil.csv`,
 * то есть файл остаётся внутри `save_dir`.
 */
export function sanitizeFileName(raw: string | null, fallback: string): string {
  if (raw === null) return fallback;

  let name = raw.replace(/\\/g, '/');
  const slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.slice(slash + 1);

  name = name
    // eslint-disable-next-line no-control-regex
    .replace(/[\u0000-\u001f\u007f]/g, '')
    .replace(/[<>:"|?*]/g, '')
    .trim()
    .replace(/^\.+/, '')
    .replace(/[. ]+$/, '');

  if (name === '' || name === '.' || name === '..') return fallback;
  if (name.length > 120) name = name.slice(0, 120);
  if (!/\.csv$/i.test(name)) name = `${name}.csv`;
  return name;
}

async function saveCsv(
  transfer: ReceivedCsv,
  id: number,
  saveDir: string,
  elapsedMs: number,
): Promise<DownloadLogResult> {
  const fallback = `${id}.csv`;
  let fileName = sanitizeFileName(transfer.fileName, fallback);
  let savedPath = resolve(saveDir, fileName);

  // Страховка от экзотических платформенных имён: итоговый путь строго в saveDir.
  if (dirname(savedPath) !== saveDir) {
    fileName = fallback;
    savedPath = resolve(saveDir, fileName);
  }

  const metrics = csvMetrics(transfer.csv);
  await mkdir(saveDir, { recursive: true });
  await writeFile(savedPath, transfer.csv, 'utf8');

  return {
    saved_path: savedPath,
    bytes: Buffer.byteLength(transfer.csv, 'utf8'),
    lines: metrics.lines,
    preview: metrics.preview,
    // 3 знака после запятой: миллисекунды в отчёте не нужны.
    elapsed_s: Number((elapsedMs / 1000).toFixed(3)),
  };
}

/**
 * Размер и первые строки CSV.
 *
 * BOM (`\uFEFF`) отбрасывается до подсчёта, иначе первая строка начиналась бы с
 * невидимого символа. Строки считаются по `\r\n`, `\n` и `\r`; завершающий перевод
 * строки не добавляет пустую запись. Каждая строка preview усечена до 200 символов.
 */
export function csvMetrics(csv: string): { lines: number; preview: string[] } {
  let text = csv;
  if (text.charCodeAt(0) === 0xfeff) text = text.slice(1);
  if (text === '') return { lines: 0, preview: [] };

  const rows = text.split(/\r\n|\n|\r/);
  if (rows.length > 0 && rows[rows.length - 1] === '') rows.pop();

  return {
    lines: rows.length,
    preview: rows
      .slice(0, PREVIEW_LINES)
      .map((line) => (line.length > PREVIEW_LINE_MAX ? line.slice(0, PREVIEW_LINE_MAX) : line)),
  };
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolveSleep) => {
    const timer = setTimeout(resolveSleep, ms);
    timer.unref?.();
  });
}
