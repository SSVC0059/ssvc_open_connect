/**
 * Доменный слой «информация и возможности контроллера» (WP1, mcp-server-agents.md §5).
 *
 * Источники:
 *  - `GET /rest/oc/info`   — версии прошивки OpenConnect и UART API SSVC, совместимость,
 *                            реестр возможностей с `available`;
 *  - `GET /rest/features`  — рантайм-флаги сборки (карта bool).
 *
 * Почему `features` читаются здесь, а не через `CapabilityCache.features()`: на живом
 * устройстве (прошивка 0.2.9.2) `/rest/features` отдаёт не только bool, но и строковые
 * метаданные (`firmware_version`, `firmware_name`, …). Строгая схема WP0
 * `featureFlagsSchema = z.record(z.boolean())` на таком ответе падает в `device_error`,
 * поэтому домен разбирает ответ терпимо и раскладывает его на `features` (только bool)
 * и `features_meta` (строки). Схему WP0 не меняем: у неё другие потребители.
 *
 * Ответ наружу — плоская модель `{device, api_features, unavailable}`: агенту не нужно
 * помнить вложенность `ssvc`/`oc` прошивки.
 */

import type { DeviceClient } from '../device/DeviceClient.js';
import { OcError, httpError } from '../errors.js';
import {
  bodyForDetails,
  headerValue,
  isJsonContentType,
  parseJsonBody,
} from '../device/schemas/common.js';
import {
  infoApiCode,
  infoCompatibility,
  unavailableFeatures,
  type OcInfo,
} from '../device/schemas/info.js';

export const INFO_PATH = '/rest/oc/info';
export const FEATURES_PATH = '/rest/features';

/** Плоский паспорт устройства: только то, что нужно агенту для решений. */
export interface DeviceInfoFlat {
  /** Версия прошивки OpenConnect (`oc.version`). */
  firmware: string;
  /** Версия прошивки SSVC (`ssvc.version`). */
  ssvc_version: string;
  /** Версия UART API строкой: «1.7», «1.10». */
  api: string;
  /** Целочисленный код API: major * 100 + minor. */
  api_code: number;
  /** Ветка SSVC: late_heads | tails (null, если прошивка не сообщила). */
  mode: string | null;
  /** Совместимость API: unknown | too_old | supported. */
  compatibility: 'unknown' | 'too_old' | 'supported';
  /** Признак поддержки «Сброса и снижения» (только late_heads). */
  is_support_release: boolean | null;
  /** Минимальная версия API, с которой работает прошивка. */
  api_min: string | null;
  /** Версия API, которую реализует прошивка. */
  api_target: string | null;
}

/** Возможность UART API с признаком доступности на этом контроллере. */
export interface FeatureCapability {
  name: string;
  min_api: string;
  available: boolean;
}

export interface InfoView {
  device: DeviceInfoFlat;
  api_features: FeatureCapability[];
  /** Имена возможностей с `available:false` — короткий список для агента. */
  unavailable: string[];
}

/**
 * Маппинг `/rest/oc/info` в плоскую модель.
 * Недостающие поля прошивки (старый API) остаются `null`, а не выдумываются.
 */
export function mapInfo(info: OcInfo): InfoView {
  const features: FeatureCapability[] = (info.oc.api_features ?? []).map((feature) => ({
    name: feature.name,
    min_api: feature.min_api,
    available: feature.available,
  }));

  return {
    device: {
      firmware: info.oc.version,
      ssvc_version: info.ssvc.version,
      api: info.ssvc.api,
      api_code: infoApiCode(info),
      mode: info.ssvc.mode ?? null,
      compatibility: infoCompatibility(info),
      is_support_release: info.oc.is_support_release ?? null,
      api_min: info.oc.api_min ?? null,
      api_target: info.oc.api_target ?? null,
    },
    api_features: features,
    unavailable: unavailableFeatures(info),
  };
}

