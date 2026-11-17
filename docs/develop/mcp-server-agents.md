# MCP-сервер: разбивка на пакеты работ для независимых агентов

> Документ-задание для параллельной разработки. Дополняет [mcp-server.md](mcp-server.md): там — зачем,
> что и на каком API; здесь — **кто, в каких файлах и с какими контрактами делает**, чтобы пакеты можно
> было выполнять независимо и собрать без переписывания.
>
> **Реализация не начата.** Ниже — спецификация, готовая к запуску: волна 0 (каркас) → волны 1–3
> (домены) → волна 4 (сборка).

---

## 0. Как пользоваться

- **Лиду (сборщику):** §1–§3 задают правила и контракты; §4 — каркас, который делается первым; §15 —
  сборка, §16 — волны, §17–§18 — шаблоны задания и отчёта.
- **Агенту домена:** прочитать §1–§3 (правила и контракты), затем свою секцию из §4–§14 целиком. Ничего
  вне своей секции и вне списка «Файлы» трогать нельзя.
- **Всё, что не описано явно** (структура ответа устройства, коды ошибок), берётся из
  [mcp-server.md §2](mcp-server.md) и из кода прошивки, а не додумывается.

Главное правило: **пакет считается выполненным, когда он компилируется, проходит свои тесты и не
изменил ни одного чужого файла.** «Работает у меня в голове» не считается.

---

## 1. Принципы нарезки

1. **Один пакет = один домен API.** Домен = группа REST-эндпоинтов с общим смыслом (команды, настройки,
   датчики, журналы…). Домены почти не пересекаются, поэтому их можно делать параллельно.
2. **Каркас — блокер.** Всё, что нужно всем (транспорт, конфиг, клиент устройства, ошибки, схемы
   телеметрии, тестовый двойник), делает WP0. До его приёмки домены не начинаются.
3. **Владение файлами жёсткое.** У каждого файла ровно один владелец. Пересечение = конфликт при
   параллельной работе, поэтому оно запрещено, даже если «одна строчка».
4. **Контракт важнее реализации.** Агент домена может поменять внутренности своего сервиса, но не
   сигнатуры из §2. Если сигнатуры не хватает — это «запрос на изменение контракта» в отчёте, а не
   правка чужого файла.
5. **Тесты — часть пакета.** Каждый пакет приносит тесты на свои инструменты; приёмка — зелёный
   `npm test` на собранном дереве, а не только «код написан».
6. **Сборка — отдельный пакет.** Никто из доменных агентов не регистрирует свои инструменты в общем
   реестре: это делает WP11 одним движением, чтобы не было гонки за `src/tools/index.ts`.

### 1.1 Матрица владения

| Область | Владелец | Кто ещё может читать |
|---|---|---|
| `package.json`, `tsconfig*.json`, `.env.example`, `.gitignore`, `Dockerfile` | WP0 / WP11 | все |
| `src/config.ts`, `src/logging.ts`, `src/errors.ts`, `src/audit.ts`, `src/rawlog.ts` | WP0 | все |
| `src/http/*`, `src/index.ts` (сборка приложения), `src/mcp/*`, `src/policy/*` | WP0 | WP11 (только подключение модулей) |
| `src/device/*` (клиент, авторизация, кэш, схемы `common`/`info`/`telemetry`) | WP0 | все |
| `src/device/schemas/<домен>.ts` | агент домена | все |
| `src/domain/<домен>.ts`, `src/tools/<домен>.tools.ts`, `test/<домен>.test.ts` | агент домена | все |
| `src/prompts/prompts.ts` | WP10 | WP11 |
| `src/tools/index.ts`, `mock/*`, `test/e2e/*`, `README.md` | WP11 | все |

---

## 2. Общие контракты (менять нельзя)

### 2.1 Ошибки и результат инструмента

```ts
// src/errors.ts — WP0
export type ErrorCode =
  | 'device_unreachable' | 'device_timeout' | 'auth_failed' | 'auth_required'
  | 'forbidden_admin_required' | 'feature_unsupported' | 'queue_full' | 'guard_blocked'
  | 'validation_failed' | 'partial_apply' | 'busy' | 'not_found'
  | 'confirmation_required' | 'disabled_by_policy' | 'unsupported_operation'
  | 'device_error' | 'internal_error';

export interface ErrorEnvelope {
  ok: false;
  error: {
    code: ErrorCode;
    message: string;   // по-русски, для человека
    hint?: string;     // по-русски, что сделать
    retryable: boolean;
    details?: Record<string, unknown>;
  };
}

export class OcError extends Error {
  constructor(code: ErrorCode, message: string, opts?: {
    hint?: string; retryable?: boolean; details?: Record<string, unknown>;
    httpStatus?: number; cause?: unknown;
  });
}

export function fail(code: ErrorCode, message: string, opts?: {
  hint?: string; retryable?: boolean; details?: Record<string, unknown>;
}): ErrorEnvelope;

export function errorEnvelope(error: unknown): ErrorEnvelope;
```

Правила:

- сообщения и `hint` — **по-русски**; `code`, ключи `details` — латиницей;
- `OcError` бросаем внутри домена, конверт `fail` возвращаем из обработчика инструмента;
- неизвестные исключения не глотаем: их поймает `registerTools` и превратит в `internal_error`.

### 2.2 Определение инструмента

```ts
// src/mcp/types.ts — WP0
import type { z } from 'zod';
import type { ToolAnnotations } from '@modelcontextprotocol/sdk/types.js';

export interface ToolContext {
  cfg: Config;
  client: DeviceClient;
  caps: CapabilityCache;
  log: Logger;
  audit: AuditLog;
  raw: RawLogger;
}
export interface DeviceMeta {
  url: string;        // без пароля
  firmware: string;   // oc.version
  ssvcVersion: string;
  api: string;        // "1.10"
  apiCode: number;
  compatibility: 'unknown' | 'too_old' | 'supported';
}

export type ToolResult =
  | ({ ok: true } & Record<string, unknown>)
  | ErrorEnvelope;

export interface ToolDef {
  name: string;                       // oc_<домен>_<действие>
  title: string;                      // по-русски, до 60 символов
  description: string;                // по-русски, 1–2 предложения, «что делает / чего не делает»
  inputSchema: z.ZodRawShape;         // НЕ z.object(...)
  outputSchema?: z.ZodRawShape;       // по умолчанию не задаём (см. §2.3)
  annotations: {
    readOnlyHint: boolean;
    destructiveHint: boolean;
    idempotentHint: boolean;
    openWorldHint: false;
  };
  handler: (args: Record<string, any>, ctx: ToolContext) => Promise<ToolResult>;
}

export function defineTool(def: ToolDef): ToolDef;

// src/mcp/registry.ts — WP0
export function registerTools(server: McpServer, ctx: ToolContext, tools: ToolDef[]): void;
```

### 2.3 Правило про `outputSchema` и `structuredContent`

SDK проверяет `structuredContent` на соответствие `outputSchema`, если схема объявлена, и ведёт себя
строго к лишним полям. Поэтому:

- **по умолчанию `outputSchema` не указываем**, а `structuredContent` отдаём (SDK это разрешает);
- если конкретному инструменту нужна строгая схема вывода — **сначала подтвердить поведение тестом**
  (WP0 обязан это проверить и зафиксировать в `test/registry.test.ts`);
- поля ответа описываем в `description` инструмента и в типах TypeScript.

WP0 обязан проверить оба сценария (со схемой и без) и записать вывод в README пакета — от этого зависит
весь каталог.

### 2.4 Клиент устройства

