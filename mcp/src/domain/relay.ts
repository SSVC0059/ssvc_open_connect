/**
 * Доменный слой «реле на PCF8574» (WP5, mcp-server-agents.md §9).
 *
 * Контракт устройства (mcp-contract-notes.md §B.13, §C.6, §D.5):
 *  - `GET /rest/oc/relay/metadata` → `{supported, relays[], relayChipCount, relayLineCount, …}`;
 *  - `GET /rest/oc/relay/state`    → `{supported, userShadow, userShadows[]}`;
 *  - `POST /rest/oc/relay/override` с телом `{bit, enable, energized}` → `{ok:true}`;
 *  - на GPIO-сборке все GET отвечают `200 {"supported":false}` — это не сбой.
 *
 * Что определяет реализацию:
 *
 * 1. `supported:false` — не ошибка устройства, а «функция недоступна в этой сборке
 *    прошивки» (mcp-server.md §2.5 п.9). Для metadata/state/override одинаково:
 *    `feature_unsupported` с одним и тем же русским сообщением.
 * 2. `bit` в `override` валидируется ЛОКАЛЬНО по `relayLineCount` из metadata, чтобы
 *    заведомо неверный номер не уходил POST-ом. Номер реле (`index`) — 1-based, а
 *    `bit`/`targetBit` — 0-based глобальный: путать их нельзя (§B.13, §C.6).
 * 3. `enable` и `energized` инструмент требует явно, хотя прошивка считает их
 *    необязательными (`enable` по умолчанию true): молчаливое «включить» — это
 *    переключение реле, которого агент не просил (mcp-server.md §2.5 п.8).
 * 4. `403 {"error":"Relay is reserved for AlarmManager"}` — это не «нужен admin»
 *    (транспорт иначе отдал бы `forbidden_admin_required`), а запрет на системные
 *    реле аварий: `validation_failed` с `details.bit`. Прочие 403 идут обычным путём.
 * 5. Кэш metadata: `bit` проверяется по line_count, а metadata — это справочник,
 *    который меняется только с прошивкой/перезагрузкой. Держим его 60 с (тот же TTL,
 *    что у `CapabilityCache`) в WeakMap по клиенту устройства: без нового глобального
 *    состояния, без утечки между тестами и без лишнего GET на каждый override.
 *    Сам инструмент `oc_get_relay_metadata` читает принудительно (force), чтобы
 *    агент видел актуальные подписи, а не снимок минутной давности.
 * 6. SPA-заглушка (`200 text/html`) — `feature_unsupported`, а не пустые реле:
 *    разбор общий с hardware-доменом в `readJson.ts`.
 *
 * Запись идёт через `client.withWrite('relay', …)`: POST не ретраится транспортом,
 * а последовательность важна для журнала. Риск 2 (`confirm`) проверяет инструмент.
 */

import type { DeviceClient } from '../device/DeviceClient.js';
import {
  relayMetadataSchema,
  relayOverrideResponseSchema,
  relayStateSchema,
  type RelayMetadataRaw,
} from '../device/schemas/relay.js';
import { parseJsonBody } from '../device/schemas/common.js';
import { OcError, httpError } from '../errors.js';
import { readJson } from './readJson.js';

export const RELAY_METADATA_PATH = '/rest/oc/relay/metadata';
export const RELAY_STATE_PATH = '/rest/oc/relay/state';
export const RELAY_OVERRIDE_PATH = '/rest/oc/relay/override';

/** Ключ очереди записей домена: ручные переключения не должны перемешиваться. */
const WRITE_KEY = 'relay';

/**
 * TTL справочника реле. Тот же, что у `CapabilityCache` (60 с): metadata меняется
 * только с прошивкой или после перезагрузки контроллера.
 */
export const RELAY_METADATA_TTL_MS = 60_000;

