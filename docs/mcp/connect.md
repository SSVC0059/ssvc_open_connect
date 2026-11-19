# Подключение клиентов

Сервер поддерживает два транспорта:

- **Streamable HTTP** — для удалённого доступа и контейнера: `POST /mcp`, `Authorization: Bearer ...`;
- **stdio** — для клиентов, которые сами запускают процесс (DSH, Claude Desktop, IDE):
  `node dist/index.js --stdio`, без порта и токена, лог уходит в stderr.

## Streamable HTTP

Точка входа — `POST /mcp`; дополнительно `GET /mcp` открывает
поток уведомлений для существующей сессии, `DELETE /mcp` закрывает её.

- URL: `http://<host>:8080/mcp`
- заголовок: `Authorization: Bearer <SSVC_MCP_TOKEN>`
- заголовок `Accept` должен разрешать и JSON, и поток: `application/json, text/event-stream`

Сессии живут по `Mcp-Session-Id`, который сервер возвращает в ответе на `initialize`; клиент обязан
передавать его дальше. Простаивающие сессии закрываются через `SSVC_MCP_IDLE_MS` (по умолчанию 30 минут).

## DSH и другие stdio-клиенты

```bash
cd mcp
npm run build
node dist/index.js --stdio
```

Клиент должен запускать эту команду как дочерний процесс и общаться через stdin/stdout.
Обязательна только `SSVC_DEVICE_URL`; `SSVC_MCP_TOKEN` в stdio не нужен, а `SSVC_PASSWORD`
требуется лишь при включённой на прошивке авторизации. Рабочий каталог — `mcp/`, чтобы
сервер прочитал `mcp/.env` с адресом и учётными данными контроллера.

Для DSH сервер добавляется как stdio-клиент (**Plugins → Add plugin**): команда
`node dist/index.js --stdio`, рабочий каталог `mcp/`; после этого инструменты видны как
`mcp__openconnect__oc_*` в новой сессии.

## Claude Desktop

Claude Desktop умеет запускать stdio-серверы напрямую:

```json
{
  "mcpServers": {
    "openconnect": {
      "command": "node",
      "args": ["C:\\\\path\\\\to\\\\mcp\\\\dist\\\\index.js", "--stdio"],
      "cwd": "C:\\\\path\\\\to\\\\mcp"
    }
  }
}
```

Если сервер развёрнут по HTTP, используйте мост `mcp-remote`:

```json
{
  "mcpServers": {
    "openconnect": {
      "command": "npx",
      "args": [
        "-y",
        "mcp-remote",
        "http://192.168.1.10:8080/mcp",
        "--header",
        "Authorization: Bearer ВАШ_ТОКЕН"
      ]
    }
  }
}
```

Файл конфигурации: `claude_desktop_config.json` (macOS — `~/Library/Application Support/Claude/`,
Windows — `%APPDATA%\Claude\`). После правки перезапустите Claude Desktop.

## MCP Inspector

```bash
npx @modelcontextprotocol/inspector
```

В интерфейсе: Transport — **Streamable HTTP**, URL — `http://<host>:8080/mcp`, в разделе
Authentication/Headers добавьте `Authorization: Bearer <токен>`. Inspector покажет список из 51
инструмента, промпты и ресурсы.

## Проверка вручную (curl)

```bash
TOKEN=ваш_токен
curl -sS -D - -X POST http://127.0.0.1:8080/mcp \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{
        "protocolVersion":"2025-06-18",
        "capabilities":{},
        "clientInfo":{"name":"curl","version":"1.0"}}}'
```

В ответе будет `Mcp-Session-Id`. Дальше:

```bash
SID=<значение из ответа>
curl -sS -X POST http://127.0.0.1:8080/mcp \
  -H "Authorization: Bearer $TOKEN" \
  -H "Mcp-Session-Id: $SID" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","method":"notifications/initialized"}'

curl -sS -X POST http://127.0.0.1:8080/mcp \
  -H "Authorization: Bearer $TOKEN" \
  -H "Mcp-Session-Id: $SID" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list"}'
```

## Сеть и доступ

- По умолчанию сервер слушает `127.0.0.1`; для доступа из сети задайте `SSVC_MCP_BIND=0.0.0.0`.
- При `0.0.0.0` обязательно перечислите допустимые `Host` в `SSVC_MCP_ALLOWED_HOSTS`
  (например `192.168.1.10,openconnect.local`) — иначе запросы с этих адресов будут отклонены.
- Кто может подключаться, ограничивает `SSVC_MCP_ALLOWED_CIDRS` (по умолчанию `private`).
- Через интернет — только за reverse-proxy с TLS; готовые конфигурации nginx/Caddy —
  в [reverse-proxy](../develop/reverse-proxy.md).

## Клиенты без моста

Клиенты, поддерживающие сетевой MCP напрямую (IDE-агенты, собственные приложения на SDK), подключаются
к `http://<host>:8080/mcp` с тем же Bearer-токеном. Для SDK используйте Streamable HTTP транспорт и
заголовок `Authorization`; `Mcp-Session-Id` SDK ведёт сам.

## Частые вопросы

- **401 `unauthorized`** — нет или неверен `Authorization`; проверьте `SSVC_MCP_TOKEN` на сервере и у клиента.
- **403 `forbidden`** — адрес, `Host` или `Origin` не разрешён; добавьте адрес в `SSVC_MCP_ALLOWED_HOSTS`
  и проверьте `SSVC_MCP_ALLOWED_CIDRS`.
- **400 на `GET /mcp` или `DELETE /mcp`** — нет сессии: сначала `initialize`, затем передавайте
  `Mcp-Session-Id` в каждом запросе.
- **404 на `POST /mcp`** — сессия неизвестна или истекла по `SSVC_MCP_IDLE_MS`; начните с `initialize`.
- **Соединение рвётся на большом ответе** — клиент или прокси режет тело; сервер всегда отправляет
  `Content-Length`, проверьте лимиты прокси.
