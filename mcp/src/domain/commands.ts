/**
 * Доменный слой команд SSVC (WP2, mcp-server-agents.md §6 + «Обновление после §9»).
 *
 * Две особенности прошивки, из-за которых этот слой существует:
 *
 * 1. `POST /rest/commands` асинхронен: 200/accepted означает «принято в очередь»,
 *    а не «выполнено». Успешный ответ теперь JSON с `commandId`; старые прошивки
 *    отвечают `text/plain "Command accepted"` без идентификатора. Журнал попытки
 *    живёт 10 минут кольцом на 16 записей, поэтому `GET /rest/commands/{id}` может
 *    ответить 404 — это «запись вытеснена», а не «команда не выполнялась».
 *
 * 2. Терминальные состояния различаются по смыслу: `sent` — норма для
 *    start/next/pause/resume/stop (ответа SSVC не ждут), `done` — SSVC ответил.
 *    В любом случае `confidence:"transport"` значит «ушло в UART», физического
 *    подтверждения действия сервер не получает.
 *
 * SPA-заглушка (`200 text/html`) на `/rest/commands` — это не успех: старый или
 * чужой сервер вернул веб-страницу вместо JSON, команда никуда не пошла.
 * Распознавание такой страницы — общее (`looksLikeSpa` из `readJson.ts`), чтобы
 * «веб-страница вместо API» определялась одинаково во всех доменах.
 */

import type { DeviceClient, HttpResponse } from '../device/DeviceClient.js';
import {
  bodyForDetails,
  headerValue,
  isJsonContentType,
  parseJsonBody,
} from '../device/schemas/common.js';
import {
  UNKNOWN_COMMAND_STATE,
  commandAcceptedBodySchema,
  commandJournalEntrySchema,
  commandRejectedBodySchema,
  isTerminalCommandState,
  normalizeCommandState,
  parseCommandId,
} from '../device/schemas/commands.js';
import { OcError, httpError } from '../errors.js';
import { looksLikeSpa } from './readJson.js';
import {
  buildStateView,
  fetchSnapshot,
  stalenessTrackerFor,
  type StateView,
} from './telemetry.js';

export const COMMANDS_PATH = '/rest/commands';

export function commandStatusPath(id: number): string {
  return `${COMMANDS_PATH}/${id}`;
}

// ---------------------------------------------------------------------------
// Отправка команды
// ---------------------------------------------------------------------------

export interface CommandDispatch {
  accepted: true;
  status: 'accepted';
  command: string;
  /**
   * Идентификатор записи журнала. null — старая прошивка ответила text/plain
   * без commandId: команда принята, но проследить её судьбу нельзя.
   */
  command_id: number | null;
  warnings: string[];
  /** Подтверждение всегда транспортное: «ушло в очередь/UART». */
  confidence: 'transport';
  /** Текст исходного ответа, когда прошивка ответила не JSON (диагностика). */
  raw?: string;
}

export interface SendCommandOptions {
  /** Параметры команды (для SET/AT/GET_LOG); по умолчанию пустая строка. */
  parameters?: string;
  /**
   * Защитное действие: отправить вне общей очереди записей (`withUrgent`).
   * Нужно только EMERGENCY_STOP — ожидание за скачиванием журнала недопустимо.
   */
  urgent?: boolean;
}

function commandDetails(name: string, response: HttpResponse): Record<string, unknown> {
  return {
    command: name,
    status: response.status,
    content_type: headerValue(response.headers, 'content-type') ?? null,
    body: bodyForDetails(response.body),
  };
}

function commandIdFromHeaders(headers: Record<string, string>): number | null {
  return parseCommandId(headerValue(headers, 'x-command-id'));
}

