# MCP-сервер OpenConnect (SSVC)

HTTP-сервис, который открывает контроллер **OpenConnect (SSVC)** MCP-клиентам: один процесс —
один контроллер, транспорт **Streamable HTTP** на `/mcp`, Bearer-токен, управление сессиями,
политика риска с подтверждениями, клиент устройства с авторизацией/ретраями, мок-устройство
и Docker-образ.

**Состояние: v1 собран (WP0–WP11).** В каталоге **50 инструментов**, **5 промптов-сценариев**,
2 ресурса (`openconnect://info`, `openconnect://telemetry`), **230 тестов** (`npm test`), включая
e2e «настоящий MCP-клиент → HTTP → сервис → мок-устройство».

Требования задания и контракты: [docs/develop/mcp-server.md](../../docs/develop/mcp-server.md),
разбивка на пакеты: [docs/develop/mcp-server-agents.md](../../docs/develop/mcp-server-agents.md),
проверенный контракт прошивки: [docs/develop/mcp-contract-notes.md](../../docs/develop/mcp-contract-notes.md).

---

## 1. Что сервис делает и чего не делает

| Умеет | Не делает |
|---|---|
| Читать состояние, телеметрию, датчики, журналы, профили, файлы, системный статус | Не хранит историю: каждый ответ — снимок «сейчас» (журналы скачиваются файлом) |
| Управлять процессом ректификации (start/pause/resume/next/stop), настройками, зонами, порогами, подсистемами | Не подменяет SSVC: команды уходят в UART и подтверждаются только транспортом (`confidence:"transport"`) |
| Работать с профилями (создать/копировать/переименовать/применить/удалить) и файлами LittleFS | Не обслуживает нескольких контроллеров: параметра `device` нет, URL задаётся в `.env` |
| Отдавать MCP-ресурсы и промпты-сценарии | Не решает за оператора: разрушительные операции требуют `confirm`+`reason` и флага сервера |

Опасные действия разрешаются **политикой на стороне сервера** — MCP-клиент не может её обойти:
сначала проверяются `confirm`/`reason`, затем `SSVC_ALLOW_DESTRUCTIVE`, и только потом идёт сеть.

---

## 2. Требования и установка

- **Node.js ≥ 22** (проверено на 24.11.1), npm 10+.
- Зависимости зафиксированы: `@modelcontextprotocol/sdk@1.32.2`, `zod@3.25`.
- Docker — только если нужен контейнер (см. §7).

```bash
cd mcp
npm ci --ignore-scripts --cache .npm-cache   # установка без lifecycle-скриптов
cp .env.example .env                          # Windows PowerShell: Copy-Item .env.example .env
# заполнить .env (см. §4), сгенерировать SSVC_MCP_TOKEN
npm run build                                 # tsc → dist/
node dist/index.js
```

Проверки (то же делает CI, §13):

```bash
npm run typecheck   # tsc по src/ и test/, 0 ошибок
npm test            # 230 тестов node:test, включая e2e
npm run build       # dist/
```

`npm test` сначала компилирует `src/`, `test/` и `mock/` в `.test-dist/`, затем запускает
`node --test` по шаблонам `.test-dist/test/*.test.js` и `.test-dist/test/e2e/*.test.js`.
Тестовый раннер — только `node:test`: vitest/esbuild в этой среде не работают (песочница
блокирует spawn с пайпами).

---

## 3. Быстрый старт (5 шагов)

```bash
cd mcp
cp .env.example .env
node -e "console.log(require('crypto').randomBytes(32).toString('hex'))"   # → SSVC_MCP_TOKEN
# в .env: SSVC_DEVICE_URL=http://<ip-контроллера>, SSVC_USER=admin, SSVC_PASSWORD=<пароль>, SSVC_MCP_TOKEN=<токен>
npm ci --ignore-scripts --cache .npm-cache && npm run build && node dist/index.js
curl http://127.0.0.1:8080/healthz        # {"ok":true,...} — сервис жив
curl -H "Authorization: Bearer <токен>" http://127.0.0.1:8080/readyz   # {"ready":true,...} — контроллер отвечает
```