```ts
// src/device/DeviceClient.ts — WP0
export interface RequestOptions {
  query?: Record<string, string | number | boolean | undefined>;
  timeoutMs?: number;
  anonymous?: boolean;                 // без Authorization (signIn)
  rawBody?: string;                    // CSV/текст как есть
  headers?: Record<string, string>;
}
export interface HttpResponse {
  status: number;
  headers: Record<string, string>;
  body: string;                        // всегда строка
  ms: number;
}
export interface CommandAccepted {
  accepted: true;
  command: string;
  warnings: string[];
  raw: string;
}

export class DeviceClient {
  get<T>(path: string, schema: z.ZodType<T>, opts?: RequestOptions): Promise<T>;
  post<T>(path: string, body: unknown, schema: z.ZodType<T>, opts?: RequestOptions): Promise<T>;
  put<T>(path: string, body: unknown, schema: z.ZodType<T>, opts?: RequestOptions): Promise<T>;
  del<T>(path: string, body: unknown, schema: z.ZodType<T>, opts?: RequestOptions): Promise<T>;

  /** POST /rest/commands с разбором 200/409/503. */
  sendCommand(name: string, parameters?: string): Promise<CommandAccepted>;

  /** Один писатель на устройство: записи выполняются строго последовательно. */
  withWrite<T>(key: string, fn: () => Promise<T>): Promise<T>;

  /** Сырой запрос — только для CSV, 202-поллинга и нестандартных случаев. */
  request(method: string, path: string, body?: unknown, opts?: RequestOptions): Promise<HttpResponse>;
}
```

Гарантии, на которые могут рассчитывать домены:

| Гарантия | Поведение |
|---|---|
| Авторизация | `signIn`, кэш JWT, один повтор после `401`; `404` на `verifyAuthorization` = security off |
| Ретраи | только `GET` (сеть, `502-504`, `503 queue_full`), экспоненциальный backoff, 3 попытки |
| Запись | `POST/PUT/DELETE` не ретраятся; сериализуются через `withWrite` при необходимости |
| Ошибки | `401/403/404/503` → соответствующие `OcError`; тело сохраняется в `details.body` |
| Сырой лог | каждый обмен пишется в `RawLogger` согласно `SSVC_RAW_LOG` |
| Валидация | ответ парсится zod-схемой домена; несоответствие → `OcError('device_error')` |

### 2.5 Конфиг

```ts
// src/config.ts — WP0
export interface Config {
  device: { url: string; user: string; password: string; timeoutMs: number; pollHz: number };
  mcp: {
    host: string; port: number; token: string;
    allowedHosts: string[]; allowedCidrs: string[];
    tls: { cert: string; key: string } | null;
    maxSessions: number; idleMs: number;
  };
  raw: { mode: 'off' | 'errors' | 'all'; dir: string; maxMb: number };
  allowDestructive: boolean;
  allowDebug: boolean;
  auditPath: string;
  logLevel: LogLevel;
}
export function loadConfig(env?: NodeJS.ProcessEnv): Config;   // бросает при отсутствии обязательных
```

### 2.6 Политика риска

```ts
// src/policy/risk.ts — WP0
export type RiskClass = 0 | 1 | 2 | 3 | 4;

// src/policy/confirmation.ts — WP0
export function enforceRisk(
  ctx: ToolContext,
  tool: string,
  risk: RiskClass,
  args: { confirm?: boolean; reason?: string },
): void;

export function auditAction(ctx: ToolContext, entry: {
  tool: string; risk: RiskClass; confirm?: boolean; reason?: string;
  result: string; details?: Record<string, unknown>;
}): void;
```

Матрица (реализуется в `enforceRisk`):

| Риск | Требование |
|---|---|
| 0 | ничего |
| 1 | ничего, но результат пишется в лог |
| 2 | `confirm === true`, иначе `OcError('confirmation_required')` с русской подсказкой |
| 3 | `confirm === true` + `reason` (строка ≥ 10 символов после trim) + `cfg.allowDestructive`, иначе `OcError('disabled_by_policy')` |
| 4 | то же, что 3, плюс отдельный список разрешённых операций (по умолчанию пуст) |
| — | `oc_emergency_stop` риска не требует: защитное действие |

### 2.7 Тестовый двойник устройства

```ts
// test/fakeHttp.ts — WP0
export interface FakeCall {
  method: string; path: string;
  query: Record<string, string>;
  body: unknown;
  headers: Record<string, string>;
}
export interface FakeReply { status?: number; body?: unknown; headers?: Record<string, string> }
export type FakeRoute = {
  method: 'GET' | 'POST' | 'PUT' | 'DELETE';
  path: string | RegExp;
  reply: (call: FakeCall) => FakeReply | Promise<FakeReply>;
};

export function fakeHttp(routes: FakeRoute[]): {
  http: HttpLike;
  calls: FakeCall[];
  addRoute(route: FakeRoute): void;
};

/** Готовый ToolContext с фейковым устройством, выключенным сырым логом и временным каталогом. */
export function makeTestContext(routes: FakeRoute[], overrides?: {
  cfg?: Partial<Config>;
  allowDestructive?: boolean;
}): Promise<{ ctx: ToolContext; calls: FakeCall[]; dir: string; cleanup(): Promise<void> }>;
```

### 2.8 Соглашения

| Тема | Правило |
|---|---|
| Язык | `title`, `description`, описания zod-полей, сообщения и подсказки ошибок — по-русски. Код, имена, ключи JSON — латиницей |
| Импорты | ESM с явным `.js`: `import { x } from './x.js'` (сборка `tsc`, `module: NodeNext`) |
| Схемы | zod 3, `z.record(z.unknown())` для «сырых» вложенных объектов, `.passthrough()` где фронт/прошивка может добавить поля |
| Тесты | `node:test` + `node:assert/strict`; только `test/fakeHttp.ts`, никакого реального сетевого доступа |
| Логи | `ctx.log.child({ tool })`, в лог — без секретов, без целых тел ответов |
| Записи | изменяющие вызовы оборачивать в `ctx.client.withWrite('<ключ>', …)` |
| Комментарии | по-русски, объясняют «почему», а не «что» |
| Никаких | `git commit`, `git checkout`, правок `lib/framework`, `interface/`, прошивки |

---

## 3. Карта пакетов

| ID | Пакет | Дни | Зависит от | Основные файлы |
|---|---|---|---|---|
| WP0 | Каркас: HTTP, конфиг, клиент устройства, ошибки, схемы info/telemetry, тест-двойник | 3 | — | `src/config.ts`, `src/http/*`, `src/device/*`, `src/mcp/*`, `src/policy/*`, `test/fakeHttp.ts` |
| WP1 | Информация, возможности, телеметрия, диагностика | 2 | WP0 | `src/domain/info.ts`, `src/domain/telemetry.ts`, `src/tools/info.tools.ts`, `src/tools/telemetry.tools.ts`, `src/resources/*` |
| WP2 | Команды, ожидание, предохранители | 2 | WP0 | `src/domain/commands.ts`, `src/device/schemas/commands.ts`, `src/tools/commands.tools.ts` |
| WP3 | Настройки SSVC | 1.5 | WP0 | `src/domain/settings.ts`, `src/device/schemas/settings.ts`, `src/tools/settings.tools.ts` |
| WP4 | Датчики, зоны, пороги тревог | 2 | WP0 | `src/domain/sensors.ts`, `src/device/schemas/sensors.ts`, `src/tools/sensors.tools.ts` |
| WP5 | Реле и аппаратная конфигурация | 2 | WP0 | `src/domain/relay.ts`, `src/domain/hardware.ts`, `src/device/schemas/relay.ts`, `src/device/schemas/hardware.ts`, `src/tools/relay.tools.ts`, `src/tools/hardware.tools.ts` |
| WP6 | Датчики уровня XKC-25 | 2 | WP0 | `src/domain/xkc25.ts`, `src/device/schemas/xkc25.ts`, `src/tools/xkc25.tools.ts` |
| WP7 | Журналы ректификации | 2 | WP0 | `src/domain/logs.ts`, `src/device/schemas/logs.ts`, `src/tools/logs.tools.ts` |
| WP8 | Профили | 2 | WP0 | `src/domain/profiles.ts`, `src/device/schemas/profiles.ts`, `src/tools/profiles.tools.ts` |
| WP9 | Файлы, подсистемы, система | 1.5 | WP0 | `src/domain/files.ts`, `src/domain/system.ts`, `src/device/schemas/files.ts`, `src/device/schemas/system.ts`, `src/tools/files.tools.ts`, `src/tools/system.tools.ts` |
| WP10 | Промпты-сценарии | 1 | WP1–WP3 (имена инструментов) | `src/prompts/prompts.ts`, `test/prompts.test.ts` |
| WP11 | Сборка, e2e, Docker, релизная закалка | 3 | все | `src/tools/index.ts`, `mock/*`, `test/e2e/*`, `Dockerfile`, `README.md` |