/** Сообщение для GPIO-сборки — дословно по §9 и одинаковое для всех трёх инструментов. */
export const RELAY_UNSUPPORTED_MESSAGE = 'Реле на PCF8574 недоступны в этой сборке прошивки';

// ---------------------------------------------------------------------------
// Справочник реле (metadata)
// ---------------------------------------------------------------------------

export interface RelayInfo {
  /** Номер реле для человека: 1-based (`index` прошивки). */
  index: number;
  /** Подпись прошивки, например «Реле 3». */
  label: string;
  /** false — системное реле AlarmManager, ручное управление запрещено. */
  editable: boolean;
  /** 0-based глобальный бит: именно его принимает override. */
  target_bit: number;
  /** Причина запрета (для `editable:false`), иначе null. */
  reason: string | null;
}

export interface RelayMetadataSnapshot {
  supported: true;
  relays: RelayInfo[];
  /** Число чипов расширителя; null, если прошивка не прислала. */
  relay_chip_count: number | null;
  /** Число линий (битов) во всех чипах; null, если прошивка не прислала. */
  relay_line_count: number | null;
  observed_at: string;
}

interface MetadataCacheEntry {
  value: RelayMetadataSnapshot;
  at: number;
}

/** Кэш справочника по клиенту устройства: у каждого контроллера свой снимок. */
const metadataCache = new WeakMap<DeviceClient, MetadataCacheEntry>();

function unsupportedRelayError(): OcError {
  return new OcError('feature_unsupported', RELAY_UNSUPPORTED_MESSAGE, {
    hint:
      'Сборка прошивки с GPIO-реле не поддерживает расширитель PCF8574: чтение состояния и ' +
      'ручное управление реле недоступны. Это ограничение сборки, а не сбой устройства; ' +
      'повтор запроса не поможет.',
    details: { supported: false },
  });
}

function normalizeMetadata(raw: RelayMetadataRaw): RelayMetadataSnapshot {
  const relays: RelayInfo[] = (raw.relays ?? [])
    .map((entry) => ({
      index: entry.index,
      label: entry.label,
      editable: entry.editable,
      target_bit: entry.targetBit,
      reason: entry.reason ?? null,
    }))
    .sort((left, right) => left.target_bit - right.target_bit || left.index - right.index);

  return {
    supported: true,
    relays,
    relay_chip_count: raw.relayChipCount ?? null,
    relay_line_count: raw.relayLineCount ?? null,
    observed_at: new Date().toISOString(),
  };
}

/**
 * Читает метаданные реле. При `force:false` возвращает снимок не старше 60 с.
 *
 * @throws OcError `feature_unsupported` на GPIO-сборке, коды транспорта/разбора.
 */
export async function getRelayMetadata(
  client: DeviceClient,
  opts: { force?: boolean } = {},
): Promise<RelayMetadataSnapshot> {
  if (opts.force !== true) {
    const cached = metadataCache.get(client);
    if (cached !== undefined && Date.now() - cached.at < RELAY_METADATA_TTL_MS) {
      return cached.value;
    }
  }

  const raw = await readJson(client, RELAY_METADATA_PATH, relayMetadataSchema);
  if (!raw.supported) throw unsupportedRelayError();

  const snapshot = normalizeMetadata(raw);
  metadataCache.set(client, { value: snapshot, at: Date.now() });
  return snapshot;
}

// ---------------------------------------------------------------------------
// Состояние реле (state)
// ---------------------------------------------------------------------------

export interface RelayStateSnapshot {
  supported: true;
  /** Битовая маска первого расширителя (активный низкий уровень: включено = 0). */
  user_shadow: number | null;
  /** Маска по каждому расширителю. */
  user_shadows: number[];
  observed_at: string;
}