function guardBlocked(
  name: string,
  blockReasons: string[],
  details: Record<string, unknown>,
): OcError {
  const reasonText = blockReasons.length > 0 ? blockReasons.join('; ') : 'причина не указана';
  return new OcError(
    'guard_blocked',
    `Контроллер отклонил команду «${name}»: сработал предохранитель`,
    {
      hint:
        'Команда не отправлена в UART. Устраните причины из details.block_reasons: ' +
        'проверьте датчики уровня XKC-25, их калибровку и аппаратные сбои, затем повторите.',
      retryable: false,
      details: { ...details, block_reasons: blockReasons, reasons_text: reasonText },
    },
  );
}

function featureUnsupported(name: string, details: Record<string, unknown>, why: string): OcError {
  return new OcError(
    'feature_unsupported',
    `Команда «${name}» не поддерживается этой версией прошивки SSVC`,
    {
      hint:
        'Обновите прошивку SSVC до версии с нужной командой (или не используйте её). ' +
        'Повтор запроса не поможет: ограничение версии UART API.',
      retryable: false,
      details: { ...details, reason: why },
    },
  );
}

/**
 * Разбирает ответ `POST /rest/commands`:
 *  200 JSON/text/plain → accepted (+command_id/warnings);
 *  409 JSON blockReasons → guard_blocked;
 *  409 text  → feature_unsupported;
 *  503 text  → queue_full;
 *  501 text  → unsupported_operation (not_implemented).
 *
 * POST не ретраится: повторять постановку команды должен клиент, осознанно.
 */
