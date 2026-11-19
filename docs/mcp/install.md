# Установка и обновление

Три способа поставить MCP-сервер: Docker-образ из GHCR, tarball из GitHub Release (ставится ссылкой
через npm) и сборка из исходников. Для всех способов нужен `.env` — см. [Настройка](configuration.md).

## Docker (рекомендуется)

Образы публикуются в GitHub Container Registry:

```
ghcr.io/ssvc0059/ssvc_open_connect/mcp:<версия>
```

Теги:

- `1.0.0` — конкретный релиз (рекомендуется: воспроизводимо);
- `sha-<commit>` — сборка из конкретного коммита;
- `latest` — последний не-prerelease релиз.

```bash
docker run -d --name ssvc-mcp --restart unless-stopped \
  -p 127.0.0.1:8080:8080 \
  --env-file .env \
  -v "$PWD/logs:/app/logs" \
  ghcr.io/ssvc0059/ssvc_open_connect/mcp:1.0.0
```

Порт публикуется только на петлевой адрес хоста: наружу сервис выставляется через
reverse-proxy с TLS (§ «Обратный прокси»), а не прямым пробросом на все интерфейсы.

Внутри образа уже заданы `SSVC_MCP_BIND=0.0.0.0` и `SSVC_MCP_PORT=8080`, запуск — от непривилегированного
пользователя, журналы и скачанные файлы — в `/app/logs` (том обязателен, иначе данные исчезнут с
контейнером).

### docker compose

```yaml
services:
  mcp:
    image: ghcr.io/ssvc0059/ssvc_open_connect/mcp:1.0.0
    restart: unless-stopped
    ports:
      - "8080:8080"
    env_file: .env
    volumes:
      - ./logs:/app/logs
    security_opt:
      - no-new-privileges:true
```

```bash
docker compose up -d
docker compose logs -f mcp
```

## Из GitHub Release по ссылке

Релизы содержат установочный tarball `ssvc-openconnect-mcp-<версия>.tgz` и контрольную сумму.
Устанавливается прямо по ссылке на GitHub:

```bash
npm install -g https://github.com/SSVC0059/ssvc_open_connect/releases/download/mcp-v1.0.0/ssvc-openconnect-mcp-1.0.0.tgz
```

После установки доступны команды `ssvc-mcp` и `ssvc-openconnect-mcp`:

```bash
ssvc-mcp            # читает .env из текущего каталога или переменные окружения
```

Проверить целостность:

```bash
curl -LO https://github.com/SSVC0059/ssvc_open_connect/releases/download/mcp-v1.0.0/ssvc-openconnect-mcp-1.0.0.tgz
curl -LO https://github.com/SSVC0059/ssvc_open_connect/releases/download/mcp-v1.0.0/ssvc-openconnect-mcp-1.0.0.tgz.sha256
sha256sum -c ssvc-openconnect-mcp-1.0.0.tgz.sha256
```

### Релизы с отдельных веток

Ветка `main` даёт обычные релизы (`latest` обновляется). Ручной запуск с любой другой ветки без тега
публикует **prerelease** с версией вида `0.0.0-<ветка>.<номер-запуска>`; ставится так же — по ссылке
на этот prerelease. Релиз, созданный по тегу `mcp-v*` (в том числе с рабочей ветки), считается обычным,
если при ручном запуске не отмечен флажком prerelease. Prerelease-сборки не затрагивают тег `latest`.

## Из исходников

```bash
git clone https://github.com/SSVC0059/ssvc_open_connect.git
cd ssvc_open_connect/mcp
npm ci
npm run build
node dist/index.js
```

## Проверка после установки

```bash
curl -s http://127.0.0.1:8080/healthz          # {"ok":true,"status":"alive",...} — процесс жив
curl -s -H "Authorization: Bearer $TOKEN" \
     http://127.0.0.1:8080/readyz              # контроллер доступен и авторизация прошла
```

`healthz` не требует токена и ничего не говорит о контроллере; `readyz` проверяет связь с ним.

## Обновление и откат

- Docker: поменяйте тег на новую версию и `docker compose up -d`; откат — вернуть прежний тег.
- npm-установка: повторите `npm install -g <ссылка на новый релиз>`; откат — ссылка на старый релиз.
- Данные (`logs/`, аудит) не входят в образ и переживают обновление.

Перед обновлением сверьтесь с [CHANGELOG](../info/changelog.md): несовместимые изменения фиксируются там.