Суммарно ≈ 24 человеко-дня; при 4 параллельных агентах — около 7 календарных дней.

---

## 4. WP0 — каркас (делается первым, блокирует остальные)

**Цель:** работающий HTTP-сервис без доменных инструментов, с проверенными транспортом, сессиями,
авторизацией и тестовым двойником устройства.

**Файлы (создаёт только WP0):**

```
mcp/package.json, tsconfig.json, tsconfig.test.json, .gitignore, .env.example
src/config.ts, src/logging.ts, src/errors.ts, src/audit.ts, src/rawlog.ts
src/http/app.ts, src/http/auth.ts, src/http/sessions.ts, src/http/net.ts
src/mcp/types.ts, src/mcp/registry.ts, src/mcp/server.ts
src/policy/risk.ts, src/policy/confirmation.ts
src/device/http.ts, src/device/AuthSession.ts, src/device/DeviceClient.ts
src/device/CapabilityCache.ts, src/device/WriteQueue.ts, src/device/RawLogger.ts
src/device/schemas/common.ts, src/device/schemas/info.ts, src/device/schemas/telemetry.ts
src/index.ts
test/fakeHttp.ts, test/http.test.ts, test/registry.test.ts, test/deviceClient.test.ts
```

**Требования:**

1. **Стек:** Node ≥ 22, ESM, TypeScript strict, `@modelcontextprotocol/sdk@1.32.2`, zod 3, встроенный
   `node:http`. Никаких Fastify/Express: SDK уже приносит лишнее, а маршрутов у нас три.
2. **Транспорт:** `StreamableHTTPServerTransport` в stateful-режиме на пути `/mcp`:
   `POST` (JSON-RPC), `GET` (SSE), `DELETE` (закрытие сессии); `Mcp-Session-Id`; `isInitializeRequest`;
   коды `400/404/503` по протоколу; `MAX_SESSIONS=32`, idle 30 мин.
3. **Доступ:** `SSVC_MCP_TOKEN` обязателен (иначе процесс падает при старте с понятным сообщением),
   сравнение в постоянном времени, `401` + `WWW-Authenticate`; проверка `Host`/`Origin` по
   `SSVC_MCP_ALLOWED_HOSTS`; CIDR-allowlist `SSVC_MCP_ALLOWED_CIDRS` (значение `private` = RFC1918).
4. **Эндпоинты сервиса:** `/mcp`, `GET /healthz` (без токена, только «жив»), `GET /readyz` (с токеном,
   проверяет `GET /rest/oc/info`), `GET /` → 404 JSON.
5. **Сырой лог:** `RawLogger` — JSONL в `SSVC_RAW_LOG_DIR`, режимы `off|errors|all`, редакция
   `authorization/password/token/access_token`, усечение 64 КБ, предел каталога `SSVC_RAW_LOG_MAX_MB`.
6. **Аудит:** JSONL, поля `ts, session, tool, risk, confirm, reason, args_hash, result, duration_ms`;
   секреты и тела файлов не пишутся.
7. **`enforceRisk`** по матрице §2.6; `auditAction` — запись после действия.
8. **Схемы:** `info` (полный ответ `/rest/oc/info`), `telemetry` (telemetry + status + lastUpdate),
   `common` (единый формат ошибок устройства).
9. **Ошибки:** `httpError()` — маппинг `401/403/404/503` в коды; тело — в `details.body` (усечённое).
10. **Тест-двойник:** `fakeHttp` + `makeTestContext` (§2.7), полностью в памяти, без сети и без диска
    вне временного каталога.
11. **Скрипты:** `build`, `start`, `typecheck`, `build:test`, `test`. Тесты — `node:test` (Vitest/esbuild
    в этой среде не запускаются: песочница блокирует spawn с пайпами). Установка зависимостей —
    `npm install --ignore-scripts --cache .npm-cache` внутри `mcp/`.

**Тесты WP0 (обязательные):**

| Тест | Что проверяет |
|---|---|
| `http.test.ts` | `initialize` по HTTP создаёт сессию; повтор с `Mcp-Session-Id` работает; без токена `401`; чужой `Host` → `403`; неизвестная сессия → `404`; `DELETE` закрывает; лимит сессий → `503`; `/healthz` без токена |
| `registry.test.ts` | инструмент регистрируется и вызывается через клиента; `ok:false` → `isError:true`; **поведение `structuredContent` с `outputSchema` и без** — зафиксировать в README |
| `deviceClient.test.ts` | `signIn` + кэш токена; повтор после `401`; `404` на `verifyAuthorization` = security off; ретрай только `GET`; `409 blockReasons` → `guard_blocked`; `503 queue_full` → `queue_full`; `withWrite` сериализует |

**Приёмка WP0:** `npm run typecheck` — 0 ошибок; `npm test` — зелёный; `npm run build && node dist/index.js`
поднимается с тестовым `.env`, `/healthz` отвечает, `initialize` через MCP Inspector проходит;
README фиксирует решение по `outputSchema`.

---

## 5. WP1 — информация, возможности, телеметрия, диагностика

> **Обновление после §9 (обязательно к исполнению).** Прошивка уже отдаёт
> `GET /rest/oc/snapshot` — один admin-запрос с `{observed_at, info, telemetry, status, lastUpdate, sensor, faults, partial[]}`.
> Опрос нескольких эндпоинтов ниже оставлен как справка по формам, но состояния по умолчанию
> читаются снимком: `oc_get_state` = snapshot, `oc_get_telemetry` = snapshot без `sensor`
> (либо `include_sensors:true`). `partial[]` обязательно доносить до агента: компонент, которого
> нет, — это «данных нет», а не «ноль». Время `lastUpdate`/`observed_at` — это `millis()` устройства,
> поэтому `stale` считается по возрасту с момента ответа, а не сравнением с системными часами.
> События `/ws/events` (`telemetry`, `stage`) в v1 не используются: ожидание изменения — опрос
> snapshot раз в 2 с. Интеграция событий — отдельный опциональный пакет после WP11.

**REST-контракт устройства:**

```
GET /rest/oc/info
→ { "ssvc": { "version": "2.5.1", "api": "1.10", "api_code": 110, "mode": "late_heads" },
    "oc": { "version": "0.2.9.1", "is_support_api": true, "is_support_release": true,
            "api_min": "1.7", "api_target": "1.10", "api_compatibility": "supported",
            "api_features": [ { "name": "get_log", "min_api": "1.10", "available": true } ] } }

GET /rest/features → { "mqtt": true, "ntp": true, "security": true, ... }        (карта bool)
GET /rest/telemetry
→ { "telemetry": { "type": "heads", "tank_mmhg": 12.5, "common": { "mmhg": "12.5", "tp1": 78.6, ... },
                   "stop": false, "stops": 0, ... },
    "status": { "stage": "heads", "status": "running", "startTime": "...", "endTime": "",
                "stages": { ... }, "uartConnectionError": false },
    "lastUpdate": 123456 }
GET /rest/sensor → { "receiver_tank_1": { "28ff...": { "v": 78.6, "u": "°C", "type": "DS18B20" } } }
```

**Файлы:** `src/domain/info.ts`, `src/domain/telemetry.ts`, `src/tools/info.tools.ts`,
`src/tools/telemetry.tools.ts`, `src/resources/info.resource.ts`, `src/resources/telemetry.resource.ts`,
`test/info.test.ts`, `test/telemetry.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_get_info` | 0 | — | `device`, `firmware`, `ssvc_version`, `api`, `api_code`, `mode`, `compatibility`, `is_support_release`, `api_min`, `api_target` |
| `oc_get_capabilities` | 0 | `include_runtime?` (bool, по умолчанию true) | `api_features[]`, `features`, `unavailable[]` (имена недоступных возможностей) |
| `oc_get_telemetry` | 0 | `include_sensors?` (bool, false) | `telemetry`, `status`, `observed_at`, `stale`, `last_update_ms` |
| `oc_get_state` | 0 | `include?` (`telemetry,sensors,faults,info`, по умолчанию все) | один вызов `GET /rest/oc/snapshot`: `info`, `telemetry`, `status`, `sensor?`, `faults?`, `partial[]`, `observed_at`, `stale` |
| `oc_get_process_status` | 0 | — | `stage`, `status`, `uart_error`, `started_at`, `ended_at`, `stages` |
| `oc_diagnostics` | 0 | — | `config` (без секретов), `auth` (ok/disabled), `capability_cache` (age_ms), `counters`, `sessions`, `last_audit[]` |

