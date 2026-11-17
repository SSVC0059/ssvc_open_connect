/**
 * Кэш возможностей устройства (контракт §3.8): `GET /rest/oc/info` и `GET /rest/features`.
 *
 * TTL 60 с — компромисс: реестр возможностей меняется только с прошивкой или перезагрузкой,
 * но после `oc_restart`/`409 feature_unsupported` кэш нужно сбросить (`invalidate()`).
 */

import type { Logger } from '../logging.js';
import type { DeviceMeta } from '../mcp/types.js';
import type { DeviceClient } from './DeviceClient.js';
import { featureFlagsSchema, parseJsonBody } from './schemas/common.js';
import {
  infoApiCode,
  infoCompatibility,
  ocInfoSchema,
  unavailableFeatures,
  type OcInfo,
} from './schemas/info.js';
import { OcError } from '../errors.js';

export const CAPABILITY_TTL_MS = 60_000;

export interface CapabilityCacheOptions {
  ttlMs?: number;
  now?: () => number;
  log?: Logger;
}

export class CapabilityCache {
  private readonly client: DeviceClient;
  private readonly ttlMs: number;
  private readonly now: () => number;
  private readonly log?: Logger;

  private info: OcInfo | null = null;
  private infoAt: number | null = null;
  private inflight: Promise<OcInfo> | null = null;
  private featuresValue: Record<string, boolean> | null = null;
  private featuresAt: number | null = null;

  constructor(client: DeviceClient, opts: CapabilityCacheOptions = {}) {
    this.client = client;
    this.ttlMs = opts.ttlMs ?? CAPABILITY_TTL_MS;
    this.now = opts.now ?? Date.now;
    this.log = opts.log;
  }

  /** Снимок `/rest/oc/info` (из кэша, если не истёк TTL). */
  async get(force = false): Promise<OcInfo> {
    if (!force && this.info !== null && this.infoAt !== null && this.now() - this.infoAt < this.ttlMs) {
      return this.info;
    }
    if (this.inflight !== null) return this.inflight;
    this.inflight = this.client
      .get('/rest/oc/info', ocInfoSchema)
      .then((info) => {
        this.info = info;
        this.infoAt = this.now();
        return info;
      })
      .finally(() => {
        this.inflight = null;
      });
    return this.inflight;
  }

  /** Значение из кэша без сетевого вызова (для диагностики). */
  peek(): OcInfo | null {
    return this.info;
  }

  /** Возраст кэша в миллисекундах; null — данных ещё не было. */
  ageMs(): number | null {
    return this.infoAt === null ? null : Math.max(0, this.now() - this.infoAt);
  }

  /** Сброс кэша: после рестарта контроллера или `feature_unsupported`. */
  invalidate(): void {
    this.info = null;
    this.infoAt = null;
    this.featuresValue = null;
    this.featuresAt = null;
  }

  /** Паспорт устройства для ответов инструментов (без пароля и токена). */
  async meta(force = false): Promise<DeviceMeta> {
    const info = await this.get(force);
    return {
      url: this.client.deviceUrl,
      firmware: info.oc.version,
      ssvcVersion: info.ssvc.version,
      api: info.ssvc.api,
      apiCode: infoApiCode(info),
      compatibility: infoCompatibility(info),
    };
  }

  /** Имена возможностей, недоступных на этом устройстве. */
  async unavailable(force = false): Promise<string[]> {
    return unavailableFeatures(await this.get(force));
  }

  /**
   * Рантайм-флаги прошивки `GET /rest/features` (карта bool).
   * Кэшируются вместе с info: один TTL на оба запроса.
   */
  async features(force = false): Promise<Record<string, boolean>> {
    if (
      !force &&
      this.featuresValue !== null &&
      this.featuresAt !== null &&
      this.now() - this.featuresAt < this.ttlMs
    ) {
      return this.featuresValue;
    }
    const raw = await this.client.request('GET', '/rest/features', undefined, {
      headers: { accept: 'application/json' },
    });
    if (raw.status < 200 || raw.status >= 300) {
      throw new OcError('device_error', 'Не удалось получить рантайм-флаги прошивки', {
        hint: 'Проверьте доступность контроллера и повторите запрос.',
        details: { status: raw.status, body: raw.body.slice(0, 2_000) },
      });
    }
    const parsed = parseJsonBody(raw.body);
    if (!parsed.ok) {
      throw new OcError('device_error', 'Ответ /rest/features не является JSON', {
        hint: 'Проверьте версию прошивки контроллера.',
        details: { body: raw.body.slice(0, 2_000) },
      });
    }
    const result = featureFlagsSchema.safeParse(parsed.value);
    if (!result.success) {
      throw new OcError('device_error', 'Ответ /rest/features не соответствует ожидаемой схеме', {
        hint: 'Проверьте версию прошивки контроллера.',
        details: {
          issues: result.error.issues.map((issue) => issue.path.join('.')),
          body: raw.body.slice(0, 2_000),
        },
      });
    }
    this.featuresValue = result.data;
    this.featuresAt = this.now();
    this.log?.debug('Рантайм-флаги прошивки обновлены', { count: Object.keys(result.data).length });
    return result.data;
  }
}