/** Читает текущее состояние линий реле. */
export async function getRelayState(client: DeviceClient): Promise<RelayStateSnapshot> {
  const raw = await readJson(client, RELAY_STATE_PATH, relayStateSchema);
  if (!raw.supported) throw unsupportedRelayError();

  return {
    supported: true,
    user_shadow: raw.userShadow ?? null,
    user_shadows: raw.userShadows ?? [],
    observed_at: new Date().toISOString(),
  };
}

// ---------------------------------------------------------------------------
// Ручное управление (override)
// ---------------------------------------------------------------------------

export interface RelayOverrideInput {
  bit: unknown;
  enable: unknown;
  energized: unknown;
}

export interface RelayOverrideResult {
  ok: true;
  bit: number;
  enable: boolean;
  energized: boolean;
}

/** Номер бита: целое ≥ 0. Проверка локальная — опечатка не должна уходить POST-ом. */
export function normalizeRelayBit(value: unknown): number {
  if (typeof value !== 'number' || !Number.isInteger(value) || value < 0) {
    throw new OcError('validation_failed', 'Номер бита реле должен быть целым числом не меньше нуля', {
      hint:
        'Передайте bit — 0-based глобальный бит из oc_get_relay_metadata (поле target_bit). ' +
        'Не путайте с номером реле index: он 1-based. Проверка локальная: запрос к ' +
        'контроллеру не отправлялся.',
      details: {
        field: 'bit',
        reason: 'invalid_bit',
        value: typeof value === 'number' ? value : String(value),
      },
    });
  }
  return value;
}

/** `enable`/`energized` обязательны: полагаться на дефолт прошивки нельзя (§2.5 п.8). */
export function requireRelayBoolean(value: unknown, field: 'enable' | 'energized'): boolean {
  if (typeof value !== 'boolean') {
    throw new OcError(
      'validation_failed',
      `Поле ${field} обязательно и должно быть boolean`,
      {
        hint:
          'Передайте оба поля явно: enable — включать ли ручное управление битом, ' +
          'energized — требуемое состояние реле (true — включено, false — выключено). ' +
          'У прошивки enable по умолчанию true, поэтому пропуск поля включит реле.',
        details: {
          field,
          reason: 'missing_or_invalid_boolean',
          value: value === undefined ? 'undefined' : String(value),
        },
      },
    );
  }
  return value;
}

/**
 * Включает/выключает одно реле вручную.
 *
 * Порядок: проверка аргументов → metadata (кэш ≤60 с, нужен line_count и supported) →
 * локальная проверка диапазона → POST через очередь записей. Никаких ретраев:
 * POST транспортом не повторяется, а повторное переключение реле — не идемпотентно
 * с точки зрения процесса.
 *
 * @throws OcError `validation_failed` (аргументы, диапазон, 400, 403 резерва),
 *         `feature_unsupported` (GPIO-сборка), коды транспорта.
 */
export async function setRelayOverride(
  client: DeviceClient,
  input: RelayOverrideInput,
): Promise<RelayOverrideResult> {
  const bit = normalizeRelayBit(input.bit);
  const enable = requireRelayBoolean(input.enable, 'enable');
  const energized = requireRelayBoolean(input.energized, 'energized');

  const metadata = await getRelayMetadata(client);
  if (metadata.relay_line_count !== null && bit >= metadata.relay_line_count) {
    throw new OcError('validation_failed', 'Бит реле вне диапазона линий расширителя', {
      hint:
        `В этой сборке ${metadata.relay_line_count} линий реле (relay_line_count), допустимы ` +
        `биты 0…${metadata.relay_line_count - 1}. Возьмите target_bit из oc_get_relay_metadata. ` +
        'Запрос к контроллеру не отправлялся.',
      details: {
        field: 'bit',
        reason: 'bit_out_of_range',
        bit,
        relay_line_count: metadata.relay_line_count,
      },
    });
  }

  const body = { bit, enable, energized };

  await client.withWrite(WRITE_KEY, async () => {
    const response = await client.request('POST', RELAY_OVERRIDE_PATH, body, {
      headers: { accept: 'application/json' },
    });

    if (response.status >= 200 && response.status < 300) {
      assertOverrideConfirmed(response);
      return;
    }
    if (response.status === 403) {
      if (isReservedRelayBody(response.body)) throw reservedRelayError(bit, response);
      throw httpError({
        status: response.status,
        body: response.body,
        method: 'POST',
        path: RELAY_OVERRIDE_PATH,
      });
    }
    if (response.status === 400) throw invalidOverrideError(bit, response);
    throw httpError({
      status: response.status,
      body: response.body,
      method: 'POST',
      path: RELAY_OVERRIDE_PATH,
    });
  });

  return { ok: true, bit, enable, energized };
}

