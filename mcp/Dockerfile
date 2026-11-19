# syntax=docker/dockerfile:1
# MCP-сервер OpenConnect: multi-stage сборка (build → runtime).
#
# Контекст сборки — корень репозитория, а не `mcp/`: сервер использует общий пакет
# `packages/rectification` (та же модель, что у интерфейса и документации).
# Поэтому:
#   docker build -f mcp/Dockerfile .
#   docker compose -f mcp/docker-compose.yml up -d --build
#
# Runtime-слой содержит только скомпилированный `dist/` (общая модель уже внутри,
# её включает esbuild), production-зависимости (`@modelcontextprotocol/sdk`, `zod`)
# и запускается от non-root пользователя.

# ---------------------------------------------------------------------------
# Этап 1: сборка
# ---------------------------------------------------------------------------
FROM node:22-alpine AS build

# Путь /app/mcp + /app/packages повторяет раскладку репозитория: зависимость
# `file:../packages/rectification` в package.json разрешается как /app/packages/...
WORKDIR /app/mcp

COPY mcp/package.json mcp/package-lock.json ./
COPY packages/rectification /app/packages/rectification
RUN npm ci --ignore-scripts

COPY mcp/tsconfig.json ./
COPY mcp/scripts ./scripts
COPY mcp/src ./src
RUN npm run build \
    && npm prune --omit=dev

# ---------------------------------------------------------------------------
# Этап 2: runtime
# ---------------------------------------------------------------------------
FROM node:22-alpine AS runtime

# Метаданные образа: версия приходит из CI (`--build-arg VERSION=<release>`), чтобы `docker inspect`
# и GHCR показывали конкретный релиз, а не «latest».
ARG VERSION=dev
LABEL org.opencontainers.image.title="ssvc-openconnect-mcp" \
      org.opencontainers.image.description="MCP-сервер для контроллера OpenConnect (SSVC)" \
      org.opencontainers.image.version="${VERSION}" \
      org.opencontainers.image.source="https://github.com/SSVC0059/ssvc_open_connect" \
      org.opencontainers.image.documentation="https://github.com/SSVC0059/ssvc_open_connect/tree/main/mcp"

ENV NODE_ENV=production \
    SSVC_MCP_BIND=0.0.0.0 \
    SSVC_MCP_PORT=8080

WORKDIR /app

# Non-root пользователь: сервис не должен уметь писать вне своих каталогов.
RUN addgroup -S mcp && adduser -S -G mcp -h /app mcp

COPY --from=build --chown=mcp:mcp /app/mcp/node_modules ./node_modules
COPY --from=build --chown=mcp:mcp /app/mcp/dist ./dist
COPY --chown=mcp:mcp mcp/package.json ./

# Каталоги для сырых логов, аудита и скачанных файлов: владелец — mcp.
RUN mkdir -p /app/logs/raw /app/logs/downloads && chown -R mcp:mcp /app/logs

USER mcp

EXPOSE 8080

# HEALTHCHECK без curl/wget: в alpine-образе Node их нет, а fetch есть в самом рантайме.
HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
  CMD node -e "fetch('http://127.0.0.1:' + (process.env.SSVC_MCP_PORT || '8080') + '/healthz').then((r) => process.exit(r.ok ? 0 : 1)).catch(() => process.exit(1))"

CMD ["node", "dist/index.js"]