Дальше подключайте MCP-клиент (§6) на `http://127.0.0.1:8080/mcp` с заголовком
`Authorization: Bearer <токен>`.

---

## 4. Конфигурация `.env`

Файл `.env` лежит рядом с сервисом (`mcp/.env`) и в git не попадает. Переменные окружения
приоритетнее файла (это важно для Docker: `env_file`/`environment` не перетираются файлом).
Обязательные переменные проверяются **до** открытия порта, иначе процесс падает с русским
сообщением (`ConfigError`).

### Обязательные

| Переменная | Назначение |
|---|---|
| `SSVC_DEVICE_URL` | Базовый URL контроллера без завершающего `/`, например `http://192.168.1.42`. Прошивка слушает только HTTP, порт 80 |
| `SSVC_USER` | Логин контроллера; нужен пользователь **admin**: `/rest/telemetry`, `/rest/sensor`, `/rest/settings` отдают данные только администратору (иначе `forbidden_admin_required`) |
| `SSVC_PASSWORD` | Пароль контроллера. Несекретная сборка (security off) работает и с пустым паролем |
| `SSVC_MCP_TOKEN` | Bearer-токен MCP-клиентов. Без него сервер **не стартует**: открытый порт без аутентификации недопустим |

Генерация токена:

```bash
openssl rand -hex 32
# или без openssl:
node -e "console.log(require('crypto').randomBytes(32).toString('hex'))"
```

### Необязательные

| Переменная | По умолчанию | Назначение |
|---|---|---|
| `SSVC_DEVICE_TIMEOUT_MS` | `10000` | Таймаут одного HTTP-запроса к контроллеру, мс (100…600000). Скачивание журнала/файла имеет свой бюджет (45/60 с) и этой переменной не режется |
| `SSVC_POLL_HZ` | `1` | Частота фонового опроса состояния, Гц (0.1…10). Выше 2 Гц бессмысленно: прошивка обновляет снимок примерно раз в две секунды |
| `SSVC_MCP_BIND` | `0.0.0.0` | Адрес прослушивания. `127.0.0.1` — только локально (рекомендуется при работе за прокси) |
| `SSVC_MCP_PORT` | `8080` | Порт сервиса (1…65535) |
| `SSVC_MCP_ALLOWED_HOSTS` | пусто | Разрешённые значения `Host` через запятую (защита от DNS-rebinding). Пусто = localhost/127.0.0.1/[::1] + адрес из `SSVC_MCP_BIND`. При `0.0.0.0` укажите явно, например `192.168.1.10,openconnect.local` |
| `SSVC_MCP_ALLOWED_CIDRS` | `private` | Кто может подключаться: `private` = RFC1918 (10/8, 172.16/12, 192.168/16) + петлевые адреса, либо список CIDR |
| `SSVC_MCP_MAX_SESSIONS` | `32` | Лимит одновременных MCP-сессий (при исчерпании `initialize` отвечает 503) |
| `SSVC_MCP_IDLE_MS` | `1800000` | Закрытие простаивающей сессии, мс (30 мин) |
| `SSVC_MCP_TLS_CERT` / `SSVC_MCP_TLS_KEY` | пусто | Встроенный TLS (нужны оба файла). По умолчанию TLS нет — наружу только через reverse-proxy (§7) |
| `SSVC_ALLOW_DESTRUCTIVE` | `0` | Разрушительные операции (риск 3/4). `0` — инструменты отвечают `disabled_by_policy` (§10) |
| `SSVC_ALLOW_DEBUG` | `0` | Разрешить `debug:true` в вызовах (в ответ добавляется блок `raw`) |
| `SSVC_RAW_LOG` | `off` | Сырые обмены с устройством: `off` / `errors` (только ≥400 и таймауты) / `all` |
| `SSVC_RAW_LOG_DIR` | `./logs/raw` | Каталог сырых логов (JSONL по дням, лимит объёма) |
| `SSVC_RAW_LOG_MAX_MB` | `50` | Предел объёма каталога сырых логов, МБ |
| `SSVC_AUDIT_PATH` | `./logs/audit.jsonl` | Журнал аудита опасных операций (JSONL) |
| `SSVC_LOG_LEVEL` | `info` | Уровень логов в stdout: `debug`/`info`/`warn`/`error`/`silent` |

