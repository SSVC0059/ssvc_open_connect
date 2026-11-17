/**
 * Сетевой контур A: разбор CIDR и проверка `Host`/`Origin` (контракт §3.4, §3.11).
 *
 * Чистые функции без сокетов: их проверяют тесты, а `http/app.ts` только применяет.
 *
 * Решения:
 *  - `SSVC_MCP_ALLOWED_CIDRS` принимает `private` (RFC1918 + петлевые адреса) и обычные CIDR;
 *    одиночный адрес без `/` трактуется как `/32` (или `/128`);
 *  - адрес `::ffff:127.0.0.1` нормализуется в IPv4 — иначе allowlist «не видит» локального клиента;
 *  - пустой `SSVC_MCP_ALLOWED_HOSTS` = localhost/127.0.0.1/[::1] + адрес из `SSVC_MCP_BIND`;
 *    при `0.0.0.0` сервис доступен по IP, который в этот список не входит, —
 *    `app.ts` пишет об этом предупреждение при старте.
 */

import { PRIVATE_CIDRS } from '../config.js';

export type IpFamily = 4 | 6;

export interface ParsedIp {
  family: IpFamily;
  /** Адрес в виде массива байт: 4 или 16 значений. */
  bytes: number[];
}

export interface Cidr {
  family: IpFamily;
  bytes: number[];
  prefix: number;
  /** Исходная запись — для сообщений об ошибках и диагностики. */
  source: string;
}

/** Ключевое слово «вся частная сеть». */
export const PRIVATE_KEYWORD = 'private';

function parseIpv4(text: string): number[] | null {
  const parts = text.split('.');
  if (parts.length !== 4) return null;
  const bytes: number[] = [];
  for (const part of parts) {
    if (!/^\d{1,3}$/.test(part)) return null;
    const value = Number.parseInt(part, 10);
    if (value < 0 || value > 255) return null;
    bytes.push(value);
  }
  return bytes;
}

function parseIpv6(text: string): number[] | null {
  let input = text.toLowerCase();
  const zone = input.indexOf('%');
  if (zone >= 0) input = input.slice(0, zone);

  // Встроенный IPv4-хвост (`::ffff:192.168.1.1`) превращаем в две hex-группы.
  const lastColon = input.lastIndexOf(':');
  const tail = input.slice(lastColon + 1);
  if (tail.includes('.')) {
    const v4 = parseIpv4(tail);
    if (v4 === null) return null;
    const hi = ((v4[0]! << 8) | v4[1]!).toString(16);
    const lo = ((v4[2]! << 8) | v4[3]!).toString(16);
    input = `${input.slice(0, lastColon + 1)}${hi}:${lo}`;
  }

  let groups: string[];
  const doubleColon = input.indexOf('::');
  if (doubleColon >= 0) {
    const left = input.slice(0, doubleColon);
    const right = input.slice(doubleColon + 2);
    if (right.includes('::')) return null;
    const leftGroups = left === '' ? [] : left.split(':');
    const rightGroups = right === '' ? [] : right.split(':');
    const missing = 8 - leftGroups.length - rightGroups.length;
    if (missing < 1) return null;
    groups = [...leftGroups, ...new Array<string>(missing).fill('0'), ...rightGroups];
  } else {
    groups = input.split(':');
    if (groups.length !== 8) return null;
  }

  const bytes: number[] = [];
  for (const group of groups) {
    if (!/^[0-9a-f]{1,4}$/.test(group)) return null;
    const value = Number.parseInt(group, 16);
    bytes.push((value >> 8) & 0xff, value & 0xff);
  }
  return bytes.length === 16 ? bytes : null;
}

/**
 * Разбирает IP-адрес. Возвращает `null` для мусора (в отличие от CIDR, здесь ошибка
 * не повод падать: `remoteAddress` может быть неожиданным).
 */
export function parseIp(input: string): ParsedIp | null {
  let text = input.trim().toLowerCase();
  if (text.startsWith('[') && text.endsWith(']')) text = text.slice(1, -1);
  if (text === '') return null;
  if (!text.includes(':')) {
    const bytes = parseIpv4(text);
    return bytes === null ? null : { family: 4, bytes };
  }
  const bytes = parseIpv6(text);
  if (bytes === null) return null;
  // IPv4-mapped IPv6 (`::ffff:a.b.c.d`) — это IPv4-клиент за dual-stack сокетом.
  const mapped =
    bytes.slice(0, 10).every((byte) => byte === 0) && bytes[10] === 0xff && bytes[11] === 0xff;
  if (mapped) return { family: 4, bytes: bytes.slice(12) };
  return { family: 6, bytes };
}

/** Строковая форма адреса для логов: без IPv4-префикса и скобок. */
export function normalizeIp(input: string): string {
  const parsed = parseIp(input);
  if (parsed === null) return input.trim();
  if (parsed.family === 4) return parsed.bytes.join('.');
  return parsed.bytes
    .map((byte) => byte.toString(16).padStart(2, '0'))
    .join('')
    .replace(/(.{4})(?=.)/g, '$1:');
}

