# MCP-сервер OpenConnect (пакет WP0 — каркас)

HTTP-сервис, который открывает контроллер OpenConnect (SSVC) MCP-клиентам: один
процесс, один контроллер, транспорт Streamable HTTP на `/mcp`, Bearer-токен,
управление сессиями, клиент устройства с авторизацией/ретраями и тестовый двойник
контроллера.

**Статус: WP0.** Здесь есть транспорт, доступ, конфиг, `DeviceClient`, политика риска,
тест-двойник и схемы `info`/`telemetry`. Доменных инструментов `oc_*` пока нет:
каталог собирает WP11 (`src/tools/index.ts` → массив `ToolDef` → `startService`).
Пакет уже принимает инструменты через `registerTools`, поэтому домены подключаются
без правок каркаса.

Требования задания: `docs/develop/mcp-server-agents.md` §4, архитектура и REST-контракт —
`docs/develop/mcp-server.md`, проверенный контракт прошивки — `docs/develop/mcp-contract-notes.md`.

---

## 1. Требования и установка

- Node.js ≥ 22 (проверено на 24.11.1), npm 11.
- Зависимости фиксированы: `@modelcontextprotocol/sdk@1.32.2`, `zod@3.25`.
- Установка (как в задании, без lifecycle-скриптов):

```bash
cd mcp
npm install --ignore-scripts --cache .npm-cache
```

Сборка и проверки:

```bash
npm run typecheck   # tsc по src и test, 0 ошибок
npm test            # node:test, 26 тестов
npm run build       # dist/
npm start           # node dist/index.js
```

`npm test` сначала компилирует `src/` и `test/` в `.test-dist/` (`npm run build:test`),
затем запускает `node --test ".test-dist/test/*.test.js"`. Шаблон-глоб вместо каталога
используется потому, что Node 24 больше не принимает каталог позиционным аргументом
`--test`. Vitest/esbuild в этой среде не работают (песочница блокирует spawn с пайпами),
поэтому тестовый раннер — только `node:test`.

---

## 2. Конфигурация `.env`

Файл `.env` лежит рядом с сервисом (`mcp/.env`), в git не попадает (`.gitignore`).
Шаблон — `.env.example`; переменные окружения приоритетнее файла (удобно для Docker).

```bash
cp .env.example .env      # Windows PowerShell: Copy-Item .env.example .env
```

Обязательные переменные (без них процесс падает на старте с русским сообщением):

| Переменная | Зачем |
|---|---|
| `SSVC_MCP_TOKEN` | Bearer-токен MCP-клиентов. Без него сервер **не стартует**: открытый порт без аутентификации недопустим |
| `SSVC_DEVICE_URL` | Базовый URL контроллера без завершающего `/`, например `http://192.168.1.42`. Прошивка слушает только HTTP, порт 80 |
| `SSVC_PASSWORD` | Пароль контроллера. Нужна учётная запись **admin**: `/rest/telemetry` и `/rest/sensor` отдают данные только администратору |
| `SSVC_USER` | Логин, по умолчанию `admin` |

Основные необязательные переменные:

| Переменная | По умолчанию | Назначение |
|---|---|---|
| `SSVC_MCP_BIND` | `0.0.0.0` | Адрес прослушивания (`127.0.0.1` — только локально) |
| `SSVC_MCP_PORT` | `8080` | Порт сервиса |
| `SSVC_MCP_ALLOWED_HOSTS` | localhost/127.0.0.1/[::1] + `SSVC_MCP_BIND` | Разрешённые `Host`/`Origin` (защита от DNS-rebinding) |
| `SSVC_MCP_ALLOWED_CIDRS` | `private` | Кто может подключаться: `private` = RFC1918 + петлевые адреса, либо CIDR через запятую |
| `SSVC_MCP_MAX_SESSIONS` | `32` | Лимит одновременных MCP-сессий |
| `SSVC_MCP_IDLE_MS` | `1800000` (30 мин) | Закрытие простаивающей сессии |
| `SSVC_MCP_TLS_CERT` / `_KEY` | — | Встроенный TLS (оба файла сразу). Иначе — работа за TLS-прокси |
| `SSVC_ALLOW_DESTRUCTIVE` | `1` в коде, `0` в `.env.example` | Разрушительные операции (риск 3/4) |
| `SSVC_ALLOW_DEBUG` | `1` в коде, `0` в `.env.example` | Разрешить `debug:true` в вызовах |
| `SSVC_RAW_LOG` | `all` в коде, `off` в `.env.example` | `off` / `errors` / `all` — сырые обмены с устройством |
| `SSVC_RAW_LOG_DIR`, `SSVC_RAW_LOG_MAX_MB` | `./logs/raw`, `50` | Каталог и предел объёма сырых логов |
| `SSVC_AUDIT_PATH` | `./logs/audit.jsonl` | Журнал аудита действий |
| `SSVC_LOG_LEVEL` | `info` | `debug` / `info` / `warn` / `error` / `silent` |