Готовые описания (вставлять как есть):

- `oc_get_info` — «Информация о контроллере»: «Возвращает версию прошивки OpenConnect, версию и режим
  UART API контроллера SSVC и совместимость. Используйте перед любыми действиями, чтобы понять, какие
  функции доступны.»
- `oc_get_capabilities` — «Доступные функции устройства»: «Возвращает реестр возможностей UART API
  (predec, extended_heads, get_log и другие) и рантайм-флаги прошивки. Показывает, что именно
  поддерживает подключённый контроллер; недоступные функции перечислены явно.»
- `oc_get_telemetry` — «Телеметрия процесса»: «Возвращает текущий снимок телеметрии: стадию, статус,
  показания SSVC, давление и признак потери UART. Контроллер обновляет данные примерно раз в две
  секунды, поле last_update_ms показывает возраст снимка.»
- `oc_get_process_status` — «Статус ректификации»: «Возвращает только стадию и статус процесса без
  полной телеметрии. Подходит для частого опроса и проверок «процесс идёт или стоит».»
- `oc_diagnostics` — «Диагностика MCP-сервера»: «Возвращает состояние самого MCP-сервера: конфигурацию
  без секретов, статус авторизации на контроллере, возраст кэша возможностей, счётчики запросов и
  последние события аудита. К контроллеру не обращается.»

**Ресурсы:** `openconnect://info` (JSON из `/rest/oc/info` + `features`), `openconnect://telemetry`
(последний снимок). Реализуются через `server.registerResource` в `src/resources/*`; WP11 подключает их.

**Тесты:** маппинг info → плоские поля; `api_features` с `available:false`; телеметрия без `telemetry`
(ошибка устройства) → `device_error`; `stale` при `lastUpdate` старше 10 с; `oc_diagnostics` не делает
сетевых вызовов.

**Приёмка:** инструменты возвращают `ok:true` на фикстурах из §2.4; `oc_start`-подобных действий нет;
ни одного обращения вне `/rest/oc/info`, `/rest/features`, `/rest/telemetry`, `/rest/sensor`.

---

## 6. WP2 — команды, ожидание, предохранители

> **Обновление после §9 (обязательно к исполнению).** Успешный `POST /rest/commands` теперь отдаёт
> **JSON**: `{"status":"accepted","command":"<имя>","commandId":<число>,"warnings":[…]?}`.
> Каждая команда журналируется, и `GET /rest/commands/{id}` возвращает
> `{id,command,state,queued_at,started_at,finished_at,ok,reason,confidence:"transport"}`, где `state`
> ∈ `queued/running/sent/done/rejected/timeout`. Поэтому:
> - `accepted` в инструменте должен содержать `command_id` и `warnings[]` (предупреждения ≠ отказ);
> - для команд, которые ждут реакции (`status`, `version`, `get_settings`, `set`, `get_log`),
>   инструмент опрашивает `GET /rest/commands/{id}` до терминального состояния и возвращает его;
> - для `start/next/pause/resume/stop` терминальное `sent` означает «ушло в UART, ответа не будет» —
>   это норма, а не ошибка; фактический результат берётся из `oc_wait_for_change`/снимка;
> - `404` на `/rest/commands/{id}` — «запись вытеснена» (журнал 16 записей, TTL 10 мин), а не «команда
>   не выполнялась»: в этом случае падать нельзя, нужно вернуть `command_id` + `state:"unknown"`;
> - `confidence:"transport"` всегда, и это надо писать в описании инструмента прямо: подтверждение
>   транспортное, физического действия сервер не подтверждает.
> Коды 409/503/501 из таблицы ниже остаются в силе.

**REST-контракт:**

```
POST /rest/commands   { "commands": "start", "parameters": "" }
→ 200 text/plain "Command accepted"
→ 409 application/json { "status": "rejected", "command": "start", "blockReasons": ["..."] }
→ 409 text/plain "Command unsupported by device API"      (feature_unsupported)
→ 503 text/plain "Command queue is full"                  (queue_full)
→ 501 text/plain "Command not implemented"

Разрешённые имена: at, next, pause, stop, start, resume, version, get_settings,
settings, emergency_stop, status, set, get_log.
```

**Файлы:** `src/domain/commands.ts`, `src/device/schemas/commands.ts`,
`src/tools/commands.tools.ts`, `test/commands.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_start` | 2 | `confirm:literal(true)` | `accepted`, `preflight{guard_mode, block_start, block_reasons[], warnings[]}`, `observed_at` |
| `oc_pause` | 2 | `confirm` | `accepted`, `observed_at` |
| `oc_resume` | 2 | `confirm` | `accepted`, `observed_at` |
| `oc_next` | 2 | `confirm` | `accepted`, `observed_at` |
| `oc_stop` | 2 | `confirm` | `accepted`, `observed_at` |
| `oc_emergency_stop` | — | — | `accepted`, `observed_at`; подтверждения не требует |
| `oc_wait_for_change` | 0 | `expect{stage?,status?,telemetry_type?,uart_error?}`, `timeout_s` 1..600 (120), `poll_ms` 250..5000 (1000) | `matched`, `elapsed_s`, `observed`, `timeout` |
| `oc_send_command` | 2 | `command` (enum служебных), `parameters?`, `confirm` | `accepted`, `command`, `warnings[]` |

Готовые описания:

- `oc_start` — «Запустить ректификацию»: «Ставит команду START в очередь контроллера и возвращает
  результат постановки вместе с предстартовой проверкой датчиков уровня. НЕ ждёт начала процесса:
  чтобы дождаться стадии, используйте oc_wait_for_change.»
- `oc_pause` — «Поставить отбор на паузу»: «Ставит команду PAUSE в очередь контроллера. Процесс
  остаётся запущенным, отбор приостанавливается до oc_resume.»
- `oc_resume` — «Продолжить отбор»: «Ставит команду RESUME в очередь контроллера и возвращает результат
  постановки.»
- `oc_next` — «Перейти к следующей фракции»: «Ставит команду NEXT в очередь контроллера: текущая
  фракция завершается, начинается следующая по программе.»
- `oc_stop` — «Остановить ректификацию»: «Ставит команду STOP в очередь контроллера. Это штатная
  остановка; для аварийной используйте oc_emergency_stop.»
- `oc_emergency_stop` — «Аварийная остановка»: «Немедленно ставит STOP в приоритетную очередь
  контроллера, минуя накопленные команды. Подтверждение не требуется: это защитное действие.»
- `oc_wait_for_change` — «Дождаться изменения состояния»: «Опрашивает телеметрию, пока указанные поля не
  совпадут с ожидаемыми, либо пока не истечёт таймаут. Возвращает наблюдённое состояние и признак
  таймаута; сам ничего не меняет.»
- `oc_send_command` — «Служебная команда контроллеру»: «Отправляет служебную команду из белого списка
  (version, status, get_settings, set, at, get_log). Команды управления процессом сюда не входят —
  для них есть отдельные инструменты.»

**Логика:**

1. `enforceRisk(ctx, tool, risk, args)` → для риска 2.
2. Для `oc_start`: `GET /rest/oc/xkc25/status`; если `blockStart === true` → `fail('guard_blocked', …,
   {details:{block_reasons, warnings}})` **без** отправки команды.
3. `client.sendCommand()`, затем `auditAction`.
4. `oc_wait_for_change`: цикл с `setTimeout`, первый снимок — сразу; сравнение только по указанным полям;
   `AbortSignal` при закрытии сессии не обязателен, но таймаут обязателен; при `uartConnectionError`
   возвращать `ok:true` с `observed.uart_error:true` (это наблюдение, а не ошибка).

**Тесты:** 200 → `accepted:true`; 409 JSON → `guard_blocked` с причинами; 409 text → `feature_unsupported`;
503 → `queue_full`; `confirm` отсутствует → `confirmation_required`; `emergency_stop` без `confirm`;
ожидание: совпадение с первого опроса, совпадение на третьем, таймаут; дедуп двух `oc_start` подряд.