/** Успех подтверждаем телом `{ok:true}`, если прошивка его прислала (пустое тело допустимо). */
function assertOverrideConfirmed(response: { body: string; status: number }): void {
  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) return;
  const result = relayOverrideResponseSchema.safeParse(parsed.value);
  if (!result.success) {
    throw new OcError('device_error', 'Контроллер вернул неожиданный ответ на переключение реле', {
      hint: 'Расхождение контракта с прошивкой: проверьте версию контроллера и details.body.',
      details: { status: response.status, body: response.body.slice(0, 2_000) },
    });
  }
  if (!result.data.ok) {
    throw new OcError('device_error', 'Контроллер не подтвердил переключение реле', {
      hint:
        'Прошивка ответила успешным статусом, но ok:false. Перечитайте oc_get_relay_state и ' +
        'проверьте журнал контроллера.',
      details: { status: response.status, body: response.body.slice(0, 2_000) },
    });
  }
}

/** Резерв под AlarmManager: строка `"Relay is reserved for AlarmManager"` (§D.5). */
function isReservedRelayBody(body: string): boolean {
  const parsed = parseJsonBody(body);
  if (!parsed.ok || typeof parsed.value !== 'object' || parsed.value === null) return false;
  const error = (parsed.value as { error?: unknown }).error;
  const text = typeof error === 'string' ? error : '';
  return /reserved/i.test(text) || /AlarmManager/i.test(text);
}

function reservedRelayError(
  bit: number,
  response: { status: number; body: string },
): OcError {
  return new OcError('validation_failed', 'Реле зарезервировано под аварийную сигнализацию', {
    hint:
      'Системные реле AlarmManager (первые две линии первого расширителя) нельзя переключать ' +
      'вручную. Выберите реле с editable:true в oc_get_relay_metadata — это не отказ в правах ' +
      'и не требует учётной записи admin.',
    details: {
      bit,
      status: response.status,
      method: 'POST',
      path: RELAY_OVERRIDE_PATH,
      body: response.body.slice(0, 2_000),
    },
  });
}

/** 400 override: `Missing bit` / `Invalid bit` / `bit out of range` (§D.5). */
function invalidOverrideError(
  bit: number,
  response: { status: number; body: string },
): OcError {
  const parsed = parseJsonBody(response.body);
  const deviceError =
    parsed.ok && typeof parsed.value === 'object' && parsed.value !== null &&
    typeof (parsed.value as { error?: unknown }).error === 'string'
      ? (parsed.value as { error: string }).error
      : null;

  return new OcError(
    'validation_failed',
    deviceError === null
      ? 'Контроллер отклонил ручное управление реле'
      : `Контроллер отклонил ручное управление реле: ${deviceError}`,
    {
      hint:
        'Проверьте bit: это 0-based глобальный бит из oc_get_relay_metadata (target_bit), ' +
        'он должен быть меньше relay_line_count. enable и energized передавайте явно. ' +
        'При 400 прошивка реле не переключала.',
      details: {
        status: response.status,
        method: 'POST',
        path: RELAY_OVERRIDE_PATH,
        bit,
        device_error: deviceError,
        body: response.body.slice(0, 2_000),
      },
    },
  );
}