> Кодовые значения по умолчанию — отладочные (`all`, `allow destructive`, `allow debug`).
> Это осознанно для P0–P3. `.env.example` содержит **релизные безопасные** значения:
> берите его за основу, а не «пустой .env».

### Выдача токена

```bash
openssl rand -hex 32
# либо без openssl:
node -e "console.log(require('node:crypto').randomBytes(32).toString('hex'))"
```

Полученное значение — в `SSVC_MCP_TOKEN`. Один токен на сервис; клиенты получают его
от администратора сервиса. Смена токена применяется после перезапуска.

---

## 3. Запуск и проверка

```bash
cd mcp
npm run build
node dist/index.js        # или npm start
```

При старте сервис пишет в stdout структурный JSON (уровень — `SSVC_LOG_LEVEL`), пароли
и токены вырезаются на уровне логгера. Если конфигурация неполная — одна строка в stderr
и код возврата 1, порт не открывается.

Проверка «жив» (без токена, годится для HEALTHCHECK):

```bash
curl -s http://127.0.0.1:8080/healthz
# {"ok":true,"status":"alive","service":"ssvc-openconnect-mcp","uptime_s":3,"sessions":0}
```

Проверка доступа: без токена — `401` и `WWW-Authenticate: Bearer`:

```bash
curl -i -s -X POST http://127.0.0.1:8080/mcp \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  --data '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"smoke","version":"1.0.0"}}}'
# HTTP/1.1 401 Unauthorized
# WWW-Authenticate: Bearer realm="ssvc-openconnect-mcp"
```

`initialize` с токеном создаёт сессию и возвращает `Mcp-Session-Id`:

```bash
curl -i -s -X POST http://127.0.0.1:8080/mcp \
  -H "Authorization: Bearer $SSVC_MCP_TOKEN" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  --data '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"smoke","version":"1.0.0"}}}'
# HTTP/1.1 200 OK
# mcp-session-id: <uuid>
```

Дальнейшие запросы — с этим заголовком; `DELETE /mcp` с тем же `Mcp-Session-Id` закрывает
сессию (повторный запрос с ним даёт `404`). `GET /readyz` с токеном проверяет
`GET /rest/oc/info` на контроллере и отвечает `200`/`503`.

---

## 4. Маршруты и коды

| Метод и путь | Доступ | Поведение |
|---|---|---|
| `POST /mcp` | токен | JSON-RPC. `initialize` создаёт сессию (`Mcp-Session-Id`); остальное — с сессией |
| `GET /mcp` | токен + сессия | SSE-поток сервер→клиент (уведомления) |
| `DELETE /mcp` | токен + сессия | Закрывает сессию |
| `GET /healthz` | сеть (CIDR + Host) | `200 {"ok":true,"status":"alive"}` — без токена |
| `GET /readyz` | токен | `200`, если устройство отвечает на `GET /rest/oc/info`, иначе `503` с конвертом ошибки |
| `GET /` и прочее | — | `404` JSON с подсказкой |

Коды протокола на `/mcp`: неизвестная сессия — `404`; не-`initialize` без
`Mcp-Session-Id` — `400`; `initialize` при исчерпанном лимите сессий — `503`;
неверный/отсутствующий токен — `401`; чужой адрес/`Host`/`Origin` — `403`.

POST отвечает JSON (`enableJsonResponse: true`): SSE нужен только для `GET`-потока
уведомлений, а JSON-ответ детерминирован и проще для curl/тестов.

---

## 5. Решение по `outputSchema` и `structuredContent` (§2.3)

**Решение: по умолчанию `outputSchema` не объявляем, `structuredContent` отдаём всегда.**
Если инструменту действительно нужна строгая схема вывода — её можно добавить, но
конверт ошибки в `structuredContent` тогда не попадает (см. ниже).

Что показал тест (`test/registry.test.ts`, реальный MCP-клиент + `InMemoryTransport`):

