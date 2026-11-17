# syntax=docker/dockerfile:1
# MCP-сервер OpenConnect: multi-stage сборка (build → runtime).
#
# Почему два этапа: typescript, @types и тестовые каталоги в образ не нужны.
# Runtime-слой содержит только скомпилированный `dist/`, production-зависимости
# (`@modelcontextprotocol/sdk`, `zod`) и запускается от non-root пользователя.
#
# Проверка живости — `GET /healthz` (без токена, mcp-server.md §3.4): она отвечает
# «процесс жив», а не «контроллер доступен» (для второго есть `/readyz` с токеном).

# ---------------------------------------------------------------------------
# Этап 1: сборка
# ---------------------------------------------------------------------------
FROM node:22-alpine AS build

WORKDIR /app

# Зависимости — отдельным слоем: при правке исходников npm ci не перезапускается.
COPY package.json package-lock.json ./
RUN npm ci --ignore-scripts

# Компиляция: только tsconfig и src (тесты и мок в образ не нужны).
COPY tsconfig.json ./
COPY src ./src
RUN npm run build \
    && npm prune --omit=dev

# ---------------------------------------------------------------------------
# Этап 2: runtime
# ---------------------------------------------------------------------------
FROM node:22-alpine AS runtime

ENV NODE_ENV=production \
    SSVC_MCP_BIND=0.0.0.0 \
    SSVC_MCP_PORT=8080

WORKDIR /app

# Non-root пользователь: сервис не должен уметь писать вне своих каталогов.
RUN addgroup -S mcp && adduser -S -G mcp -h /app mcp

COPY --from=build --chown=mcp:mcp /app/node_modules ./node_modules
COPY --from=build --chown=mcp:mcp /app/dist ./dist
COPY --chown=mcp:mcp package.json ./

# Каталоги для сырых логов, аудита и скачанных файлов: владелец — mcp.
RUN mkdir -p /app/logs/raw /app/logs/downloads && chown -R mcp:mcp /app/logs

USER mcp

EXPOSE 8080

# HEALTHCHECK без curl/wget: в alpine-образе Node их нет, а fetch есть в самом рантайме.
HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
  CMD node -e "fetch('http://127.0.0.1:' + (process.env.SSVC_MCP_PORT || '8080') + '/healthz').then((r) => process.exit(r.ok ? 0 : 1)).catch(() => process.exit(1))"

CMD ["node", "dist/index.js"]
