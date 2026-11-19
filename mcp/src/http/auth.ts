/**
 * Доступ MCP-клиентов к серверу: Bearer-токен, allowlist адресов, `Host`/`Origin`
 * (контракт §3.4, §3.11 — контур A).
 *
 * Правила:
 *  - сравнение токена — в постоянном времени (сравниваем SHA-256 дайджесты,
 *    чтобы длина токена тоже не утекала по времени ответа);
 *  - без токена ответ `401` + `WWW-Authenticate: Bearer`, иначе клиент не поймёт, что делать;
 *  - чужой адрес/`Host`/`Origin` — `403` (это не «неверный пароль», а отказ в доступе);
 *  - `/healthz` проверяет только сеть (нужен для HEALTHCHECK контейнера без секретов).
 */

import { createHash, timingSafeEqual } from 'node:crypto';
import type { Config } from '../config.js';
import {
  effectiveAllowedHosts,
  expandCidrList,
  isHostAllowed,
  isIpAllowed,
  isOriginAllowed,
  normalizeIp,
  type Cidr,
} from './net.js';

export const AUTH_REALM = 'ssvc-openconnect-mcp';

export interface AccessPolicy {
  /** Bearer-токен клиентов (SSVC_MCP_TOKEN). */
  token: string;
  /** Разрешённые сети клиентов. */
  cidrs: Cidr[];
  /** Разрешённые значения Host/Origin. */
  allowedHosts: string[];
}

/** Отказ контура A. `status` — то, что уйдёт клиенту. */
export interface AccessFailure {
  status: 401 | 403;
  code: 'unauthorized' | 'forbidden';
  message: string;
  hint?: string;
  details?: Record<string, unknown>;
  headers?: Record<string, string>;
}

/** Запрос в объёме, который нужен проверке (не тащим node:http в чистые функции). */
export interface AccessRequest {
  remoteAddress?: string;
  headers: Record<string, string | string[] | undefined>;
}

export function createAccessPolicy(cfg: Config): AccessPolicy {
  return {
    token: cfg.mcp.token,
    cidrs: expandCidrList(cfg.mcp.allowedCidrs),
    allowedHosts: effectiveAllowedHosts(cfg),
  };
}

/** Сравнение строк в постоянном времени. Длина и содержимое не влияют на время. */
export function constantTimeEquals(left: string, right: string): boolean {
  const leftDigest = createHash('sha256').update(left, 'utf8').digest();
  const rightDigest = createHash('sha256').update(right, 'utf8').digest();
  return timingSafeEqual(leftDigest, rightDigest) && left === right;
}

function headerValue(
  headers: AccessRequest['headers'],
  name: string,
): string | undefined {
  const target = name.toLowerCase();
  for (const [key, value] of Object.entries(headers)) {
    if (key.toLowerCase() !== target) continue;
    if (value === undefined) return undefined;
    return Array.isArray(value) ? value.join(', ') : value;
  }
  return undefined;
}

/** Разбирает `Authorization: Bearer <token>`; схема сравнивается без учёта регистра. */
export function bearerToken(header: string | undefined): string | null {
  if (header === undefined) return null;
  const match = /^Bearer[ \t]+(\S+)[ \t]*$/i.exec(header.trim());
  return match === null ? null : match[1]!;
}

/**
 * Проверка сети и заголовков без токена (для `/healthz`).
 * `null` — доступ разрешён.
 */
export function checkNetwork(policy: AccessPolicy, req: AccessRequest): AccessFailure | null {
  const ip = req.remoteAddress;
  if (!isIpAllowed(ip, policy.cidrs)) {
    return {
      status: 403,
      code: 'forbidden',
      message: 'Адрес клиента не входит в список разрешённых (SSVC_MCP_ALLOWED_CIDRS)',
      hint:
        'Подключайтесь из доверенной сети или добавьте свою подсеть в SSVC_MCP_ALLOWED_CIDRS ' +
        'и перезапустите сервис.',
      details: { remote: ip === undefined ? null : normalizeIp(ip) },
    };
  }

  const host = headerValue(req.headers, 'host');
  if (!isHostAllowed(host, policy.allowedHosts)) {
    return {
      status: 403,
      code: 'forbidden',
      message: 'Заголовок Host не входит в список разрешённых (SSVC_MCP_ALLOWED_HOSTS)',
      hint:
        'Обращайтесь к сервису по имени из SSVC_MCP_ALLOWED_HOSTS или добавьте нужное имя в .env. ' +
        'Проверка защищает от DNS-rebinding.',
      details: { host: host ?? null, allowed_hosts: policy.allowedHosts },
    };
  }

  const origin = headerValue(req.headers, 'origin');
  if (!isOriginAllowed(origin, policy.allowedHosts)) {
    return {
      status: 403,
      code: 'forbidden',
      message: 'Источник запроса (Origin) не разрешён',
      hint:
        'Браузерные клиенты не поддерживаются: обращайтесь к /mcp из MCP-клиента или curl. ' +
        'Если нужен именно этот источник — добавьте его имя в SSVC_MCP_ALLOWED_HOSTS.',
      details: { origin: origin ?? null },
    };
  }

  return null;
}

/** Полная проверка: сеть + токен. `null` — доступ разрешён. */
export function authorize(policy: AccessPolicy, req: AccessRequest): AccessFailure | null {
  const networkFailure = checkNetwork(policy, req);
  if (networkFailure !== null) return networkFailure;

  const header = headerValue(req.headers, 'authorization');
  const token = bearerToken(header);
  if (token === null) {
    return {
      status: 401,
      code: 'unauthorized',
      message: 'Не передан Bearer-токен MCP-клиента',
      hint:
        'Передайте заголовок Authorization: Bearer <SSVC_MCP_TOKEN>. Значение токена — в .env сервиса ' +
        '(сгенерировать новый: openssl rand -hex 32).',
      headers: { 'WWW-Authenticate': `Bearer realm="${AUTH_REALM}"` },
    };
  }
  if (!constantTimeEquals(token, policy.token)) {
    return {
      status: 401,
      code: 'unauthorized',
      message: 'Неверный Bearer-токен MCP-клиента',
      hint:
        'Сверьте токен клиента со значением SSVC_MCP_TOKEN в .env сервиса. ' +
        'Смена токена применяется после перезапуска сервиса.',
      headers: { 'WWW-Authenticate': `Bearer realm="${AUTH_REALM}", error="invalid_token"` },
    };
  }
  return null;
}