**Приёмка:** ни один инструмент не ретраит `POST`; `oc_start` при `blockStart` не делает `POST /rest/commands`.

---

## 7. WP3 — настройки SSVC

**REST-контракт:**

```
GET /rest/settings → { "ssvcSettings": { "heads": [120, 3000], "hyst": 0.5, ... } }
PUT /rest/settings   { "hyst": 0.5, "decrement": 2 }          ← ПЛОСКИЙ объект, без обёртки
→ 200 { "success": true, "accepted": 2, "ignored": [], "skipped": [] }
→ 503 { "success": false, "partial": true, "accepted": 1, "failed": ["decrement=2"], ... }
→ 400 { "hyst": "Invalid value" }
```

Ключи записи — закрытый набор `PARAM_HANDLERS` (heads, hearts, late_heads, tails, hyst, decrement,
tank_mmhg, tank_mmhg_act, heads_timer, late_heads_timer, hearts_timer, tails_temp, start_delay,
hearts_finish_temp, formula_start_temp, valve_bw, release_speed, release_timer, heads_final, parallel,
parallel_v1, parallel_v3, s_temp, s_hyst, s_speed, s_decrement, s_timer, formula, predec,
extended_heads). Остальные ключи попадают в `ignored`.

**Файлы:** `src/domain/settings.ts`, `src/device/schemas/settings.ts`, `src/tools/settings.tools.ts`,
`test/settings.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_get_settings` | 0 | — | `settings` (объект), `observed_at` |
| `oc_update_settings` | 1 | `settings: record` (плоский), `strict?` (bool, false) | `accepted`, `ignored[]`, `skipped[]`, `applied` (перечитанные настройки), `partial` (bool) |

Описания:

- `oc_get_settings` — «Действующие настройки SSVC»: «Возвращает текущие настройки контроллера SSVC:
  фракции, таймеры, гистерезис, декремент, формулу и прочее. Значения читаются из последнего ответа
  контроллера.»
- `oc_update_settings` — «Изменить настройки SSVC»: «Отправляет частичное обновление настроек: в теле
  только изменяемые поля, плоским объектом. Возвращает отчёт контроллера: что принято, что проигнорировано
  и что отброшено из-за версии API, а также перечитанные настройки. При частичном применении повторять
  весь запрос нельзя.»

**Логика:**

1. `GET /rest/settings` → `{ssvcSettings}` → наружу отдаём **только** `ssvcSettings`.
2. `PUT` — тело ровно `args.settings`, **без** обёртки `ssvcSettings` (частая ошибка).
3. Разбор ответа: `success`, `ignored`, `skipped`; при `503 partial:true` — вернуть `ok:true` с
   `partial:true` и `hint`, не бросать исключение и **не повторять** запрос.
4. После успеха — `GET /rest/settings` и вернуть `applied`.
5. `strict:true` — если `ignored.length > 0` или `skipped.length > 0`, вернуть `ok:false` с
   `validation_failed` и перечнем (для агентов, которым важно «всё или ничего»).

**Тесты:** чтение обёртки и разворачивание; PUT плоским телом (проверить, что в теле нет `ssvcSettings`);
`ignored`/`skipped`; `503 partial` → `ok:true partial:true`; `strict` → `ok:false`; `400` → `validation_failed`
с полями.

---

## 8. WP4 — датчики, зоны, пороги

**REST-контракт:**

```
GET  /rest/sensor → { "<zone>": { "<address>": { "v": 78.6, "u": "°C", "type": "DS18B20" } } }
GET  /rest/zones  → { "zones": { "<address>": <zoneIndex|"zoneName"> } }
POST /rest/zones    { "zones": { "<address>": <zone> } }        (полная карта, admin)
PUT  /rest/sensors/zone?address=<hex>&zone=<name>               (одна привязка)
GET  /rest/alarms → { "thresholds": { "<address>": { "enabled": true, "min": 0, "dangerous": 80, "critical": 100 } } }
POST /rest/alarms   { "thresholds": { "<address>": { ... } } }
```

Все четыре эндпоинта — **admin-only**: `403` маппится в `forbidden_admin_required` с подсказкой.

**Файлы:** `src/domain/sensors.ts`, `src/device/schemas/sensors.ts`, `src/tools/sensors.tools.ts`,
`test/sensors.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_get_sensors` | 0 | `zone?` | `zones[]` с `zone`, `readings[]{address,value,unit,type}`, `observed_at` |
| `oc_get_zones` | 0 | — | `zones` (карта адрес→зона) |
| `oc_set_zone` | 1 | `address` (hex, 16 символов), `zone` (string) | `ok`, `address`, `zone` |
| `oc_get_alarms` | 0 | — | `thresholds` (карта) |
| `oc_set_alarm_thresholds` | 1 | `thresholds: record` | `thresholds` (перечитанные) |

Описания:

- `oc_get_sensors` — «Показания датчиков»: «Возвращает текущие показания датчиков по зонам: адрес,
  значение, единицы и тип. Данные обновляются циклом опроса прошивки, а не по запросу.»
- `oc_get_zones` — «Привязка датчиков к зонам»: «Возвращает карту «адрес датчика → зона»: куб, царга,
  приёмные ёмкости.»
- `oc_set_zone` — «Привязать датчик к зоне»: «Меняет зону одного датчика по его адресу. Требует прав
  администратора; изменение сразу применяется к опросу.»
- `oc_get_alarms` — «Пороги тревог»: «Возвращает пороги min/dangerous/critical по каждому датчику и
  признак включения тревог.»
- `oc_set_alarm_thresholds` — «Изменить пороги тревог»: «Обновляет пороги тревог для указанных датчиков.
  Передавайте только те адреса, которые нужно изменить.»

**Тесты:** плоский маппинг зон; пустой `/rest/sensor`; `403` → `forbidden_admin_required`; невалидный
адрес → `validation_failed` без запроса к устройству; пороги с частичным набором полей.

---

## 9. WP5 — реле и аппаратная конфигурация

**REST-контракт:**

```
GET  /rest/oc/relay/capabilities → { "supported": true, "pcf8574": {...} } | { "supported": false }
GET  /rest/oc/relay/metadata → { "supported": true, "relays": [ { "index": 1, "label": "Реле 1",
      "editable": true, "targetBit": 0 } ], "relayChipCount": 1, "relayLineCount": 8, ... }
GET  /rest/oc/relay/state → { "supported": true, "userShadow": 0, "userShadows": [0] }
POST /rest/oc/relay/override { "bit": 3, "enable": true, "energized": false } → { "ok": true }
→ 400 { "error": "bit out of range" } | 403 { "error": "Relay is reserved for AlarmManager" }

GET  /rest/oc/hardware-config → { ..., "capabilities": {...}, "needsReboot": false,
      "bmp581ProbeOk": true, "relayPcf8574ProbeOk": [true], "relayHardwareOk": true }
PUT  /rest/oc/hardware-config { "pressureSensorEnabled": true, ... } → та же структура
GET  /rest/oc/hardware-faults → { "faults": [ { "code": "i2c_nack", "codeLabel": "...",
      "address": 39, "role": "lcd1602" } ], "count": 1, "i2cBusActive": true, "i2cScan": [39, 104] }
```

**Файлы:** `src/domain/relay.ts`, `src/domain/hardware.ts`, `src/device/schemas/relay.ts`,
`src/device/schemas/hardware.ts`, `src/tools/relay.tools.ts`, `src/tools/hardware.tools.ts`,
`test/relay.test.ts`, `test/hardware.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_get_relay_metadata` | 0 | — | `supported`, `relays[]`, `relay_chip_count`, `relay_line_count` |
| `oc_get_relay_state` | 0 | — | `supported`, `user_shadow`, `user_shadows[]` |
| `oc_relay_override` | 2 | `bit` (int ≥0), `enable` (bool, **обязателен**), `energized` (bool, **обязателен**), `confirm` | `ok`, `bit`, `enable`, `energized` |
| `oc_get_hardware_config` | 0 | — | конфиг + `capabilities` + `needs_reboot` + результаты проб |
| `oc_update_hardware_config` | 1 | `config: record` (частичный) | конфиг после записи + `needs_reboot` |
| `oc_get_hardware_faults` | 0 | — | `faults[]`, `count`, `i2c_bus_active`, `i2c_scan[]` |