`SSVC_RAW_LOG_DIR`, `SSVC_AUDIT_PATH` и каталог скачивания (`logs/downloads`) создаются
сервисом при необходимости. В контейнере каталог `/app/logs` смонтирован на хост (§7).

---

## 5. Запуск и эндпоинты

```bash
node dist/index.js          # или npm start, или npm run mock для мока-устройства (§8)
```

| Маршрут | Назначение |
|---|---|
| `POST /mcp` | JSON-RPC MCP-запроса (Streamable HTTP). `GET /mcp` — SSE-поток уведомлений, `DELETE /mcp` — завершение сессии |
| `GET /healthz` | «Процесс жив», **без токена** — для HEALTHCHECK контейнера и мониторинга |
| `GET /readyz` | С токеном: проверяет `GET /rest/oc/info` на контроллере, отдаёт версии и `compatibility` |

Сессии **stateful**: подписки на ресурсы и уведомления привязаны к `Mcp-Session-Id`.
Неизвестная сессия — `404` (клиент пересоздаёт), не-init запрос без `Mcp-Session-Id` — `400`,
`initialize` при исчерпанном лимите — `503`. Допуск по `Host`/`Origin` защищает от DNS-rebinding.

Логи — структурный JSON в stdout (Docker/systemd собирают штатно), секреты вырезаются.
Опасные операции дополнительно пишутся в аудит (JSONL): `tool`, `risk`, `confirm`, `reason`, `result`.

Остановка: `SIGINT`/`SIGTERM` — сервис закрывает сессии, HTTP, аудит и сырые логи.

---

## 6. Подключение клиентов

### curl: полный цикл вручную

```bash
TOKEN=<ваш SSVC_MCP_TOKEN>
BASE=http://127.0.0.1:8080

# 1) initialize — в ответе придёт заголовок Mcp-Session-Id
curl -i -X POST "$BASE/mcp" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"curl","version":"1.0.0"}}}'

# 2) уведомление об инициализации (после него сервер принимает обычные запросы)
curl -X POST "$BASE/mcp" -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" -H "Mcp-Session-Id: <ID>" \
  -d '{"jsonrpc":"2.0","method":"notifications/initialized"}'

# 3) список инструментов (50) и промптов (5)
curl -X POST "$BASE/mcp" -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" -H "Mcp-Session-Id: <ID>" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list"}'

# 4) вызов инструмента
curl -X POST "$BASE/mcp" -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" -H "Mcp-Session-Id: <ID>" \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"oc_get_state","arguments":{}}}'

# 5) завершить сессию
curl -X DELETE "$BASE/mcp" -H "Authorization: Bearer $TOKEN" -H "Mcp-Session-Id: <ID>"
```

Без токена любой запрос к `/mcp` — `401` + `WWW-Authenticate: Bearer`.

### MCP Inspector

```bash
npx @modelcontextprotocol/inspector
```

В открывшемся UI: **Transport = Streamable HTTP**, **URL = `http://127.0.0.1:8080/mcp`**,
и в **Headers** добавьте `Authorization: Bearer <токен>`. Вкладки Tools/Prompts/Resources
показывают каталог без ручных запросов.

### Claude Desktop (клиенты без нативного HTTP)