function parseCommandResponse(name: string, response: HttpResponse): CommandDispatch {
  const details = commandDetails(name, response);

  if (looksLikeSpa(response.headers, response.body)) {
    // Не «успех»: вместо JSON пришла веб-страница. Команда не отправлена.
    throw new OcError(
      'feature_unsupported',
      `POST /rest/commands вернул веб-страницу вместо ответа API: команда «${name}» не отправлена`,
      {
        hint:
          'По этому адресу отвечает не API контроллера (SPA-заглушка или прокси). ' +
          'Проверьте SSVC_DEVICE_URL и версию прошивки; POST не повторяйте.',
        retryable: false,
        details: { ...details, reason: 'spa_html_response' },
      },
    );
  }

  switch (response.status) {
    case 200:
    case 201:
    case 202: {
      const parsed = parseJsonBody(response.body);
      if (!parsed.ok) {
        if (isJsonContentType(response.headers)) {
          throw new OcError('device_error', `Ответ на команду «${name}» не является корректным JSON`, {
            hint:
              'Прошивка ответила JSON-типом, но тело не разобралось. ' +
              'Включите SSVC_RAW_LOG=all и посмотрите сырой обмен.',
            details: { ...details, parse_error: parsed.error },
          });
        }
        // Старые прошивки: 200 text/plain "Command accepted" без commandId.
        return {
          accepted: true,
          status: 'accepted',
          command: name,
          command_id: commandIdFromHeaders(response.headers),
          warnings: [],
          confidence: 'transport',
          raw: bodyForDetails(response.body),
        };
      }

      const accepted = commandAcceptedBodySchema.safeParse(parsed.value);
      if (!accepted.success) {
        throw new OcError('device_error', `Ответ на команду «${name}» имеет неожиданную форму`, {
          hint:
            'Ожидался {status:"accepted", command, commandId, warnings?[]}. ' +
            'Проверьте версию прошивки контроллера и details.issues.',
          details: {
            ...details,
            issues: accepted.error.issues.map((issue) => ({
              path: issue.path.join('.'),
              message: issue.message,
            })),
          },
        });
      }

      const data = accepted.data;
      if (data.status !== undefined && data.status.trim().toLowerCase() !== 'accepted') {
        throw new OcError('device_error', `Контроллер вернул 200, но статус команды «${data.status}»`, {
          hint:
            'Контракт прошивки нарушен: успешный HTTP-код должен сопровождаться status:"accepted". ' +
            'Проверьте версию прошивки и сырой обмен.',
          details,
        });
      }

      return {
        accepted: true,
        status: 'accepted',
        command: data.command ?? name,
        command_id: parseCommandId(data.commandId) ?? commandIdFromHeaders(response.headers),
        warnings: data.warnings ?? [],
        confidence: 'transport',
      };
    }

    case 409: {
      const parsed = parseJsonBody(response.body);
      if (parsed.ok) {
        const rejected = commandRejectedBodySchema.safeParse(parsed.value);
        if (rejected.success && rejected.data.blockReasons !== undefined) {
          throw guardBlocked(name, rejected.data.blockReasons, details);
        }
        // 409 JSON без blockReasons: всё равно предохранитель, но причин нет.
        if (typeof parsed.value === 'object' && parsed.value !== null) {
          throw guardBlocked(name, [], details);
        }
      }
      if (/unsupported/i.test(response.body)) {
        throw featureUnsupported(name, details, 'feature_unsupported');
      }
      throw guardBlocked(name, [], details);
    }

    case 501:
      throw new OcError(
        'unsupported_operation',
        `Команда «${name}» не реализована в этой прошивке`,
        {
          hint:
            'Имени нет в COMMAND_MAP прошивки. Проверьте версию контроллера: ' +
            'команда появится только в новой прошивке. Повтор не поможет.',
          retryable: false,
          details: { ...details, reason: 'not_implemented' },
        },
      );

    case 503:
      if (/queue/i.test(response.body)) {
        throw new OcError('queue_full', 'Очередь команд контроллера переполнена', {
          hint:
            'Подождите несколько секунд и повторите вызов: очередь разгрузится сама. ' +
            'Инструмент POST автоматически не повторяет — решение за клиентом.',
          retryable: true,
          details: { ...details, reason: 'queue_full' },
        });
      }
      throw new OcError('busy', 'Контроллер занят и не принял команду', {
        hint: 'Повторите вызов через несколько секунд.',
        retryable: true,
        details,
      });

    case 404:
      throw new OcError('feature_unsupported', `Маршрут команд не найден на контроллере`, {
        hint:
          'Похоже, прошивка не поддерживает POST /rest/commands или адрес указан неверно. ' +
          'Проверьте SSVC_DEVICE_URL и версию прошивки.',
        retryable: false,
        details: { ...details, reason: 'route_not_found' },
      });

    case 400:
      throw new OcError('validation_failed', `Контроллер отклонил параметры команды «${name}»`, {
        hint: 'Проверьте имя команды и parameters; тело ответа — в details.body.',
        details,
      });

    default:
      throw httpError({
        status: response.status,
        body: response.body,
        method: 'POST',
        path: COMMANDS_PATH,
      });
  }
}

/**
 * Ставит команду в очередь контроллера. Все записи идут через `withWrite`,
 * чтобы не пересекаться с другими изменяющими вызовами; защитные команды
 * (`urgent`) идут отдельной дорожкой и общую очередь не ждут.
 *
 * Бросает `OcError` по контракту §2.1; 200/accepted — не исключение.
 */
export async function sendCommand(
  client: DeviceClient,
  name: string,
  options: SendCommandOptions = {},
): Promise<CommandDispatch> {
  const parameters = options.parameters ?? '';
  const send = async (): Promise<CommandDispatch> => {
    const payload = { commands: name, parameters };
    // ESPAsyncWebServer не разбирает chunked-body: Node без Content-Length шлёт
    // Transfer-Encoding: chunked, и контроллер рвёт соединение (ECONNRESET).
    // Заголовок задаём здесь, потому что тело сериализует DeviceClient.request.
    const bodyLength = Buffer.byteLength(JSON.stringify(payload), 'utf8');
    const response = await client.request('POST', COMMANDS_PATH, payload, {
      headers: { 'content-length': String(bodyLength) },
    });
    return parseCommandResponse(name, response);
  };
  return options.urgent === true
    ? client.withUrgent('commands', send)
    : client.withWrite('commands', send);
}