Описания:

- `oc_get_relay_metadata` — «Метаданные реле»: «Возвращает список реле расширителя: номера, подписи,
  доступность для ручного управления и системные реле, зарезервированные под аварии.»
- `oc_get_relay_state` — «Состояние реле»: «Возвращает текущее состояние линий реле (битовую маску) для
  каждого расширителя.»
- `oc_relay_override` — «Ручное управление реле»: «Включает или выключает одно реле вручную, минуя
  правила автоматики. Обязательно указывайте и enable, и energized: значение по умолчанию у контроллера
  включает реле. Системные реле аварий менять нельзя.»
- `oc_get_hardware_config` — «Аппаратная конфигурация»: «Возвращает настройки подключённого оборудования:
  датчик давления, RTC, дисплей, реле, датчики уровня, а также результаты проб I²C и признак
  необходимости перезагрузки.»
- `oc_update_hardware_config` — «Изменить аппаратную конфигурацию»: «Частично обновляет настройки
  оборудования. Изменения, требующие перезагрузки, отмечаются в needs_reboot; применяются после
  перезапуска контроллера.»
- `oc_get_hardware_faults` — «Аппаратные сбои»: «Возвращает активные сбои оборудования (I²C NACK, шина
  недоступна, устройство не найдено, потеря UART) и список ответивших адресов I²C для диагностики.»

**Логика:**

- `supported:false` (GPIO-сборка) → `fail('feature_unsupported', 'Реле на PCF8574 недоступны в этой
  сборке прошивки')`, не ошибка устройства.
- `oc_relay_override`: `bit` валидируется по `relay_line_count` из metadata; `enable`/`energized` —
  обязательные поля схемы; `403` → `forbidden_admin_required`? Нет: это резерв под аварии →
  `fail('validation_failed', 'Реле зарезервировано под аварийную сигнализацию')`.
- `oc_update_hardware_config`: если в ответе `needsReboot === true`, добавить `hint` «изменения вступят
  в силу после oc_restart».

**Тесты:** `supported:false` для всех реле-инструментов; metadata → состояние; override с явными
полями (проверить тело запроса); 403 резерва; 400 вне диапазона; hardware config с пробами;
`needsReboot` → hint.

---

## 10. WP6 — датчики уровня XKC-25

**REST-контракт:**

```
GET  /rest/oc/xkc25/status → { "sensors": [ { "index": 0, "enabled": true, "pinSource": 0,
      "gpio": 4, "pinLabel": "GPIO 4", "zone": "receiver_tank_1", "calibrated": true,
      "live": true, "rawLevel": 1, "filled": false } ],
      "triggerAction": 1, "startGuardMode": 2, "blockStart": false,
      "blockReasons": [], "warnings": ["..."] }
POST /rest/oc/xkc25/calibrate { "index": 0, "step": "without_liquid" }
→ 200 { "ok": true, "completed": false, "code": "ok", "message": "...", "checklist": [], "sensor": {...} }
POST /rest/oc/xkc25/calibrate { "index": 0, "step": "verify_return", "timeoutMs": 5000 }
→ 202 { "status": "pending", "index": 0, "code": "ok", "message": "Проверка возврата запущена" }
GET  /rest/oc/xkc25/verify-status → { "status": "pending"| "done" | "none", "index": 0,
      "ok": true, "completed": true, "code": "ok", "rawLevel": 0, "message": "..." }
```

**Файлы:** `src/domain/xkc25.ts`, `src/device/schemas/xkc25.ts`, `src/tools/xkc25.tools.ts`,
`test/xkc25.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_get_xkc25_status` | 0 | — | `sensors[]`, `trigger_action`, `start_guard_mode`, `block_start`, `block_reasons[]`, `warnings[]` |
| `oc_xkc25_calibrate` | 1 | `index` 0..3, `step` enum, `timeout_ms` 1000..15000 (5000) | `ok`, `completed`, `code`, `message`, `checklist[]`, `sensor`, `verify` (для `verify_return`) |
| `oc_xkc25_verify_status` | 0 | — | `status`, `ok?`, `completed?`, `code?`, `message?` |

Описания:

- `oc_get_xkc25_status` — «Датчики уровня XKC-25»: «Возвращает состояние датчиков уровня: зона, источник
  сигнала, калибровка, текущий уровень и заполненность, а также режим предохранителя старта и причины
  возможной блокировки.»
- `oc_xkc25_calibrate` — «Откалибровать датчик уровня»: «Выполняет один шаг калибровки: без жидкости, с
  жидкостью или проверку возврата. Шаг проверки возврата выполняется асинхронно — инструмент дожидается
  результата в пределах timeout_ms.»
- `oc_xkc25_verify_status` — «Результат проверки возврата»: «Возвращает результат последней асинхронной
  проверки возврата датчика: идёт, завершена, не запускалась.»

**Логика:**

- `without_liquid` / `with_liquid` → один `POST`, вернуть тело как есть.
- `verify_return` → `POST`; при `202` опрашивать `GET /rest/oc/xkc25/verify-status` каждые 500 мс до
  `status !== 'pending'` или `timeout_ms`; если не дождались — `ok:true`, `verify.status:'pending'`,
  `hint` «проверка продолжается, повторите oc_xkc25_verify_status».
- `404 sensor_not_available` / `404 sensor_not_configured` → `fail('not_found', …)` с русской подсказкой.

**Тесты:** статус с блокировкой и без; шаги калибровки 200; `verify_return` 202 → done с третьей попытки;
`verify_return` таймаут → `pending`; 404 → `not_found`; 409 `unstable_signal` → `validation_failed` с кодом.

---

## 11. WP7 — журналы ректификации

**REST-контракт (stateful-сессия на устройстве!):**

```
GET  /rest/logs → { "status": "list", "total": 0, "received": 0,
                    "files": ["12.CSV", "13.CSV"] }                 (запрос списка из idle)
GET  /rest/logs?statusOnly=1 → только снимок, без новой команды
GET  /rest/logs?refresh=1    → принудительно запросить список
GET  /rest/logs/{id} → 202 { "status": "receiving", "total": 120, "received": 40 }
                     → 200 text/csv, Content-Disposition: attachment; filename="12.CSV"
DELETE /rest/logs → { "status": "idle" }                             (отмена)
409 { "error": "log_transfer_busy" } | 503 { "error": "queue_full" } | 503 { "error": "log_heap_low" }
```

**Файлы:** `src/domain/logs.ts`, `src/device/schemas/logs.ts`, `src/tools/logs.tools.ts`,
`test/logs.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_logs_list` | 0 | `refresh?` | `status`, `files[]`, `total`, `received` |
| `oc_logs_download` | 0 | `process_id` (int ≥1), `save_dir?` | `saved_path`, `bytes`, `lines`, `preview` (первые 20 строк), `elapsed_s` |
| `oc_logs_cancel` | 1 | — | `status` |

Описания:

- `oc_logs_list` — «Список журналов»: «Запрашивает у контроллера список доступных журналов ректификации.
  Контроллер отдаёт список не мгновенно, поэтому при статусе receiving повторите вызов.»
- `oc_logs_download` — «Скачать журнал»: «Скачивает журнал по номеру процесса и сохраняет его в файл на
  сервере, возвращая путь, размер и краткий фрагмент. Полный CSV в ответ не попадает, чтобы не
  переполнять контекст.»
- `oc_logs_cancel` — «Отменить передачу журнала»: «Прерывает текущую передачу журнала с контроллера и
  освобождает сессию передачи.»

**Логика:**

- Скачивание: `GET /rest/logs/{id}` → `202` → пауза 700 мс → повтор, пока не `200`; суммарный таймаут 45 с
  (не 90: MCP-клиенты по умолчанию обрывают вызов на 60 с, и инструмент с бюджетом 90 с не успевал бы ответить);
  `200` → сохранить в `save_dir` (по умолчанию `logs/downloads`), имя `<id>.csv`, посчитать строки и
  первые 20 строк (усечь строку до 200 символов).