| Сценарий | Поведение SDK | Тест |
|---|---|---|
| `outputSchema` нет, успех с произвольными полями | `structuredContent` проходит без проверки, любые поля и вложенность сохраняются | «без outputSchema structuredContent проходит с произвольными полями» |
| `outputSchema` есть, успех соответствует схеме | сервер проверяет `structuredContent`, клиент проверяет повторно; проходит | «с outputSchema успешный structuredContent проверяется и проходит» |
| `outputSchema` есть, успех **не** соответствует схеме | сервер отвечает `isError:true` с текстом `Output validation error: ...` — ответ инструмента до клиента не доходит | «с outputSchema несоответствие схеме отвергается SDK» |
| `outputSchema` есть, инструмент вернул конверт ошибки | сервер проверку пропускает (`if (result.isError) return`), но **клиент SDK валидирует `structuredContent` даже у ошибки** — конверт ошибки был бы отвергнут | «с outputSchema отказ не несёт structuredContent» |
| `outputSchema` нет, инструмент вернул конверт ошибки | `isError:true` + `structuredContent={ok:false,error:{...}}` проходит, код ошибки машиночитаем | «ok:false превращается в isError:true и конверт ошибки» |

Отсюда правило реестра (`src/mcp/registry.ts`):

- успех → `content[0].text` = JSON конверта **и** `structuredContent` = тот же объект;
- ошибка → `isError:true`, текст = JSON конверта; `structuredContent` добавляется
  **только если `outputSchema` не объявлена**;
- `code`/`message`/`hint` ошибки всегда есть в текстовой части, поэтому клиент, который
  не читает `structuredContent`, ничего не теряет.

Почему так: строгая схема вывода полезна там, где ответ стабилен (например,
`oc_get_telemetry`), но она же превращает любую ошибку в «несоответствие схеме», если
конверт ошибки класть в `structuredContent`. Доменам рекомендуется не объявлять
`outputSchema`, пока не появится причина, и описывать поля в `description` и типах.

---

## 6. Доступ и безопасность (контур A)

- `SSVC_MCP_TOKEN` обязателен; сравнение — в постоянном времени (SHA-256 + `timingSafeEqual`),
  поэтому длина токена не утекает по времени ответа.
- `401` всегда с `WWW-Authenticate: Bearer realm="ssvc-openconnect-mcp"`.
- `SSVC_MCP_ALLOWED_CIDRS`: `private` = RFC1918 + петлевые адреса; можно перечислить
  свои CIDR. IPv4-mapped адреса (`::ffff:127.0.0.1`) нормализуются в IPv4.
- `SSVC_MCP_ALLOWED_HOSTS` проверяется по `Host`; `Origin` (если передан) — по тому же
  списку. Запросы без `Origin` разрешены: браузерных MCP-клиентов не поддерживаем.
  Если `SSVC_MCP_BIND=0.0.0.0` и список пуст, клиенты по IP получат `403` — сервис пишет
  об этом предупреждение при старте.
- Наружу — только TLS: либо `SSVC_MCP_TLS_CERT`/`_KEY`, либо Caddy/Traefik перед сервисом.
- Секреты не попадают в лог, аудит и сырые логи: редакция на уровне логгера/`RawLogger`.

---

## 7. Ограничения

1. **Один контроллер на сервис.** Параметра `device` в инструментах нет — это решение
   из `mcp-server.md` §3.9. Несколько контроллеров = несколько экземпляров сервиса.
2. **До устройства — HTTP без TLS.** Прошивка не умеет HTTPS на порту 80. Трафик
   `сервис → контроллер` идёт по локальной сети в открытом виде: это осознанное
   ограничение, компенсируется доверенной сетью и CIDR-allowlist. Наружу сервис
   публикуется только через TLS-прокси.
3. **Браузерные клиенты не поддерживаются.** CORS выключен, `Origin` проверяется;
   подключение — MCP-клиент, IDE, curl.
4. **Нужна учётная запись admin.** Иначе телеметрия и датчики вернут `403`
   (`forbidden_admin_required`), что легко принять за «прошивка не поддерживает».
5. **Состояние сессий в памяти.** Рестарт сервиса рвёт `Mcp-Session-Id`; клиент
   пересоздаёт сессию через `initialize`. Команда, уже отправленная на контроллер,
   от этого не отменяется.
6. **`CAPABILITY_TTL` — 60 с**, сырой лог и аудит — файлы на диске; в контейнере каталог
   `logs/` должен быть volume, иначе данные теряются при пересоздании.