/** Рантайм-флаги сборки, разложенные по типу значения. */
export interface RuntimeFeatures {
  /** Только булевы флаги — это и есть «карта возможностей» прошивки. */
  features: Record<string, boolean>;
  /** Строковые значения ответа (версия/имя сборки): это не флаги, но полезно видеть. */
  meta: Record<string, string>;
}

/** Разбирает карту `/rest/features`, не падая на небулевых значениях. */
export function splitRuntimeFeatures(raw: Record<string, unknown>): RuntimeFeatures {
  const features: Record<string, boolean> = {};
  const meta: Record<string, string> = {};
  for (const [key, value] of Object.entries(raw)) {
    if (typeof value === 'boolean') {
      features[key] = value;
    } else if (typeof value === 'string') {
      meta[key] = value;
    }
    // Числа/объекты в этой карте прошивкой не предусмотрены: молча пропускаем,
    // чтобы не выдавать их за флаг.
  }
  return { features, meta };
}

/**
 * Читает `/rest/features`. Ошибки устройства не маскируются:
 *  - сеть/таймаут — из `DeviceClient.request`;
 *  - `404`/SPA-заглушка — `feature_unsupported`: на этой прошивке маршрута нет;
 *  - не JSON/не объект — `device_error` с телом ответа.
 */
export async function loadRuntimeFeatures(client: DeviceClient): Promise<RuntimeFeatures> {
  const response = await client.request('GET', FEATURES_PATH, undefined, {
    headers: { accept: 'application/json' },
  });

  if (response.status === 404) {
    throw new OcError('feature_unsupported', 'Прошивка не отдаёт список функций /rest/features', {
      hint:
        'Обновите прошивку OpenConnect: рантайм-флаги появились в версии с GET /rest/features. ' +
        'Реестр возможностей UART API при этом доступен в oc_get_info.',
      details: { status: response.status, path: FEATURES_PATH },
    });
  }
  if (response.status < 200 || response.status >= 300) {
    throw httpError({ status: response.status, body: response.body, method: 'GET', path: FEATURES_PATH });
  }
  // Неизвестный путь прошивка отдаёт SPA-заглушкой 200 text/html (mcp-server.md §2.5 п.16).
  if (!isJsonContentType(response.headers)) {
    throw new OcError('device_error', 'Ответ /rest/features не является JSON', {
      hint:
        'Похоже, прошивка не знает этого маршрута и вернула веб-страницу. Проверьте версию прошивки.',
      details: {
        status: response.status,
        content_type: headerValue(response.headers, 'content-type') ?? null,
        body: bodyForDetails(response.body),
      },
    });
  }
  const parsed = parseJsonBody(response.body);
  if (!parsed.ok) {
    throw new OcError('device_error', 'Ответ /rest/features не является корректным JSON', {
      hint: 'Проверьте версию прошивки контроллера и повторите запрос.',
      details: { status: response.status, parse_error: parsed.error, body: bodyForDetails(response.body) },
    });
  }
  if (typeof parsed.value !== 'object' || parsed.value === null || Array.isArray(parsed.value)) {
    throw new OcError('device_error', 'Ответ /rest/features имеет неожиданную форму', {
      hint: 'Ожидалась карта «имя флага → bool». Проверьте версию прошивки контроллера.',
      details: { status: response.status, body: bodyForDetails(response.body) },
    });
  }
  return splitRuntimeFeatures(parsed.value as Record<string, unknown>);
}

/** Возможности для `oc_get_capabilities`: реестр API + (опционально) рантайм-флаги. */
export interface CapabilitiesView {
  api_features: FeatureCapability[];
  unavailable: string[];
  /** null — рантайм-флаги не запрашивались (`include_runtime:false`). */
  features: Record<string, boolean> | null;
  features_meta: Record<string, string> | null;
}

export function buildCapabilities(info: OcInfo, runtime: RuntimeFeatures | null): CapabilitiesView {
  const view = mapInfo(info);
  return {
    api_features: view.api_features,
    unavailable: view.unavailable,
    features: runtime === null ? null : runtime.features,
    features_meta: runtime === null ? null : runtime.meta,
  };
}