- Один скачивающий вызов на устройство: `withWrite('logs', …)`.
- `409 log_transfer_busy` → `fail('busy', 'Передача журнала уже идёт', {retryable:true})`.
- `503 log_heap_low` → `fail('busy', 'На контроллере недостаточно памяти для журнала', {retryable:true})`.
- `oc_logs_list` при `409 busy` не считает это ошибкой: вернуть `status:'busy'`, `ok:true`.

**Тесты:** список из `idle`; `statusOnly` не шлёт `refresh`; 202 три раза → 200 CSV (проверить имя файла и
строки); 409 busy; 503 heap_low; отмена.

---

## 12. WP8 — профили

**REST-контракт:**

```
GET    /rest/profiles → [ { "id": "p1", "name": "Спирт-сырец", "createdAt": "...", "isApplied": true } ]
GET    /rest/profiles/active → { "id": "p1" } | 404
GET    /rest/profiles/content?id=p1 → { ...полный профиль... }
POST   /rest/profiles { "name": "...", "content": {...} } → 201 { "success": true, "id": "p2" }
POST   /rest/profiles/copy { "sourceId": "p1", "newName": "Копия" } → 201 { "success": true, "id": "p3" }
POST   /rest/profiles/content { "id": "p1", "content": {...} } → 200 { "success": "Profile content updated" }
POST   /rest/profiles/save { "id": "p1" } → 200 { "success": true, ... }
POST   /rest/profiles/set-active { "id": "p1" } → 200 { "success": "Profile set as active and applied" }
PUT    /rest/profiles/meta { "id": "p1", "name": "Новое имя" } → 200
DELETE /rest/profiles/delete { "id": "p1" } → 200 | 404
```

**Файлы:** `src/domain/profiles.ts`, `src/device/schemas/profiles.ts`,
`src/tools/profiles.tools.ts`, `test/profiles.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_profiles_list` | 0 | — | `profiles[]`, `active_id?` |
| `oc_profile_get` | 0 | `id` | `profile` (полный объект) |
| `oc_profile_create` | 1 | `name`, `content?` | `id` |
| `oc_profile_copy` | 1 | `source_id`, `new_name` | `id` |
| `oc_profile_rename` | 1 | `id`, `name` | `ok` |
| `oc_profile_update_content` | 1 | `id`, `content` | `ok` |
| `oc_profile_save_current` | 1 | `id` | `ok` |
| `oc_profile_apply` | 2 | `id`, `confirm` | `ok`, `applied_id` |
| `oc_profile_delete` | 3 | `id`, `confirm`, `reason` | `ok`, `deleted_id` |

Описания:

- `oc_profiles_list` — «Список профилей»: «Возвращает профили ректификации с идентификаторами, названиями
  и признаком активного.»
- `oc_profile_get` — «Профиль целиком»: «Возвращает полное содержимое профиля: параметры сырца, фракции,
  настройки SSVC и правила реле.»
- `oc_profile_create` — «Создать профиль»: «Создаёт новый профиль с указанным названием и, при
  необходимости, содержимым.»
- `oc_profile_copy` — «Скопировать профиль»: «Создаёт копию существующего профиля под новым названием.»
- `oc_profile_rename` — «Переименовать профиль»: «Меняет отображаемое название профиля.»
- `oc_profile_update_content` — «Обновить содержимое профиля»: «Сливает переданные ключи с содержимым
  профиля: верхнеуровневые ключи заменяются или добавляются, `null` удаляет ключ. Остальные настройки
  профиля сохраняются — это не замена файла целиком (проверено на прошивке 0.2.9.2: `updateProfileContent`
  делает merge, а не replace).»
- `oc_profile_save_current` — «Сохранить текущие настройки в профиль»: «Записывает действующие настройки
  контроллера в указанный профиль.»
- `oc_profile_apply` — «Применить профиль»: «Делает профиль активным и применяет его настройки к
  контроллеру. Влияет на процесс отбора.»
- `oc_profile_delete` — «Удалить профиль»: «Удаляет профиль без возможности восстановления. Требует
  подтверждения и причины.»

**Логика:** `enforceRisk` для 2 и 3; для `delete` — обязательный `reason`; ответы 404 → `not_found`;
`id` валидируется как непустая строка.

**Тесты:** список + активный; 404 активного → `active_id:null`; создание → id; копия; переименование;
применение без confirm → `confirmation_required`; удаление без reason → `validation_failed`; удаление при
`allowDestructive:false` → `disabled_by_policy`.

---

## 13. WP9 — файлы, подсистемы, система

**REST-контракт:**

```
GET    /rest/files → [ { "name": "config", "type": "directory", "children": [...] },
                       { "name": "x.bin", "type": "file", "size": 123 } ]
GET    /rest/files{path} → октеты
POST   /rest/files/copy { "source": "/a", "destination": "/b" } → 201
DELETE /rest/files{path} → 204
GET    /rest/subsystem → { "i2c_bus": true, "thermal": true, "telegram_bot": false, ... }
PUT    /rest/subsystem { "thermal": false } → { "success": true, "errors": [] }
GET    /rest/systemStatus → { "heap": 123456, "uptime": 3600, "firmware": "0.2.9.1", ... }
POST   /rest/restart → 200
```

Подсистемы `telegram_bot`/`vk_bot` могут включаться дольше 60 секунд — таймаут инструмента 120 с.

**Файлы:** `src/domain/files.ts`, `src/domain/system.ts`, `src/device/schemas/files.ts`,
`src/device/schemas/system.ts`, `src/tools/files.tools.ts`, `src/tools/system.tools.ts`,
`test/files.test.ts`, `test/system.test.ts`.

**Инструменты:**

| Имя | Риск | Аргументы | Возвращает |
|---|---|---|---|
| `oc_files_list` | 0 | `path?` | `tree[]` |
| `oc_file_download` | 0 | `path`, `save_dir?` | `saved_path`, `bytes` |
| `oc_file_copy` | 1 | `source`, `destination` | `ok` |
| `oc_file_delete` | 3 | `path`, `confirm`, `reason` | `ok` |
| `oc_get_subsystems` | 0 | — | `subsystems` (карта bool) |
| `oc_set_subsystem` | 1 | `name`, `enabled` | `success`, `errors[]`, `subsystems` (перечитанные) |
| `oc_system_status` | 0 | — | `system` (heap, uptime, firmware, chip) |
| `oc_restart` | 3 | `confirm`, `reason` | `ok`, `hint` |

Описания:

- `oc_files_list` — «Файлы контроллера»: «Возвращает дерево файлов и каталогов на контроллере.»
- `oc_file_download` — «Скачать файл»: «Скачивает файл с контроллера и сохраняет его на сервере,
  возвращая путь и размер.»
- `oc_file_copy` — «Скопировать файл»: «Копирует файл внутри контроллера.»
- `oc_file_delete` — «Удалить файл»: «Удаляет файл на контроллере без возможности восстановления.
  Требует подтверждения и причины.»
- `oc_get_subsystems` — «Состояние подсистем»: «Возвращает, какие подсистемы контроллера включены:
  I²C, тепловая, атмосферная, Telegram, VK, настройки.»
- `oc_set_subsystem` — «Включить или выключить подсистему»: «Переключает подсистему контроллера.
  Некоторые подсистемы (Telegram, VK) включаются дольше минуты — инструмент ждёт до двух минут.»
- `oc_system_status` — «Состояние системы»: «Возвращает состояние контроллера: свободную память,
  время работы, версию прошивки и данные чипа.»
- `oc_restart` — «Перезагрузить контроллер»: «Перезагружает контроллер. Текущий процесс ректификации
  будет прерван; требует подтверждения и причины.»

**Логика:** пути файлов начинаются с `/`, запрет `..` (валидация, `validation_failed`); `oc_set_subsystem`
— таймаут 120 000 мс; `oc_restart` — ответ 200, затем `hint` «контроллер недоступен 20–40 секунд».

**Тесты:** дерево файлов; скачивание в временный каталог; path traversal → `validation_failed`; удаление
без причины; переключение подсистемы с ошибкой → `errors[]`; restart 200 → hint; `403` → admin.

---

## 14. WP10 — промпты-сценарии

**Файлы:** `src/prompts/prompts.ts`, `test/prompts.test.ts`.

**Промпты** (русские, 5 штук; каждый — `server.registerPrompt` с аргументами и готовым текстом):