/** Разбирает одну запись CIDR (или одиночный адрес). Бросает при ошибке — это опечатка в .env. */
export function parseCidr(entry: string): Cidr {
  const text = entry.trim();
  if (text === '') {
    throw new Error('Пустая запись в SSVC_MCP_ALLOWED_CIDRS.');
  }
  const slash = text.indexOf('/');
  const addressPart = slash >= 0 ? text.slice(0, slash) : text;
  const prefixPart = slash >= 0 ? text.slice(slash + 1) : undefined;
  const parsed = parseIp(addressPart);
  if (parsed === null) {
    throw new Error(
      `Некорректная запись «${entry}» в SSVC_MCP_ALLOWED_CIDRS: ожидается CIDR (например, 192.168.1.0/24) ` +
        'или ключевое слово private.',
    );
  }
  const maxPrefix = parsed.family === 4 ? 32 : 128;
  let prefix = maxPrefix;
  if (prefixPart !== undefined) {
    if (!/^\d{1,3}$/.test(prefixPart)) {
      throw new Error(
        `Некорректная длина префикса в «${entry}»: ожидается число 0..${maxPrefix}.`,
      );
    }
    prefix = Number.parseInt(prefixPart, 10);
    if (prefix > maxPrefix) {
      throw new Error(`Длина префикса в «${entry}» больше ${maxPrefix}.`);
    }
  }
  return { family: parsed.family, bytes: parsed.bytes, prefix, source: text };
}

/** Разворачивает список из конфига: `private` → набор RFC1918 и петлевых сетей. */
export function expandCidrList(entries: readonly string[]): Cidr[] {
  const result: Cidr[] = [];
  for (const entry of entries) {
    if (entry.trim().toLowerCase() === PRIVATE_KEYWORD) {
      for (const cidr of PRIVATE_CIDRS) result.push(parseCidr(cidr));
      continue;
    }
    result.push(parseCidr(entry));
  }
  return result;
}

function bytesEqual(left: number[], right: number[], prefix: number): boolean {
  const fullBytes = Math.floor(prefix / 8);
  const restBits = prefix % 8;
  for (let index = 0; index < fullBytes; index += 1) {
    if (left[index] !== right[index]) return false;
  }
  if (restBits === 0) return true;
  const mask = (0xff << (8 - restBits)) & 0xff;
  return ((left[fullBytes]! & mask) === (right[fullBytes]! & mask));
}

/** Входит ли адрес в CIDR. */
export function ipInCidr(ip: ParsedIp, cidr: Cidr): boolean {
  if (ip.family !== cidr.family) return false;
  return bytesEqual(ip.bytes, cidr.bytes, cidr.prefix);
}

/** Проверка адреса клиента по allowlist. Пустой список — разрешено всё (конфиг так не делает). */
export function isIpAllowed(ip: string | undefined, cidrs: readonly Cidr[]): boolean {
  if (cidrs.length === 0) return true;
  if (ip === undefined || ip === '') return false;
  const parsed = parseIp(ip);
  if (parsed === null) return false;
  return cidrs.some((cidr) => ipInCidr(parsed, cidr));
}

/** Список по умолчанию, если `SSVC_MCP_ALLOWED_HOSTS` пуст. */
export function defaultAllowedHosts(bind: string): string[] {
  const hosts = new Set<string>(['localhost', '127.0.0.1', '[::1]', '::1']);
  const normalizedBind = bind.trim().toLowerCase();
  if (normalizedBind !== '' && normalizedBind !== '0.0.0.0' && normalizedBind !== '::') {
    hosts.add(normalizedBind);
  }
  return [...hosts];
}

/** Эффективный список Host: явный из .env или выведенный из адреса прослушивания. */
export function effectiveAllowedHosts(cfg: {
  mcp: { allowedHosts: string[]; host: string };
}): string[] {
  const explicit = cfg.mcp.allowedHosts.map((item) => item.trim()).filter((item) => item !== '');
  if (explicit.length > 0) return explicit;
  return defaultAllowedHosts(cfg.mcp.host);
}

/** Имя хоста из заголовка Host: отбрасывает порт и нормализует IPv6-скобки. */
export function hostnameFromHostHeader(hostHeader: string): string {
  const text = hostHeader.trim();
  if (text === '') return '';
  if (text.startsWith('[')) {
    const end = text.indexOf(']');
    return end > 0 ? text.slice(1, end) : text;
  }
  const colon = text.lastIndexOf(':');
  // Одиночное двоеточие — порт; два и более — IPv6 без скобок (оставляем как есть).
  if (colon > 0 && text.indexOf(':') === colon) return text.slice(0, colon);
  return text;
}

/**
 * Проверка `Host` (защита от DNS-rebinding). Сравнение регистронезависимое;
 * `allowedHosts` может содержать как имя, так и `host:port` — порт не обязателен.
 */
export function isHostAllowed(hostHeader: string | undefined, allowedHosts: readonly string[]): boolean {
  if (allowedHosts.length === 0) return true;
  if (hostHeader === undefined || hostHeader.trim() === '') return false;
  const hostname = hostnameFromHostHeader(hostHeader).toLowerCase();
  const raw = hostHeader.toLowerCase();
  return allowedHosts.some((entry) => {
    const allowed = entry.toLowerCase();
    return allowed === hostname || allowed === raw;
  });
}

/**
 * Проверка `Origin`. Запросы без Origin (curl, MCP-клиенты, серверы) разрешены —
 * браузерных MCP-клиентов мы не поддерживаем. `Origin: null` отклоняем.
 */
export function isOriginAllowed(origin: string | undefined, allowedHosts: readonly string[]): boolean {
  if (origin === undefined || origin.trim() === '') return true;
  if (allowedHosts.length === 0) return true;
  const text = origin.trim().toLowerCase();
  if (text === 'null') return false;
  let url: URL;
  try {
    url = new URL(text);
  } catch {
    return false;
  }
  // Сравниваем только хост (порт в allowlist не фиксируем: сервис может быть за прокси).
  const hostname = url.hostname.toLowerCase();
  return allowedHosts.some((entry) => hostnameFromHostHeader(entry).toLowerCase() === hostname);
}