7. **Тестовый двойник не эмулирует всё устройство.** `fakeHttp` отвечает по таблице
   маршрутов; e2e с мок-сервером — задача WP11.

---

## 8. Разрушительные операции: включить и выключить

Матрица риска (`src/policy/risk.ts`, `src/policy/confirmation.ts`):

| Риск | Примеры | Требование |
|---|---|---|
| 0 | `oc_get_info`, `oc_get_telemetry` | ничего |
| 1 | настройки, зоны, пороги, профили | ничего, результат в лог |
| 2 | `start`, `stop`, `relay/override`, `set-active` | `confirm:true` |
| 3 | `profiles/delete`, `files` DELETE, `restart`, `factoryReset`, OTA | `confirm:true` + `reason` (≥10 символов) + `SSVC_ALLOW_DESTRUCTIVE=1` |
| 4 | отдельные разрешённые операции | то же + запись в allowlist (по умолчанию пуст) |
| — | `oc_emergency_stop` | всегда разрешён: защитное действие |

**Включить осознанно** (стенд, отладка): в `.env` поставить

```dotenv
SSVC_ALLOW_DESTRUCTIVE=1
```

и перезапустить сервис. Каждое выполнение класса 3 всё равно требует `confirm:true` и
`reason` и пишется в `SSVC_AUDIT_PATH` (JSONL: `ts, session, tool, risk, confirm, reason,
args_hash, result, duration_ms`). Без флага инструмент отвечает `disabled_by_policy`.

**Выключить перед релизом** (release hardening, P4):

```dotenv
SSVC_ALLOW_DESTRUCTIVE=0
SSVC_RAW_LOG=off
SSVC_ALLOW_DEBUG=0
```

Проверка после выключения: инструмент класса 3 должен отвечать `disabled_by_policy`,
а не «сработать». Этот же набор — в `.env.example`: файл уже содержит релизные значения.

---

## 9. Аудит, сырые логи, наблюдаемость

- `SSVC_AUDIT_PATH` (`./logs/audit.jsonl`) — по строке на действие. Тела файлов,
  журналы и секреты не пишутся; «тяжёлые» ключи (`body`, `content`, `raw`, …) вырезаются
  с пометкой `details_omitted`.
- `SSVC_RAW_LOG_DIR` (`./logs/raw/YYYY-MM-DD.jsonl`) — обмены с устройством: метод, путь,
  статус, длительность, усечённые до 64 КБ тела с `truncated:true`; `authorization`,
  `password`, `token`, `access_token` заменяются на `***`. Режимы `off|errors|all`,
  предел каталога `SSVC_RAW_LOG_MAX_MB` (удаляется самое старое).
- Лог сервиса — JSON в stdout (`ts, level, msg, ...`), без тел ответов и секретов.
- `/healthz` и `/readyz` — для HEALTHCHECK: первый не требует токена, второй проверяет
  связь с контроллером.

---

## 10. Структура пакета

```
mcp/
├── src/
│   ├── index.ts            # конфиг → зависимости → HTTP → сигналы остановки
│   ├── config.ts           # .env/окружение, fail-fast, секреты только в памяти
│   ├── logging.ts          # структурный JSON-лог + редакция секретов
│   ├── errors.ts           # ErrorCode, OcError, fail/httpError
│   ├── audit.ts            # JSONL-аудит
│   ├── rawlog.ts           # правила редакции/усечения сырых обменов
│   ├── http/
│   │   ├── app.ts          # /mcp, /healthz, /readyz, 404
│   │   ├── auth.ts         # Bearer в постоянном времени, 401/403
│   │   ├── net.ts          # CIDR, Host/Origin, нормализация IP
│   │   └── sessions.ts     # Map сессий, лимит, idle-cleanup
│   ├── mcp/
│   │   ├── types.ts        # ToolDef/ToolContext/defineTool (§2.2)
│   │   ├── registry.ts     # регистрация + единый конверт результата
│   │   └── server.ts       # McpServer на сессию
│   ├── device/             # http, AuthSession, DeviceClient, кэш, очередь, raw
│   └── policy/             # risk.ts, confirmation.ts
├── test/
│   ├── fakeHttp.ts         # тест-двойник устройства + makeTestContext (§2.7)
│   ├── http.test.ts        # сессии, токен, Host/Origin, лимит, healthz/readyz
│   ├── registry.test.ts    # регистрация/вызов, isError, outputSchema
│   └── deviceClient.test.ts# signIn, 401, security off, ретраи, 409/503, withWrite
└── .env.example
```