// ---------------------------------------------------------------------------
// Журнал команд
// ---------------------------------------------------------------------------

export interface CommandJournalView {
  command_id: number;
  /** queued | running | sent | done | rejected | timeout | unknown. */
  state: string;
  /** true — состояние терминальное либо запись вытеснена (unknown). */
  terminal: boolean;
  /** true — журнал ответил 404: запись вытеснена/истекла, а не «команда не выполнялась». */
  evicted: boolean;
  command: string | null;
  ok: boolean | null;
  reason: string | null;
  confidence: string | null;
  queued_at: number | null;
  started_at: number | null;
  finished_at: number | null;
  note: string | null;
}

function uptimeOf(value: number | string | null | undefined): number | null {
  if (value === null || value === undefined) return null;
  if (typeof value === 'number') return Number.isFinite(value) ? value : null;
  const parsed = Number(value.trim());
  return Number.isFinite(parsed) ? parsed : null;
}

function evictedView(commandId: number): CommandJournalView {
  return {
    command_id: commandId,
    state: UNKNOWN_COMMAND_STATE,
    terminal: true,
    evicted: true,
    command: null,
    ok: null,
    reason: null,
    confidence: null,
    queued_at: null,
    started_at: null,
    finished_at: null,
    note:
      'Запись вытеснена из журнала команд (журнал хранит 16 записей и TTL 10 минут). ' +
      'Это не значит, что команда не выполнялась: состояние проверяйте по телеметрии ' +
      '(oc_wait_for_change / oc_get_state).',
  };
}

/**
 * Читает запись журнала `GET /rest/commands/{id}`.
 *
 * 404 — не ошибка выполнения: возвращаем `state:"unknown"` и `evicted:true`.
 * 400 invalid_id — ошибка аргумента (локально id уже проверен).
 */
export async function getCommandStatus(
  client: DeviceClient,
  commandId: number,
): Promise<CommandJournalView> {
  if (!Number.isSafeInteger(commandId) || commandId < 0) {
    throw new OcError('validation_failed', 'Некорректный идентификатор команды', {
      hint: 'command_id — неотрицательное целое число из ответа POST /rest/commands.',
      details: { reason: 'invalid_argument', command_id: commandId },
    });
  }

  const path = commandStatusPath(commandId);
  const response = await client.request('GET', path, undefined, {
    headers: { accept: 'application/json' },
    noSingleFlight: true,
  });

  if (looksLikeSpa(response.headers, response.body)) {
    throw new OcError('device_error', `GET ${path} вернул веб-страницу вместо журнала команд`, {
      hint:
        'Похоже, маршрут журнала отсутствует или по адресу отвечает не API. ' +
        'Проверьте версию прошивки и SSVC_DEVICE_URL.',
      details: {
        status: response.status,
        path,
        content_type: headerValue(response.headers, 'content-type') ?? null,
        body: bodyForDetails(response.body),
      },
    });
  }

  if (response.status === 404) {
    // Журнал — кольцо на 16 записей с TTL: 404 означает «запись вытеснена».
    return evictedView(commandId);
  }

  if (response.status === 400) {
    throw new OcError('validation_failed', 'Контроллер считает идентификатор команды некорректным', {
      hint: 'Проверьте command_id: журнал принимает только числовой id из ответа POST.',
      details: {
        status: response.status,
        path,
        body: bodyForDetails(response.body),
      },
    });
  }

  if (response.status < 200 || response.status >= 300) {
    throw httpError({ status: response.status, body: response.body, method: 'GET', path });
  }

  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) {
    throw new OcError('device_error', `Ответ ${path} не является корректным JSON`, {
      hint: 'Проверьте версию прошивки контроллера и включите SSVC_RAW_LOG=all.',
      details: {
        status: response.status,
        path,
        parse_error: parsed.error,
        body: bodyForDetails(response.body),
      },
    });
  }

  const entry = commandJournalEntrySchema.safeParse(parsed.value);
  if (!entry.success) {
    throw new OcError('device_error', `Ответ ${path} не соответствует журналу команд`, {
      hint:
        'Ожидался {id, command, state, queued_at, started_at, finished_at, ok, reason, confidence}. ' +
        'Проверьте версию прошивки и details.issues.',
      details: {
        status: response.status,
        path,
        issues: entry.error.issues.map((issue) => ({
          path: issue.path.join('.'),
          message: issue.message,
        })),
        body: bodyForDetails(response.body),
      },
    });
  }

  const data = entry.data;
  const state = normalizeCommandState(data.state);
  return {
    command_id: parseCommandId(data.id) ?? commandId,
    state,
    terminal: isTerminalCommandState(state),
    evicted: false,
    command: data.command,
    ok: data.ok ?? null,
    reason: data.reason ?? null,
    confidence: data.confidence ?? null,
    queued_at: uptimeOf(data.queued_at),
    started_at: uptimeOf(data.started_at),
    finished_at: uptimeOf(data.finished_at),
    note: null,
  };
}