Claude Desktop не умеет Streamable HTTP напрямую — нужен мост `mcp-remote`
(`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "openconnect": {
      "command": "npx",
      "args": [
        "-y", "mcp-remote",
        "http://127.0.0.1:8080/mcp",
        "--header", "Authorization: Bearer ${SSVC_MCP_TOKEN}"
      ],
      "env": { "SSVC_MCP_TOKEN": "<ваш токен>" }
    }
  }
}
```

Другие клиенты: любой, поддерживающий Streamable HTTP + статический Bearer-заголовок
(Cursor, Continue, собственный SDK-клиент). Пример SDK-клиента — `test/e2e/http.e2e.test.ts`.

---

## 7. Docker

Образ **multi-stage** (`Dockerfile`): сборка в `node:22-alpine` → runtime-слой только с
`dist/`, production-зависимостями и non-root пользователем `mcp`. `HEALTHCHECK` дергает
`GET /healthz` (без curl/wget — через встроенный `fetch`), `EXPOSE 8080`.

```bash
cd mcp
cp .env.example .env        # заполнить, см. §4
docker compose up -d --build
docker compose logs -f
curl http://127.0.0.1:8080/healthz
```

`docker-compose.yml`:

- `env_file: .env` — секреты не попадают в образ;
- `SSVC_MCP_BIND=0.0.0.0` внутри контейнера (иначе проброс порта не работает);
- порт публикуется только на `127.0.0.1` хоста;
- `./logs` смонтирован в `/app/logs` (сырые логи, аудит, скачанные журналы);
- `restart: unless-stopped`, `no-new-privileges`.

Ручная сборка без compose:

```bash
docker build -t ssvc-openconnect-mcp:0.1.0 .
docker run --rm -p 127.0.0.1:8080:8080 --env-file .env \
  -e SSVC_MCP_BIND=0.0.0.0 -v "${PWD}/logs:/app/logs" ssvc-openconnect-mcp:0.1.0
```

**TLS.** Встроенного TLS по умолчанию нет (в контейнере — тем более). Наружу сервис
выставляется через reverse-proxy с сертификатом, например Caddy:

```caddyfile
mcp.example.com {
    reverse_proxy 127.0.0.1:8080
}
```

Caddy сам получит сертификат и будет проксировать `/mcp` (SSE при этом работает: прокси
не должен буферизовать ответы). Если нужен TLS без прокси — задайте
`SSVC_MCP_TLS_CERT`/`SSVC_MCP_TLS_KEY` (оба файла сразу).

---

## 8. Мок-устройство

`mock/deviceServer.ts` — HTTP-двойник прошивки на `node:http` (без новых зависимостей):
те же маршруты, что в §2.4 `mcp-server.md`, правдоподобные данные, журнал команд с
`commandId`, сессия передачи журнала (первый ответ `202`, второй — CSV).

```bash
npm run mock        # соберёт mock в .test-dist и запустит на 127.0.0.1:8099
# в другом терминале: тот же сервис, но смотрит на мок
# .env: SSVC_DEVICE_URL=http://127.0.0.1:8099, SSVC_USER=admin, SSVC_PASSWORD=admin
node dist/index.js
```

Переменные мока:

| Переменная | По умолчанию | Назначение |
|---|---|---|
| `MOCK_HOST` / `MOCK_PORT` | `127.0.0.1` / `8099` | Где слушать (`MOCK_PORT=0` — свободный порт) |
| `MOCK_SECURITY` | `on` | `off` — не-secure сборка: `/rest/signIn` отвечает `404`, клиент работает анонимно |
| `MOCK_USER` / `MOCK_PASSWORD` | — | Если заданы, `signIn` проверяет учётные данные |
| `MOCK_STAGE_SWITCH_MS` | `1500` | Через сколько мс стадия сменится `waiting` → `heads` (`0` — не менять; так делает e2e) |

Что покрыто: `/rest/signIn`, `/rest/verifyAuthorization`, `/rest/oc/info`, `/rest/oc/snapshot`,
`/rest/features`, `/rest/telemetry`, `/rest/sensor`, `/rest/zones` (GET/POST),
`/rest/sensors/zone`, `/rest/alarms` (GET/POST), `/rest/settings` (GET/PUT),
`/rest/commands` (+`/{id}`), `/rest/oc/xkc25/{status,verify-status,calibrate}`,
`/rest/oc/relay/{capabilities,metadata,state,override}` (GPIO-сборка: `supported:false`),
`/rest/oc/hardware-config` (GET/PUT), `/rest/oc/hardware-faults`, `/rest/files` (+файл, copy,
delete), `/rest/subsystem` (GET/PUT), `/rest/systemStatus`, `/rest/restart`, `/rest/logs`
(+`/{id}`, DELETE), `/rest/profiles` (+`/active`, `/content`), `/healthz`.

Мок — инструмент разработки и тестов: в Docker-образ он не попадает (`.dockerignore`).

---

## 9. Каталог инструментов (50)

Риск: **0** — чтение, **1** — обратимая запись, **2** — влияет на процесс (`confirm`),
**3** — разрушительное (`confirm` + `reason` + `SSVC_ALLOW_DESTRUCTIVE=1`), **4** — то же +
отдельный allowlist (сейчас пуст). Имена и аннотации собирает `src/tools/index.ts`;
`tools/list` показывает ровно этот каталог.

| Домен | Инструмент | Что делает | Риск |
|---|---|---|---|
| info | `oc_get_info` | Версии прошивки и UART API, совместимость, реестр возможностей | 0 |
| info | `oc_get_capabilities` | Возможности API + рантайм-флаги сборки (`/rest/features`) | 0 |
| info | `oc_diagnostics` | Самодиагностика сервера: конфиг без секретов, авторизация, счётчики | 0 |
| telemetry | `oc_get_state` | Снимок состояния одним запросом `/rest/oc/snapshot` | 0 |
| telemetry | `oc_get_telemetry` | Телеметрия `/rest/telemetry` (+датчики по флагу) | 0 |
| telemetry | `oc_get_process_status` | Стадия, статус, стадии процесса, ошибка UART | 0 |
| commands | `oc_start` | Запустить ректификацию | 2 |
| commands | `oc_pause` | Пауза отбора | 2 |
| commands | `oc_resume` | Продолжить отбор | 2 |
| commands | `oc_next` | Перейти к следующей фракции | 2 |
| commands | `oc_stop` | Остановить процесс | 2 |
| commands | `oc_emergency_stop` | Аварийная остановка — **без подтверждения** (защитное действие) | 2* |
| commands | `oc_send_command` | Служебная команда из белого списка | 2 |
| commands | `oc_wait_for_change` | Дождаться изменения состояния (опрос, таймаут — не ошибка) | 0 |
| settings | `oc_get_settings` | Действующие настройки SSVC | 0 |
| settings | `oc_update_settings` | Изменить настройки (плоский PUT, отчёт `ignored`/`skipped`) | 1 |
| sensors | `oc_get_sensors` | Показания датчиков по зонам | 0 |
| sensors | `oc_get_zones` | Привязка адресов 1-Wire к зонам | 0 |
| sensors | `oc_set_zone` | Привязать датчик к зоне | 1 |
| sensors | `oc_get_alarms` | Пороги тревог | 0 |
| sensors | `oc_set_alarm_thresholds` | Изменить пороги тревог | 1 |
| relay | `oc_get_relay_metadata` | Справочник реле PCF8574 | 0 |
| relay | `oc_get_relay_state` | Состояние реле (тень пользователя) | 0 |
| relay | `oc_relay_override` | Ручное переключение реле | 2 |
| hardware | `oc_get_hardware_config` | Аппаратная конфигурация, пробы, `needs_reboot` | 0 |
| hardware | `oc_update_hardware_config` | Изменить конфигурацию (частичный PUT) | 1 |
| hardware | `oc_get_hardware_faults` | Аппаратные сбои I²C/датчиков | 0 |
| xkc25 | `oc_get_xkc25_status` | Датчики уровня XKC-25 и предохранитель старта | 0 |
| xkc25 | `oc_xkc25_calibrate` | Шаг калибровки (без жидкости/с жидкостью/проверка возврата) | 1 |
| xkc25 | `oc_xkc25_verify_status` | Результат асинхронной проверки возврата | 0 |
| logs | `oc_logs_list` | Список журналов ректификации | 0 |
| logs | `oc_logs_download` | Скачать журнал в файл на сервере (CSV не идёт в контекст) | 0 |
| logs | `oc_logs_cancel` | Прервать передачу журнала | 1 |
| profiles | `oc_profiles_list` | Список профилей | 0 |
| profiles | `oc_profile_get` | Профиль целиком | 0 |
| profiles | `oc_profile_create` | Создать профиль | 1 |
| profiles | `oc_profile_copy` | Скопировать профиль | 1 |
| profiles | `oc_profile_rename` | Переименовать профиль | 1 |
| profiles | `oc_profile_update_content` | Обновить содержимое профиля | 1 |
| profiles | `oc_profile_save_current` | Сохранить текущие настройки в профиль | 1 |
| profiles | `oc_profile_apply` | Применить профиль | 2 |
| profiles | `oc_profile_delete` | Удалить профиль | 3 |
| files | `oc_files_list` | Дерево файлов LittleFS | 0 |
| files | `oc_file_download` | Скачать файл в каталог сервера | 0 |
| files | `oc_file_copy` | Скопировать файл внутри контроллера | 1 |
| files | `oc_file_delete` | Удалить файл | 3 |
| system | `oc_get_subsystems` | Состояние подсистем (I²C, thermal, …) | 0 |
| system | `oc_set_subsystem` | Включить/выключить подсистему | 1 |
| system | `oc_system_status` | Память, uptime, версия, данные чипа | 0 |
| system | `oc_restart` | Перезагрузить контроллер | 3 |

`*` `oc_emergency_stop` не требует подтверждения: политика риска для него не применяется
(`RISK_EXEMPT_TOOLS`), это защитное действие.

Промпты (5): `start_rectification` («Запустить ректификацию»), `troubleshoot_start_blocked`
(«Разобрать блокировку старта»), `calibrate_xkc25` («Откалибровать датчик XKC-25»),
`analyze_last_run` («Разобрать последний прогон»), `apply_profile` («Применить профиль») —
готовые последовательности шагов для агента (тексты — в `src/prompts/prompts.ts`).
Ресурсы: `openconnect://info`, `openconnect://telemetry`.

---

## 10. Разрушительные операции: как включить осознанно

По умолчанию (`SSVC_ALLOW_DESTRUCTIVE=0`) инструменты риска 3 отвечают `disabled_by_policy`
**до любого запроса к контроллеру**. Это проверяется тестом `test/release.test.ts` для
каждого такого инструмента (список вычисляется по поведению политики, а не хардкодится).

Порядок проверок (см. `src/policy/confirmation.ts`):

1. `confirm: true` — иначе `confirmation_required`;
2. `reason` не короче 10 символов — иначе `validation_failed` (причина попадает в аудит);
3. `SSVC_ALLOW_DESTRUCTIVE=1` — иначе `disabled_by_policy`;
4. для риска 4 — ещё и allowlist (сейчас пуст: таких инструментов нет).

Чтобы действительно разрешить (например, `oc_restart` на стенде):

```dotenv
SSVC_ALLOW_DESTRUCTIVE=1
```

```json
{"name":"oc_restart","arguments":{"confirm":true,"reason":"стенд: перезагрузка после прошивки"}}
```

и перезапустить сервис. Не включайте флаг на живом процессе ректификации без оператора рядом.

Инструменты риска 2 (`oc_start`, `oc_stop`, `oc_relay_override`, `oc_profile_apply`,
`oc_send_command`) требуют только `confirm:true` и работают при выключенном флаге.

---

## 11. Решение по `outputSchema`

Инструменты **не объявляют `outputSchema`** (кроме тех, где схема реально полезна).
Причины и следствия:

- без `outputSchema` SDK не проверяет `structuredContent`, поэтому конверт ошибки
  (`{ok:false,error:{...}}`) можно отдавать и в `structuredContent`, и в текстовой части;
- с `outputSchema` сервер пропускает проверку при `isError:true`, а **клиент SDK — нет**:
  конверт ошибки не соответствует успешной схеме и был бы отвергнут. Поэтому у инструментов
  со схемой код ошибки остаётся только в `content[0].text` (тот же JSON);
- ответы прошивки версионны (`API 1.7`…`1.10`), закрытая схема вывода ломалась бы на
  расхождениях. Вход валидируется строго (`inputSchema`), выход — на уровне домена (zod-схемы
  ответов + `device_error` с телом при расхождении).

Итог: клиент получает и текст (JSON), и `structuredContent`; при ошибке — `isError:true`,
чтобы «отказ» не был принят за данные. Подробности — `src/mcp/registry.ts`.

---

## 12. Ограничения и известные особенности прошивки

| Ограничение | Что это значит в работе |
|---|---|
| **Один контроллер** | `SSVC_DEVICE_URL` — единственный адрес; параметра `device` в инструментах нет |
| **HTTP до устройства без TLS** | Трафик MCP-сервер → контроллер идёт по локальной сети открытым. Пароль читается только из `.env`, JWT живёт в памяти. Это ограничение прошивки, а не сервера |
| **Нет встроенного TLS для клиентов** | По умолчанию сервис слушает HTTP. Наружу — только через reverse-proxy с сертификатом (§7). `SSVC_MCP_TLS_CERT`/`_KEY` включают встроенный TLS, если прокси не нужен |
| **SPA-заглушка** | Неизвестный GET прошивка отдаёт как `200 text/html`. Сервер распознаёт это (`src/domain/readJson.ts`) и отвечает `feature_unsupported` с подсказкой «обновите прошивку», а не пустыми данными |
| **`/rest/files` перекрыт деревом** | В проверенных ревизиях `GET /rest/files/*` перехватывается обработчиком дерева, поэтому `oc_file_download` отвечает `feature_unsupported` («маршрут перекрыт»). Список файлов и копирование работают; скачивание — через веб-интерфейс контроллера |
| **Журналы без SD** | Если SD-карты нет, передача журнала завершается кодом `err_mnt` — сервер отдаёт `device_error` с русским объяснением (`oc_logs_download`) |
| **Реле на GPIO-сборке** | `oc_get_relay_metadata`/`oc_get_relay_state`/`oc_relay_override` отвечают `feature_unsupported` («Реле на PCF8574 недоступны в этой сборке прошивки») — это данные, а не сбой |
| **Одна передача журнала** | Вторая одновременная попытка получает `busy`/`log_transfer_busy`; `oc_logs_cancel` освобождает сессию |
| **Асинхронные команды** | Успех `oc_start` и подобных — «принято в очередь/UART» (`confidence:"transport"`). Фактическое выполнение проверяйте `oc_get_state`/`oc_wait_for_change` |
| **Данные снимка отстают** | Прошивка обновляет телеметрию ~раз в 2 с; `stale:true` появляется, если `lastUpdate` не менялся дольше 10 с |
| **`lastUpdate` — не epoch** | Это `millis()` устройства (uptime). Сравнивать с часами сервера нельзя: свежесть считается по монотонному времени получения |
| **Формат числа** | Прошивка отдаёт числа и строками (`"766.9"`): сервер нормализует, не теряя точности |

---

## 13. Тесты, мок и CI

| Уровень | Что покрывает |
|---|---|
| Юнит/контракт (`test/*.test.ts`) | Домены и инфраструктура (215 тестов): политика риска, реестр, HTTP-слой, схемы, промпты, ресурсы |
| Каталог (`test/toolsIndex.test.ts`) | 50 уникальных имён, обязательные поля `ToolDef`, согласованность аннотаций, защита от дубликатов, наличие помеченных разрушительных инструментов |
| Релизная закалка (`test/release.test.ts`) | Безопасные значения `.env.example` и умолчаний кода; каждый инструмент риска ≥3 отвечает `disabled_by_policy`, а с флагом — доходит до устройства; без `reason` — `validation_failed` |
| E2E (`test/e2e/http.e2e.test.ts`) | Настоящий SDK-клиент: `initialize` → `tools/list` (50) → `prompts/list` (5) → чтения → `oc_wait_for_change` (мок меняет стадию) → 401 без токена → `disabled_by_policy` → закрытие сессии. Порты только 0 |

CI (`.github/workflows/mcp.yaml`): `ubuntu-latest`, `node 22`, `npm ci --ignore-scripts`,
`npm run typecheck`, `npm test`, `npm run build`.

Ручная проверка на живом контроллере: чтения (`oc_get_info`, `oc_get_state`, `oc_get_settings`,
`oc_get_sensors`, `oc_get_alarms`, `oc_get_xkc25_status`, `oc_get_hardware_faults`,
`oc_get_relay_metadata`, `oc_get_subsystems`, `oc_system_status`, `oc_logs_list`,
`oc_files_list`) — на GPIO-сборке реле честно отвечает `feature_unsupported`.

---

## 14. Диагностика

| Симптом | Что проверить |
|---|---|
| Сервер не стартует | Сообщение `Ошибка конфигурации`: не задан `SSVC_MCP_TOKEN`/`SSVC_DEVICE_URL`, неверный диапазон переменной |
| `401` от `/mcp` | Заголовок `Authorization: Bearer <SSVC_MCP_TOKEN>` (токен из `.env`, не пароль контроллера) |
| `403 forbidden_admin_required` | `SSVC_USER` не admin: телеметрия и датчики доступны только администратору |
| `auth_failed` | Неверный `SSVC_USER`/`SSVC_PASSWORD`; на не-secure сборке пароль лучше не задавать лишним |
| `device_unreachable` / `device_timeout` | Контроллер в сети? `curl http://<ip>/rest/oc/info`. Увеличьте `SSVC_DEVICE_TIMEOUT_MS` |
| `feature_unsupported` | Маршрута нет в этой прошивке (SPA-заглушка/404) — обновите OpenConnect или не используйте инструмент |
| `disabled_by_policy` | Разрушительная операция при `SSVC_ALLOW_DESTRUCTIVE=0` (§10) |
| `confirmation_required` | Нужен `confirm:true` (риск 2/3) |
| `guard_blocked` | Предохранитель SSVC: причины в `error.details.block_reasons` (уровень XKC-25, сбои) |
| `busy`/`queue_full` | Контроллер занят: повторите позже или отмените передачу журнала (`oc_logs_cancel`) |
| Непонятный ответ устройства | `oc_diagnostics` (счётчики, авторизация), затем `SSVC_RAW_LOG=errors` и повтор вызова |

`GET /readyz` (с токеном) — самый быстрый способ проверить связку «сервис → контроллер».

---

## 15. Структура пакета

```
mcp/
├── Dockerfile / docker-compose.yml / .dockerignore   # образ: multi-stage, non-root, healthcheck
├── .env.example                                      # безопасные релизные значения
├── src/
│   ├── index.ts                 # запуск: конфиг → зависимости → HTTP
│   ├── config.ts, logging.ts, errors.ts, audit.ts, rawlog.ts
│   ├── http/                    # app.ts (маршруты /mcp /healthz /readyz), auth, sessions, tls
│   ├── mcp/                     # server.ts (на сессию), registry.ts (ToolDef → MCP), types.ts
│   ├── device/                  # DeviceClient, AuthSession, CapabilityCache, WriteQueue, schemas/
│   ├── domain/                  # доменные сервисы + readJson.ts (единый разбор JSON/SPA)
│   ├── tools/                   # *.tools.ts по доменам + index.ts (единый каталог)
│   ├── resources/, prompts/     # openconnect://… и 5 промптов-сценариев
│   └── policy/                  # risk.ts (классы), confirmation.ts (confirm/reason/флаг)
├── mock/deviceServer.ts         # HTTP-двойник прошивки (npm run mock)
└── test/                        # доменные, транспортные, каталог, релизные, e2e/
```

---

## 16. Ссылки

- [Архитектура и план MCP-сервера](../../docs/develop/mcp-server.md)
- [Разбивка на пакеты работ](../../docs/develop/mcp-server-agents.md)
- [Контракт прошивки (mcp-contract-notes.md)](../../docs/develop/mcp-contract-notes.md)
- [MCP: Streamable HTTP транспорт](https://modelcontextprotocol.io/specification)
