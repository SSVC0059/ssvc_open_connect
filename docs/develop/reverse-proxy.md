# Удалённый доступ: TLS через reverse-proxy

Устройство отдаёт **только HTTP** (`AsyncWebServer server(80)`), а MCP-сервер — обычный
HTTP-сервис. Если доступ нужен не только из доверенной локальной сети, шифрование
терминируется на reverse-proxy, а не на ESP32.

## Почему не HTTPS на самом устройстве

В используемой версии `ESPAsyncWebServer` нет `AsyncWebServerSecure`: `beginSecure()`
объявлен под `#if ASYNC_TCP_SSL_ENABLED`, а этот макрос нигде не определён. Плюс
`ESP32SvelteKit` завязан на `AsyncWebServer*`, а WebSocket `/ws/events` пришлось бы
поднимать поверх TLS отдельным стеком. Каталог `ssl_certs/` — это **исходящие** CA-бандлы
для OTA, серверного сертификата там нет.

Поэтому «опциональный HTTPS» — это замена транспорта целиком (новый серверный стек,
сертификат в NVS, правки `main.cpp`/`ESP32SvelteKit`, `wss://` для событий, отдельный
порт и mDNS). Оценка: десятки килобайт RAM на рукопожатие, рост образа; на ESP32-S3
выполнимо, но это отдельная задача, а не флаг сборки.

Документированный путь — reverse-proxy: устройство остаётся HTTP-only в локальной сети,
снаружи — только TLS.

## Схема

```
клиент --HTTPS--> reverse-proxy (nginx/Caddy) --HTTP--> контроллер :80
                       |                                  (локальная сеть)
                       +--HTTPS--> MCP-сервер (например, :3000)
```

Прокси и MCP-сервер удобнее держать на одном хосте (мини-ПК, NAS, Raspberry Pi):
тогда наружу открывается только 443, а порт MCP-сервера и адрес контроллера остаются
внутренними.

## nginx

```nginx
server {
    listen 443 ssl;
    server_name openconnect.example.lan;

    ssl_certificate     /etc/ssl/openconnect/fullchain.pem;
    ssl_certificate_key /etc/ssl/openconnect/privkey.pem;

    # OTA-образ ~2.4 МБ: без этого uploadFirmware вернёт 413
    client_max_body_size 16m;

    # REST
    location / {
        proxy_pass http://192.168.1.50:80;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header Authorization $http_authorization;
    }

    # WebSocket событий: обязателен Upgrade и длинный таймаут (соединение живёт часами)
    location /ws/events {
        proxy_pass http://192.168.1.50:80;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_read_timeout 3600s;
        proxy_send_timeout 3600s;
    }
}

server {
    listen 80;
    server_name openconnect.example.lan;
    return 301 https://$host$request_uri;
}
```

## Caddy

```caddyfile
openconnect.example.lan {
    # внутренний CA для локальных имён: caddy trust
    tls internal
    reverse_proxy 192.168.1.50:80 {
        # Caddy сам проксирует WebSocket, если клиент прислал Upgrade
        header_up Host {host}
        transport http {
            read_timeout 3600s
        }
    }
}
```

## MCP-сервер за тем же прокси

MCP-клиенты ходят к серверу по HTTP (`/mcp`, Streamable HTTP). Если сервер доступен
не только локально, его тоже публикуют через TLS — отдельным `server`-блоком
(nginx) или сайтом (Caddy), с `client_max_body_size` под размер ответов и с
заголовком `Authorization: Bearer <SSVC_MCP_TOKEN>`, который проверяет сам MCP-сервер.
Токен устройства (`SSVC_DEVICE_PASSWORD`) при этом остаётся только в `.env` MCP-сервера
и через прокси наружу не выходит.

## Что проверить после настройки

```bash
# REST по TLS
curl -sS https://openconnect.example.lan/rest/oc/info -H "Authorization: Bearer <jwt>"

# WebSocket: рукопожатие должно вернуть 101 Switching Protocols
curl -sS -i -N -H "Connection: Upgrade" -H "Upgrade: websocket" \
     -H "Sec-WebSocket-Version: 13" -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
     https://openconnect.example.lan/ws/events

# OTA через прокси (проверяет и client_max_body_size, и таймауты)
curl -sS -X POST -F "update=@build/esp32-s3-devkitc-1-16m/SSVC-Open-Connect_esp32-s3-devkitc-1-16m_0-2-9-2.bin;type=application/octet-stream" \
     https://openconnect.example.lan/rest/uploadFirmware
```

## Ограничения и оговорки

- **Сертификат для локального имени.** Публичный CA не выпишет сертификат на
  `*.local`/IP; используйте внутренний CA (Caddy `tls internal`, mkcert) и установите
  его корень на клиентах, либо DNS-имя в зоне, которую вы контролируете.
- **CORS не нужен.** Прокси отдаёт страницу и API с одного origin, браузерные
  preflight-запросы не возникают. Включать CORS в прошивке для этого не требуется
  (и по умолчанию он выключен).
- **Не публикуйте устройство без аутентификации.** Сборка без `FT_SECURITY`
  (`security:false` в `/rest/features`) пускает любого в локальной сети — через
  прокси это стало бы открытым API в интернете. Для внешнего доступа — только
  secure-сборка с сильным паролем и, при возможности, ограничением по IP.
- **Прокси не заменяет предохранители.** Команды `stop`/`emergencyStop` и защита
  старта XKC-25 остаются в прошивке; прокси лишь шифрует транспорт.
- **Один контроллер.** Адрес устройства задаётся в конфигурации прокси и MCP-сервера;
  реестра устройств нет.