export interface WaitForCommandTerminalOptions {
  /** Общий предел ожидания, мс. По умолчанию 15 000. */
  timeoutMs?: number;
  /** Интервал опроса, мс. По умолчанию 300. */
  pollMs?: number;
  /**
   * Считать `sent` терминальным. По умолчанию false: функция вызывается для команд,
   * которые ждут ответа SSVC (status/version/get_settings/set/get_log). На живом
   * устройстве журнал сначала показывает `sent` (успели отправить в UART, ответа
   * ещё нет), а через десятки-сотни миллисекунд — `done`. Остановка на `sent`
   * вернула бы «нет ответа» вместо реального результата. Для start/next/pause/
   * resume/stop ожидание не используется вовсе (их терминальное состояние — `sent`).
   */
  sentIsTerminal?: boolean;
  /** Инъекция времени/паузы для тестов. */
  now?: () => number;
  sleep?: (ms: number) => Promise<void>;
}

export interface CommandJournalWait extends CommandJournalView {
  /** true — не дождались терминального состояния за timeoutMs. */
  timed_out: boolean;
  elapsed_s: number;
  polls: number;
}

function defaultSleep(ms: number): Promise<void> {
  return new Promise((resolve) => {
    const timer = setTimeout(resolve, ms);
    timer.unref?.();
  });
}

function roundSeconds(ms: number): number {
  return Math.max(0, Math.round(ms)) / 1000;
}

/**
 * Опрашивает журнал до терминального состояния (done/rejected/timeout; `sent` —
 * только если явно разрешено) либо до 404 (запись вытеснена — тоже терминально).
 *
 * Для команд, ждущих ответа SSVC, `sent` не считается финальным: прошивка ставит
 * его сразу после отправки в UART, а `done` — когда пришёл ответ. Поэтому по
 * умолчанию ждём `done`; если ответа так и нет — таймаут, и в ответе остаётся `sent`.
 *
 * Таймаут — НЕ ошибка: команда уже принята, возвращаем последнее наблюдение
 * с `timed_out:true`, чтобы клиент сам решил, ждать дальше или нет.
 */
export async function waitForCommandTerminal(
  client: DeviceClient,
  commandId: number,
  options: WaitForCommandTerminalOptions = {},
): Promise<CommandJournalWait> {
  const now = options.now ?? Date.now;
  const sleep = options.sleep ?? defaultSleep;
  const timeoutMs = Math.max(0, options.timeoutMs ?? 15_000);
  const pollMs = Math.max(1, options.pollMs ?? 300);
  const sentIsTerminal = options.sentIsTerminal === true;
  const finished = (view: CommandJournalView): boolean =>
    view.terminal && (view.state !== 'sent' || sentIsTerminal);
  const started = now();

  let last = await getCommandStatus(client, commandId);
  let polls = 1;

  while (!finished(last)) {
    const remaining = timeoutMs - (now() - started);
    if (remaining <= 0) break;
    await sleep(Math.min(pollMs, remaining));
    last = await getCommandStatus(client, commandId);
    polls += 1;
  }

  return {
    ...last,
    timed_out: !finished(last),
    elapsed_s: roundSeconds(now() - started),
    polls,
  };
}