1. `start_rectification` — аргументы `profile_id?`, `confirm_token?`. Текст: проверить `oc_get_info`,
   `oc_get_process_status`, `oc_get_xkc25_status`; если `block_start` — объяснить `block_reasons`;
   иначе предложить `oc_start` с подтверждением и `oc_wait_for_change` на стадию `heads`.
2. `troubleshoot_start_blocked` — аргументы нет. Текст: `oc_get_xkc25_status` + `oc_get_hardware_faults`,
   разобрать коды, предложить действия.
3. `calibrate_xkc25` — аргументы `index`. Текст: три шага `oc_xkc25_calibrate`, проверка статуса между
   шагами, что делать при `unstable_signal`/`not_reacted`.
4. `analyze_last_run` — аргументы `process_id?`. Текст: `oc_logs_list`, `oc_logs_download`,
   краткая сводка по фракциям (без вставки всего CSV).
5. `apply_profile` — аргументы `profile_id`. Текст: `oc_profile_get`, показать diff с `oc_get_settings`,
   применить `oc_profile_apply` с подтверждением.

**Требования:** тексты по-русски, инструменты упоминаются по точным именам, никаких выдуманных
инструментов; промпт не выполняет действия сам, а задаёт последовательность.

**Тесты:** промпты зарегистрированы (5), имена и аргументы соответствуют таблице, текст содержит только
существующие имена инструментов (проверка по списку из `src/tools/index.ts`, если он уже собран, или по
литеральному списку).

---

## 15. WP11 — сборка, e2e, Docker, релизная закалка

**Файлы:** `src/tools/index.ts`, `src/index.ts` (правки только в части подключения), `mock/*`,
`test/e2e/*`, `Dockerfile`, `docker-compose.yml`, `README.md`, `.env.example`, `docs/develop/mcp-server.md`
(если нужны уточнения).

**Шаги:**

1. **Регистрация.** В `src/tools/index.ts` импортировать массивы `*Tools` из всех доменов и собрать
   `allTools: ToolDef[]`; в `src/index.ts` — передать в `registerTools`. Ресурсы — из
   `src/resources/*.ts`; промпты — из `src/prompts/prompts.ts`.
2. **Мок-устройство** `mock/deviceServer.ts`: HTTP-сервер с `signIn`, `oc/info`, `telemetry`, `commands`,
   `settings`, `xkc25/status`, `alarms`, `logs` (202→CSV), достаточный для e2e.
3. **e2e-тест** `test/e2e/http.e2e.test.ts`: поднять сервис на порту 0 с мок-устройством, пройти
   `initialize` → `tools/list` → `oc_get_telemetry` → `oc_get_info`; проверить `401` без токена и
   `disabled_by_policy` для `oc_profile_delete` при `SSVC_ALLOW_DESTRUCTIVE=0`.
4. **Docker:** multi-stage, `node:22-alpine`, non-root, `HEALTHCHECK` на `/healthz`, `.env` через
   `env_file`, `EXPOSE 8080`.
5. **README:** запуск, `.env`, выдача токена, подключение клиента, ограничения (HTTP до устройства, один
   контроллер), решение по `outputSchema`, как включить разрушительные операции.
6. **Релизная закалка:** `.env.example` с `SSVC_ALLOW_DESTRUCTIVE=0`, `SSVC_RAW_LOG=off`,
   `SSVC_ALLOW_DEBUG=0`; тест, что каждый инструмент риска 3 отвечает `disabled_by_policy`.
7. **Финальная проверка:** `npm run typecheck && npm test`; проверка, что в дереве нет чужих правок
   (`git status`).

**Приёмка WP11:** e2e зелёный, контейнер собирается и проходит healthcheck, README позволяет поднять
сервис с нуля, релизные значения по умолчанию безопасны.

---

## 16. Волны исполнения

| Волна | Пакеты | Параллелизм | Условие старта |
|---|---|---|---|
| 0 | WP0 | 1 | — |
| 1 | WP1, WP2, WP3, WP4 | 4 | WP0 принят |
| 2 | WP5, WP6, WP7, WP8 | 4 | WP0 принят (WP1 желателен: схемы info/telemetry) |
| 3 | WP9, WP10 | 2 | WP0 принят; WP1–WP3 приняты (имена инструментов для промптов) |
| 4 | WP11 | 1 (лид) | все пакеты приняты |

Правила волн:

- агент получает **только свою секцию** и общие §1–§3; чужие секции читать можно, править — нет;
- если агенту не хватает контракта — он **не** правит общий файл, а завершает работу со статусом
  `blocked` и описанием, чего не хватило;
- перед сдачей агент обязан проверить `git status` и убедиться, что изменены только его файлы;
- лид после каждой волны прогоняет `npm run typecheck && npm test` и только потом запускает следующую.

---

## 17. Шаблон задания агенту

```text
Ты реализуешь пакет <ID> (<название>) в репозитории ssvc_open_connect, каталог mcp/.

Прочитай целиком:
  1) docs/develop/mcp-server.md — §2 (REST API устройства), §2.5 (ловушки контракта), §3 (архитектура);
  2) docs/develop/mcp-server-agents.md — §1–§3 (правила и контракты) и §<N> (твоя секция).

Твоя зона — только файлы из списка «Файлы» в твоей секции. Общие файлы (config, http, device,
mcp/registry, policy, tools/index) не редактируй. Если контракта не хватает — верни status: blocked.

Требования:
  - описания инструментов и все сообщения об ошибках — по-русски;
  - схемы входа — z.ZodRawShape, обязательные поля не делать опциональными;
  - изменяющие вызовы — через client.withWrite, риск — через enforceRisk, после действия — auditAction;
  - тесты на node:test с test/fakeHttp.ts, без реальной сети;
  - npm run typecheck и npm test должны быть зелёными.

Верни отчёт строго по шаблону §18.
```

---

## 18. Шаблон отчёта агента

```json
{
  "status": "done",
  "package": "WP4",
  "files": ["src/domain/sensors.ts", "src/device/schemas/sensors.ts", "src/tools/sensors.tools.ts", "test/sensors.test.ts"],
  "tools": ["oc_get_sensors", "oc_get_zones", "oc_set_zone", "oc_get_alarms", "oc_set_alarm_thresholds"],
  "tests": { "total": 9, "passed": 9 },
  "typecheck": "ok",
  "contract_changes": [],
  "notes": "Эндпоинты admin-only; 403 маппится в forbidden_admin_required."
}
```

`status`: `done` | `blocked`. При `blocked` — `notes` содержит точный перечень недостающих контрактов.

---

## 19. Риски координации и меры

| Риск | Проявление | Мера |
|---|---|---|
| Два агента правят один файл | конфликт, потеря работы | жёсткая матрица владения §1.1, проверка `git status` |
| Расхождение схем | домены по-разному трактуют `telemetry`/`info` | общие схемы только в WP0; домен не переопределяет их |
| Дублирование утилит | три реализации «плоского объекта настроек» | общие помощники только в WP0; домен просит расширение контракта |
| Тихая подмена контракта | инструмент игнорирует `confirm` | тест на `confirmation_required` в каждом пакете риска ≥2 |
| Сетевые тесты | нестабильный CI | только `fakeHttp`, e2e — отдельным тестом WP11 |
| Забытая регистрация | инструмент не виден клиенту | WP11 собирает реестр и проверяет `tools/list` в e2e |
| Дрейф прошивки | схемы не совпадают с ответом | `.passthrough()` и фикстуры из §2.4; `device_error` с телом ответа |

---

## 20. Чек-лист приёмки разбивки

- [ ] У каждого файла ровно один владелец; пересечений нет.
- [ ] Каждый пакет компилируется и тестируется независимо от остальных доменов.
- [ ] Все общие типы и сигнатуры зафиксированы в §2 и не требуют догадок.
- [ ] Для каждого инструмента есть имя, риск, аргументы, результат и русское описание.
- [ ] Для каждого пакета перечислены тест-кейсы и критерии приёмки.
- [ ] Порядок волн и условия старта не допускают гонок.
- [ ] Определён протокол эскалации (`blocked`) и шаблон отчёта.
- [ ] Сборка (WP11) включает e2e, Docker и релизную закалку.
