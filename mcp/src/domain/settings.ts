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
import { bodyForDetails, headerValue, parseJsonBody } from '../device/schemas/common.js';
import {
  settingsEnvelopeSchema,
  settingsWriteResponseSchema,
  type SsvcSettings,
  type SettingsWriteResponseBody,
} from '../device/schemas/settings.js';
import { OcError, httpError } from '../errors.js';
import { readJson } from './readJson.js';

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
 * веб-страницей (mcp-server.md §2.5 п.16). Разбор общий — `readJson.ts`: домен
 * задаёт только русские тексты, поведение (404/SPA → `feature_unsupported`,
 * битый JSON/схема → `device_error`) одинаково во всех доменах.
 */
export async function getSettings(client: DeviceClient): Promise<SettingsSnapshot> {
  const envelope = await readJson(client, SETTINGS_PATH, settingsEnvelopeSchema, {
    notFound: {
      message: 'Прошивка не отдаёт настройки SSVC (/rest/settings)',
      hint:
        'Обновите прошивку OpenConnect: маршрут GET/PUT /rest/settings появился в версии с ' +
        'настройками SSVC. Повтор запроса не поможет.',
    },
    schemaMismatch: {
      message: 'Ответ /rest/settings не содержит объект ssvcSettings ожидаемой формы',
      hint:
        'Расхождение контракта с прошивкой: ожидался { "ssvcSettings": { ... } }. ' +
        'Проверьте версию контроллера и тело ответа в details.',
    },
  });

  return {
    settings: envelope.ssvcSettings,
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

/* ──────────────────────── Производные значения и проверки ──────────────────────── */

/**
 * Стадии, у которых есть пара `[время_открытия_с, период_с]`
 * (`docs/ssvc_uart_api_docs.md`, §`heads`, `late_heads`, `hearts`, `tails`).
 */
export type SettingsStage = 'heads' | 'late_heads' | 'hearts' | 'tails';

/**
 * Стадия → индекс `valve_bw`. Порядок повторяет расчёт `@ssvc/rectification`
 * (`packages/rectification/src/index.js`, блок «Клапан…`): головы — 1-й клапан,
 * тело — 2-й, подголовники и хвосты — 3-й. Держим один порядок на весь проект,
 * иначе `derived.flow_mlh` разойдётся с `oc_calculate_plan`.
 */
const STAGE_VALVE_INDEX: Record<SettingsStage, number> = {
  heads: 0,
  late_heads: 2,
  hearts: 1,
  tails: 2,
};

const STAGES: readonly SettingsStage[] = ['heads', 'late_heads', 'hearts', 'tails'];

/** Стадии, у которых таймер задаёт объём: остальные живут до температуры/процента. */
const STAGE_TIMER_KEY: Partial<Record<SettingsStage, string>> = {
  heads: 'heads_timer',
  late_heads: 'late_heads_timer',
};

export interface StageFlow {
  /** Расход стадии в мл/ч при текущих `valve_bw` и паре. */
  flow_mlh: number;
  /** Скважность: доля периода, когда клапан открыт, %. */
  duty_pct: number;
  period_s: number;
  /** Поле присутствует, но эта ветка прошивки его не использует (хвосты в 2.3). */
  ignored?: true;
}

export interface SettingWarning {
  code: string;
  field: string;
  hint: string;
}

/**
 * Метод пересчёта. Едет вместе с числами, а не живёт только в коде: иначе агент,
 * изолированный от репозитория, объясняет `flow_mlh` придуманной формулой
 * (в одном прогоне так появился лишний множитель ×60).
 */
export const FLOW_METHOD = 'flow_mlh = valve_bw[i] * opening_s / period_s';

/** Единицы производных значений: `valve_bw` — мл/ч, пара — секунды. */
export const DERIVED_UNITS: Readonly<Record<string, string>> = {
  flow_mlh: 'мл/ч',
  duty_pct: '%',
  period_s: 'с',
  volume_ml: 'мл',
};

export interface DerivedSettings {
  /** Формула, которой получены `flow_mlh`. */
  method: string;
  /** Единицы полей `derived` и `volumes_ml`. */
  units: Record<string, string>;
  /** Какой клапан (`valve_bw[i]`) использует каждая стадия. */
  valve_index: Record<SettingsStage, number>;
  stages: Record<SettingsStage, StageFlow | null>;
  volumes_ml: Partial<Record<SettingsStage, number>>;
  warnings: SettingWarning[];
}

/**
 * Считает производные значения из уже полученных настроек: мл/ч на стадию,
 * скважность, объёмы по таймерам и предупреждения. Обращений к устройству нет —
 * функция чистая и используется и чтением, и записью.
 */
export function analyzeSettings(settings: SsvcSettings): DerivedSettings {
  const warnings: SettingWarning[] = [];
  const valves = readValveBw(settings);
  const hasLateHeads = isPair(settings.late_heads);
  const isLateHeadsBranch = hasLateHeads;

  if (valves === null) {
    warnings.push({
      code: 'valve_bw_missing',
      field: 'valve_bw',
      hint:
        'Пропускные способности клапанов не заданы или неполные: мл/ч не определить. ' +
        'Задайте valve_bw=[<головы>,<тело>,<хвосты>] в мл/ч (калибровка клапанов).',
    });
  }

  const stages = {} as Record<SettingsStage, StageFlow | null>;
  const volumes: Partial<Record<SettingsStage, number>> = {};

  for (const stage of STAGES) {
    const pair = readPair(settings[stage]);
    if (pair === null) {
      stages[stage] = null;
      continue;
    }
    const ignored = stage === 'tails' && isLateHeadsBranch;
    const duty = (pair.opening / pair.period) * 100;
    const valve = valves === null ? null : valves[STAGE_VALVE_INDEX[stage]];
    const flow =
      valve === null || valve === undefined || valve <= 0
        ? null
        : (valve * pair.opening) / pair.period;

    if (valve !== null && valve !== undefined && valve <= 0) {
      warnings.push({
        code: 'valve_bw_zero',
        field: 'valve_bw',
        hint:
          'Пропускная способность клапана этой стадии равна 0: скорость и объём будут нулевыми. ' +
          'Проверьте калибровку valve_bw.',
      });
    }

    stages[stage] = {
      flow_mlh: flow === null ? 0 : round(flow, 1),
      duty_pct: round(duty, 2),
      period_s: pair.period,
      ...(ignored ? { ignored: true as const } : {}),
    };

    const timerKey = STAGE_TIMER_KEY[stage];
    const timer = timerKey === undefined ? null : readNumber(settings[timerKey]);
    if (flow !== null && timer !== null && timer > 0) {
      volumes[stage] = Math.round((flow * timer) / 3600);
    }
  }

  if (isLateHeadsBranch && 'tails' in settings) {
    warnings.push({
      code: 'field_ignored_by_firmware',
      field: 'tails',
      hint:
        'Этап хвостов есть только в прошивке 2.2.*; на 2.3 (есть late_heads) поле tails ' +
        'не используется.',
    });
  }

  // Совместность проверяем здесь же: чтение и запись должны показывать один набор предупреждений.
  for (const warning of lintSettings(settings)) {
    const duplicate = warnings.some(
      (entry) => entry.code === warning.code && entry.field === warning.field,
    );
    if (!duplicate) warnings.push(warning);
  }

  return {
    method: FLOW_METHOD,
    units: { ...DERIVED_UNITS },
    valve_index: { ...STAGE_VALVE_INDEX },
    stages,
    volumes_ml: volumes,
    warnings,
  };
}

/**
 * Проверяет совместность настроек: формально валидная комбинация может быть
 * неработающей (например `predec` без достаточного `hyst` прошивка отключает молча).
 *
 * Правила — руководство изготовителя и `mcp-scenarios.md` §6 (D1/D2/D3).
 */
export function lintSettings(settings: SsvcSettings): SettingWarning[] {
  const warnings: SettingWarning[] = [];
  const hyst = readNumber(settings.hyst);
  const predec = readNumber(settings.predec);
  const formula = readNumber(settings.formula);
  const extendedHeads = readNumber(settings.extended_heads);

  if (predec === 13 && hyst !== null && hyst < 0.18) {
    warnings.push({
      code: 'predec_hyst_incompatible',
      field: 'predec',
      hint:
        'predec=13 требует hyst ≥ 0.18: иначе прошивка отключит предекремент и раннего ' +
        'снижения скорости не будет. Поднимите hyst или уменьшите predec.',
    });
  } else if (predec === 7 && hyst !== null && hyst < 0.12) {
    warnings.push({
      code: 'predec_hyst_incompatible',
      field: 'predec',
      hint:
        'predec=7 требует hyst ≥ 0.12: иначе прошивка отключит предекремент. ' +
        'Поднимите hyst или уменьшите predec.',
    });
  } else if (predec !== null && predec > 0 && hyst === null) {
    warnings.push({
      code: 'predec_without_hyst',
      field: 'predec',
      hint: 'Предекремент включён, но hyst не задан: условие «hyst ≥ predec» не проверить.',
    });
  }

  if (formula === 2 && (predec === null || predec <= 0)) {
    warnings.push({
      code: 'formula_without_predec',
      field: 'formula',
      hint:
        '«Формула авто 92+» опирается на срабатывание предекремента: без predec она ' +
        'не заработает так, как задумано.',
    });
  }

  const releaseTimer = readNumber(settings.release_timer);
  const headsFinal = readNumber(settings.heads_final);
  if (
    extendedHeads === 1 &&
    (releaseTimer === null || releaseTimer <= 0) &&
    (headsFinal === null || headsFinal <= 0)
  ) {
    warnings.push({
      code: 'extended_heads_no_params',
      field: 'extended_heads',
      hint:
        '«Сброс и снижение» включён, но release_timer и heads_final нулевые: поведение ' +
        'этапа голов не определено. Задайте параметры сброса или выключите опцию.',
    });
  }

  if (hyst !== null && (hyst < 0.06 || hyst > 2.0)) {
    warnings.push({
      code: 'hyst_out_of_range',
      field: 'hyst',
      hint:
        'Гистерезис вне рекомендуемого руководством 2.3.x диапазона 0.06…2.0 °C: ' +
        'поведение термостата может быть нестабильным.',
    });
  }

  const finishTemp = readNumber(settings.hearts_finish_temp);
  if (finishTemp !== null && finishTemp > 0) {
    warnings.push({
      code: 'hearts_finish_temp_unverified',
      field: 'hearts_finish_temp',
      hint:
        'Отсечка тела по температуре ТД2: убедитесь, что датчик ТД2 стоит в кубе и реально ' +
        'достигает этой температуры — иначе отбор тела не завершится сам.',
    });
  }

  return warnings;
}

/** Разбор `[время_открытия, период]`; `null`, если форма не та или период некорректен. */
function readPair(value: unknown): { opening: number; period: number } | null {
  if (!Array.isArray(value) || value.length < 2) return null;
  const opening = readNumber(value[0]);
  const period = readNumber(value[1]);
  if (opening === null || period === null || period <= 0) return null;
  return { opening, period };
}

function isPair(value: unknown): boolean {
  return readPair(value) !== null;
}

function readValveBw(settings: SsvcSettings): number[] | null {
  const raw = settings.valve_bw;
  if (!Array.isArray(raw) || raw.length < 3) return null;
  const values: number[] = [];
  for (let index = 0; index < 3; index += 1) {
    const value = readNumber(raw[index]);
    if (value === null) return null;
    values.push(value);
  }
  return values;
}

function readNumber(value: unknown): number | null {
  return typeof value === 'number' && Number.isFinite(value) ? value : null;
}

function round(value: number, digits: number): number {
  const factor = 10 ** digits;
  return Math.round(value * factor) / factor;
}