// ---------------------------------------------------------------------------
// Ожидание изменения состояния
// ---------------------------------------------------------------------------

/** Ожидаемое значение: строка или набор допустимых строк (любая из). */
export type ExpectedValue = string | readonly string[];

export interface ChangeExpectation {
  stage?: ExpectedValue;
  status?: ExpectedValue;
  telemetry_type?: ExpectedValue;
  uart_error?: boolean;
}

export interface ObservedChange {
  stage: string | null;
  status: string | null;
  telemetry_type: string | null;
  uart_error: boolean;
  observed_at: number | null;
  last_update_ms: number | null;
  stale: boolean | null;
  partial: string[];
}

export interface WaitForChangeOptions {
  /** Общий таймаут ожидания, мс (clamp 1000..600000). По умолчанию 120 000. */
  timeoutMs?: number;
  /** Интервал опроса, мс (clamp 250..5000). По умолчанию 1000. */
  pollMs?: number;
  now?: () => number;
  sleep?: (ms: number) => Promise<void>;
}

export interface WaitForChangeResult {
  matched: boolean;
  timeout: boolean;
  elapsed_s: number;
  polls: number;
  uart_error: boolean;
  observed: ObservedChange;
  expected: ChangeExpectation;
  matched_fields: string[];
  mismatched_fields: string[];
  notice?: string;
}

const MIN_TIMEOUT_MS = 1_000;
const MAX_TIMEOUT_MS = 600_000;
const MIN_POLL_MS = 250;
const MAX_POLL_MS = 5_000;

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

function expectedStrings(value: ExpectedValue): string[] {
  return Array.isArray(value) ? value.map((item) => String(item)) : [String(value)];
}

function matchesText(actual: string | null, expected: ExpectedValue): boolean {
  return actual !== null && expectedStrings(expected).includes(actual);
}

function observeState(state: StateView): ObservedChange {
  const rawType = state.telemetry?.['type'];
  const telemetryType =
    rawType === null || rawType === undefined
      ? null
      : typeof rawType === 'string'
        ? rawType
        : String(rawType);
  return {
    stage: state.status.stage,
    status: state.status.status,
    telemetry_type: telemetryType,
    uart_error: state.status.uart_error,
    observed_at: state.observed_at,
    last_update_ms: state.last_update_ms,
    stale: state.stale,
    partial: [...state.partial],
  };
}

interface ExpectationCheck {
  matched: boolean;
  matchedFields: string[];
  mismatchedFields: string[];
}

function checkExpectation(observed: ObservedChange, expect: ChangeExpectation): ExpectationCheck {
  const matchedFields: string[] = [];
  const mismatchedFields: string[] = [];

  const check = (field: string, ok: boolean): void => {
    if (ok) matchedFields.push(field);
    else mismatchedFields.push(field);
  };

  if (expect.stage !== undefined) check('stage', matchesText(observed.stage, expect.stage));
  if (expect.status !== undefined) check('status', matchesText(observed.status, expect.status));
  if (expect.telemetry_type !== undefined) {
    check('telemetry_type', matchesText(observed.telemetry_type, expect.telemetry_type));
  }
  if (expect.uart_error !== undefined) {
    check('uart_error', observed.uart_error === expect.uart_error);
  }

  return {
    matched: mismatchedFields.length === 0 && matchedFields.length > 0,
    matchedFields,
    mismatchedFields,
  };
}