---

## 11. Тесты: что и как проверяется

`npm test` — **26 тестов**, все на `node:test` + `node:assert/strict`, без сети и без диска
вне временного каталога:

- `http.test.ts` (11): `initialize` создаёт сессию и возвращает `Mcp-Session-Id`; повтор
  с заголовком идёт в ту же сессию; `401` без токена/с неверным токеном + `WWW-Authenticate`;
  `403` на чужой `Host` и чужой `Origin`; `404` на неизвестную сессию; `400` на не-init
  запрос без сессии; `DELETE` закрывает сессию; лимит сессий → `503`; `GET /healthz` без
  токена; `/readyz` с токеном проверяет `/rest/oc/info` (200 и 503); `GET /` → 404 JSON.
- `registry.test.ts` (8): регистрация и вызов инструмента; `ok:false` → `isError:true`;
  `structuredContent` без `outputSchema` (проходит как есть), с `outputSchema` (проверяется,
  несоответствие отвергается, у ошибки `structuredContent` нет); исключение обработчика →
  `internal_error`; повторное имя — ошибка на старте.
- `deviceClient.test.ts` (7): `signIn` один раз и кэш JWT; повтор после `401` с повторным
  входом; `404` на `signIn`/`verifyAuthorization` = security off и анонимные запросы;
  ретраи только GET (503 на GET повторяется, на PUT — нет); `409 blockReasons` →
  `guard_blocked`; `503 queue_full` → `queue_full` без повтора; `withWrite` сериализует
  записи и не рвёт очередь после ошибки.

Тестовый двойник (`test/fakeHttp.ts`) полностью в памяти: `fakeHttp(routes)` подменяет
`HttpLike`, `makeTestContext(routes, overrides)` собирает готовый `ToolContext` с фейковым
устройством, выключенным сырым логом и временным каталогом (удаляется в `cleanup()`).

---

## 12. Подключение MCP-клиента

Сервис — обычный Streamable HTTP MCP-сервер. Общий вид конфигурации клиента (Claude
Desktop, IDE-плагины, MCP Inspector):

```json
{
  "mcpServers": {
    "openconnect": {
      "type": "http",
      "url": "http://127.0.0.1:8080/mcp",
      "headers": {
        "Authorization": "Bearer <SSVC_MCP_TOKEN>"
      }
    }
  }
}
```

Некоторые клиенты называют транспорт `"streamable-http"` или вообще не указывают `type`
(определяют по `url`). MCP Inspector: transport = `Streamable HTTP`, URL
`http://127.0.0.1:8080/mcp`, добавьте заголовок `Authorization`.

Если сервис за TLS-прокси — URL `https://mcp.example.lan/mcp`, а в
`SSVC_MCP_ALLOWED_HOSTS` добавьте `mcp.example.lan`.

---

## 13. Диагностика

| Симптом | Причина и что делать |
|---|---|
| Процесс сразу завершился с сообщением «Переменная окружения … не задана» | Нет обязательной переменной в `.env`/окружении. Сообщение называет переменную |
| `401 Unauthorized` | Нет/неверен `Authorization: Bearer …`. Сверьте токен с `SSVC_MCP_TOKEN` |
| `403` «Заголовок Host не входит…» | Обращаетесь по IP/имени, которого нет в `SSVC_MCP_ALLOWED_HOSTS`. Добавьте имя или задайте список явно |
| `403` «Адрес клиента не входит…» | Клиент вне `SSVC_MCP_ALLOWED_CIDRS`. Добавьте подсеть или используйте `private` |
| `503` «Слишком много активных сессий» | Исчерпан `SSVC_MCP_MAX_SESSIONS`. Закройте сессии (`DELETE /mcp`) или поднимите лимит |
| `404` «Сессия MCP не найдена» | Сессия истекла по idle, сервис перезапускался или клиент потерял `Mcp-Session-Id`. Пересоздайте через `initialize` |
| `/readyz` → `503 device_unreachable`/`auth_failed` | Контроллер недоступен или неверны `SSVC_USER`/`SSVC_PASSWORD`. Проверьте сеть и учётку admin |
| В ответах `device_error` «не соответствует ожидаемой схеме» | Расхождение контракта с прошивкой. Включите `SSVC_RAW_LOG=all` и посмотрите `logs/raw/`; в `details.issues` — какие поля не сошлись |