/**
 * Опрашивает снимок состояния (`/rest/oc/snapshot`) до совпадения ожидаемых
 * полей по подмножеству либо до таймаута.
 *
 *  - первое наблюдение — сразу, без паузы;
 *  - таймаут — `matched:false`, `timeout:true`, последний `observed` (не ошибка);
 *  - потеря UART (`uart_error:true`) — `matched:false` с явной пометкой:
 *    данные состояния недостоверны, ждать по ним бессмысленно;
 *  - `poll_ms` зажимается в 250..5000, `timeout_s` — в 1..600.
 *
 * Ожидание ничего не меняет в контроллере.
 */
export async function waitForChange(
  client: DeviceClient,
  expect: ChangeExpectation,
  options: WaitForChangeOptions = {},
): Promise<WaitForChangeResult> {
  const expectationKeys = Object.keys(expect).filter(
    (key) => (expect as Record<string, unknown>)[key] !== undefined,
  );
  if (expectationKeys.length === 0) {
    throw new OcError('validation_failed', 'Не указано ни одного ожидаемого поля', {
      hint:
        'Передайте expect хотя бы с одним полем: stage, status, telemetry_type или uart_error. ' +
        'Сравнение идёт по подмножеству.',
      details: { reason: 'invalid_argument', allowed: ['stage', 'status', 'telemetry_type', 'uart_error'] },
    });
  }

  const now = options.now ?? Date.now;
  const sleep = options.sleep ?? defaultSleep;
  const timeoutMs = clamp(options.timeoutMs ?? 120_000, MIN_TIMEOUT_MS, MAX_TIMEOUT_MS);
  const pollMs = clamp(options.pollMs ?? 1_000, MIN_POLL_MS, MAX_POLL_MS);
  const started = now();

  let polls = 0;
  let observed: ObservedChange | null = null;
  let check: ExpectationCheck = { matched: false, matchedFields: [], mismatchedFields: [] };

  for (;;) {
    const snapshot = await fetchSnapshot(client);
    const state = buildStateView(snapshot, stalenessTrackerFor(client));
    observed = observeState(state);
    polls += 1;
    check = checkExpectation(observed, expect);

    // Потеря UART делает состояние недостоверным: не выдаём ложное ожидание.
    // Исключение — вызывающий явно ждёт `uart_error:false`: это и есть ожидание
    // восстановления связи, поэтому продолжаем опрос, а не выходим сразу.
    if (observed.uart_error && expect.uart_error === undefined) {
      return {
        matched: false,
        timeout: false,
        elapsed_s: roundSeconds(now() - started),
        polls,
        uart_error: true,
        observed,
        expected: expect,
        matched_fields: check.matchedFields,
        mismatched_fields: check.mismatchedFields,
        notice:
          'Контроллер потерял связь с SSVC по UART (uart_error:true): данные состояния ' +
          'недостоверны, ожидание остановлено. Проверьте кабель и питание контроллера.',
      };
    }

    if (check.matched) {
      return {
        matched: true,
        timeout: false,
        elapsed_s: roundSeconds(now() - started),
        polls,
        uart_error: observed.uart_error,
        observed,
        expected: expect,
        matched_fields: check.matchedFields,
        mismatched_fields: check.mismatchedFields,
      };
    }

    const remaining = timeoutMs - (now() - started);
    if (remaining <= 0) break;
    await sleep(Math.min(pollMs, remaining));
  }

  // observed гарантированно не null: цикл минимум один раз выполнил снимок.
  const last = observed as ObservedChange;
  return {
    matched: false,
    timeout: true,
    elapsed_s: roundSeconds(now() - started),
    polls,
    uart_error: last.uart_error,
    observed: last,
    expected: expect,
    matched_fields: check.matchedFields,
    mismatched_fields: check.mismatchedFields,
    notice: `Ожидаемые поля не совпали за ${roundSeconds(timeoutMs)} с: состояние не изменилось.`,
  };
}
