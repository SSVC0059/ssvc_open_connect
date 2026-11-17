/**
 * E2E: настоящий MCP-клиент → HTTP → MCP-сервис → HTTP → мок-устройство (WP11, §15 п.3).
 *
 * Что это доказывает (и чего не доказывают доменные тесты):
 *  - транспорт Streamable HTTP, Bearer-токен, сессии и `tools/list` работают по сети,
 *    а не только в памяти (`test/http.test.ts` смотрит HTTP-слой, доменные тесты — handlers);
 *  - весь каталог реально зарегистрирован: 50 инструментов и 5 промптов видны клиенту;
 *  - основные READ-ONLY инструменты проходят путь до устройства и обратно;
 *  - `oc_wait_for_change` действительно ЖДЁТ изменения стадии, которое мок меняет
 *    в процессе ожидания;
 *  - доступ без токена отбивается `401`, а разрушительная операция при
 *    `SSVC_ALLOW_DESTRUCTIVE=0` — `disabled_by_policy` (политика раньше сети).
 *
 * Порты только `0`: и мок, и сервис просят свободный порт у ОС, поэтому тест не
 * конфликтует с локальными запусками и не занимает 8080. Все серверы закрываются в
 * `t.after`, тест завершается сам.
 */

import assert from 'node:assert/strict';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StreamableHTTPClientTransport } from '@modelcontextprotocol/sdk/client/streamableHttp.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { AuditLog } from '../../src/audit.js';
import { loadConfig } from '../../src/config.js';
import { CapabilityCache } from '../../src/device/CapabilityCache.js';
import { DeviceClient } from '../../src/device/DeviceClient.js';
import { RawLogger } from '../../src/device/RawLogger.js';
import { createApp } from '../../src/http/app.js';
import { silentLogger } from '../../src/logging.js';
import type { ToolContext } from '../../src/mcp/types.js';
import { PROMPT_SPECS } from '../../src/prompts/prompts.js';
import { allTools, uniqueToolNames } from '../../src/tools/index.js';
import { startMockDevice, type MockDevice } from '../../mock/deviceServer.js';
import { payloadOf } from '../riskProbe.js';

/** Токен MCP-клиента в этом тесте (не секрет: живёт только внутри процесса). */
const E2E_MCP_TOKEN = 'e2e-mcp-token-0123456789abcdef';

/** Тело `initialize` для проверки 401 (без токена запрос должен быть отбит до протокола). */
const INITIALIZE_BODY = JSON.stringify({
  jsonrpc: '2.0',
  id: 1,
  method: 'initialize',
  params: {
    protocolVersion: '2025-06-18',
    capabilities: {},
    clientInfo: { name: 'wp11-e2e', version: '1.0.0' },
  },
});

interface Harness {
  device: MockDevice;
  app: Awaited<ReturnType<typeof createApp>>;
  baseUrl: string;
  dir: string;
  close(): Promise<void>;
}

/** Поднимает мок-устройство и MCP-сервис на портах 0 с общим временным каталогом. */
async function startHarness(): Promise<Harness> {
  const dir = await mkdtemp(join(tmpdir(), 'ssvc-mcp-e2e-'));
  // stageSwitchMs: 0 — стадию меняет сам тест (детерминированно), а не таймер мока.
  const device = await startMockDevice({ port: 0, security: true, stageSwitchMs: 0 });

  const env: NodeJS.ProcessEnv = {
    SSVC_DEVICE_URL: device.url,
    SSVC_USER: 'admin',
    SSVC_PASSWORD: 'admin',
    SSVC_MCP_TOKEN: E2E_MCP_TOKEN,
    SSVC_MCP_BIND: '127.0.0.1',
    // Порт из конфига не используется: слушаем порт 0 напрямую (app.listen(0)).
    SSVC_MCP_PORT: '8080',
    SSVC_MCP_ALLOWED_CIDRS: 'private',
    SSVC_ALLOW_DESTRUCTIVE: '0',
    SSVC_RAW_LOG: 'off',
    SSVC_RAW_LOG_DIR: join(dir, 'raw'),
    SSVC_AUDIT_PATH: join(dir, 'audit.jsonl'),
    SSVC_LOG_LEVEL: 'silent',
    SSVC_DEVICE_TIMEOUT_MS: '5000',
  };

  const cfg = loadConfig(env);
  const log = silentLogger();
  const audit = new AuditLog({ path: cfg.auditPath, session: 'e2e', log });
  const raw = new RawLogger({ mode: cfg.raw.mode, dir: cfg.raw.dir, maxMb: cfg.raw.maxMb, log });
  const client = new DeviceClient({ cfg, raw, log });
  const caps = new CapabilityCache(client, { log });
  const ctx: ToolContext = { cfg, client, caps, log, audit, raw };

  const app = await createApp({ cfg, ctx, tools: [...allTools] });
  const address = await app.listen(0, '127.0.0.1');

  return {
    device,
    app,
    baseUrl: `http://127.0.0.1:${address.port}`,
    dir,
    async close(): Promise<void> {
      await app.close();
      await client.close();
      await raw.close();
      await audit.close();
      await device.close();
      await rm(dir, { recursive: true, force: true, maxRetries: 3, retryDelay: 50 });
    },
  };
}

/** Вызов инструмента через SDK: конверт результата + признак ошибки. */
async function callTool(
  client: Client,
  name: string,
  args: Record<string, unknown> = {},
): Promise<Record<string, any>> {
  const result = (await client.callTool({ name, arguments: args })) as CallToolResult;
  const payload = payloadOf(result);
  // `isError` не входит в structuredContent: переносим его в конверт для проверок.
  return { ...payload, isError: result.isError === true };
}

test('e2e: initialize → tools/list → prompts/list → чтения → ожидание изменения → политика', async (t) => {
  const harness = await startHarness();
  t.after(() => harness.close());
  const { device, baseUrl } = harness;

  // --- 1. Доступ: без токена initialize отбивается 401 -------------------
  const noToken = await fetch(`${baseUrl}/mcp`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      accept: 'application/json, text/event-stream',
    },
    body: INITIALIZE_BODY,
  });
  assert.equal(noToken.status, 401, 'без Bearer-токена сервис обязан ответить 401');
  assert.match(String(noToken.headers.get('www-authenticate')), /Bearer/i);

  // --- 2. Initialize настоящим SDK-клиентом ------------------------------
  const transport = new StreamableHTTPClientTransport(new URL(`${baseUrl}/mcp`), {
    requestInit: { headers: { Authorization: `Bearer ${E2E_MCP_TOKEN}` } },
  });
  const client = new Client({ name: 'wp11-e2e-client', version: '1.0.0' });
  t.after(async () => {
    // Штатно клиент закрывается в шаге 9; повторное закрытие не должно ронять тест.
    await client.close().catch(() => undefined);
  });
  await client.connect(transport);
  assert.equal(client.getServerVersion()?.name, 'ssvc-openconnect-mcp');

  // --- 3. tools/list: весь каталог виден клиенту --------------------------
  const listed = await client.listTools();
  const listedNames = listed.tools.map((tool) => tool.name).sort();
  assert.equal(listedNames.length, 50, 'tools/list обязан отдать 50 инструментов');
  assert.deepEqual(listedNames, [...uniqueToolNames]);
  for (const tool of listed.tools) {
    assert.ok(tool.title !== undefined && tool.title !== '', `${tool.name}: клиент не увидел title`);
    assert.ok(tool.inputSchema !== undefined, `${tool.name}: нет inputSchema в tools/list`);
  }

  // --- 4. prompts/list: пять сценариев ----------------------------------
  const prompts = await client.listPrompts();
  const promptNames = prompts.prompts.map((prompt) => prompt.name).sort();
  assert.equal(promptNames.length, 5, 'prompts/list обязан отдать 5 промптов');
  assert.deepEqual(promptNames, PROMPT_SPECS.map((spec) => spec.name).sort());

  // --- 4a. resources/list: два ресурса, зарегистрированных на сессию ----
  // Проверяет, что `registerSessionCatalog` действительно подключён HTTP-слоем,
  // а не только промпты: раньше ресурсы в живом сервисе не регистрировались вовсе.
  const resources = await client.listResources();
  assert.deepEqual(
    resources.resources.map((resource) => resource.uri).sort(),
    ['openconnect://info', 'openconnect://telemetry'],
  );

  // --- 5. READ-ONLY вызовы через весь стек -------------------------------
  const info = await callTool(client, 'oc_get_info');
  assert.equal(info.ok, true);
  assert.equal(info.device.firmware, '0.2.9.2', 'версия прошивки — из мока');
  assert.equal(info.device.api, '1.10');

  const capabilities = await callTool(client, 'oc_get_capabilities');
  assert.equal(capabilities.ok, true);
  assert.deepEqual(capabilities.unavailable, ['predec'], 'недоступная возможность видна агенту');

  const state = await callTool(client, 'oc_get_state');
  assert.equal(state.ok, true);
  assert.equal(state.status.stage, 'waiting', 'мок стартует в стадии waiting');
  assert.deepEqual(state.partial, [], 'снимок собран целиком');
  assert.equal(typeof state.telemetry.common.mmhg, 'number', 'числовая строка нормализована');

  const telemetry = await callTool(client, 'oc_get_telemetry');
  assert.equal(telemetry.ok, true);
  assert.equal(telemetry.status.stage, 'waiting');
  assert.equal(typeof telemetry.last_update_ms, 'number');

  const settings = await callTool(client, 'oc_get_settings');
  assert.equal(settings.ok, true);
  assert.equal(settings.settings.hyst, 0.5, 'настройки развёрнуты из ssvcSettings');

  const sensors = await callTool(client, 'oc_get_sensors');
  assert.equal(sensors.ok, true);
  const tankZone = (sensors.zones as Array<Record<string, any>>).find(
    (zone) => zone.zone === 'tank',
  );
  assert.ok(tankZone !== undefined, 'зона tank пришла из /rest/sensor');
  assert.equal(tankZone?.readings[0].value, 78.6);

  // --- 6. oc_wait_for_change: мок меняет стадию во время ожидания --------
  const waiting = callTool(client, 'oc_wait_for_change', {
    expect: { stage: 'heads' },
    timeout_s: 20,
    poll_ms: 250,
  });
  // Стадию меняем уже после того, как ожидание началось: иначе проверялся бы
  // «совпало с первого опроса», а не опрос.
  setTimeout(() => {
    device.state.stage = 'heads';
  }, 700);
  const waited = await waiting;
  assert.equal(waited.ok, true);
  assert.equal(waited.matched, true, 'ожидание обязано дождаться смены стадии на heads');
  assert.equal(waited.timeout, false);
  assert.equal(waited.observed.stage, 'heads');
  assert.ok(
    Number(waited.elapsed_s) >= 0.5,
    `ожидание должно было опрашивать, а не совпасть сразу (elapsed_s=${waited.elapsed_s})`,
  );

  // --- 7. Журналы и системный статус ------------------------------------
  const logs = await callTool(client, 'oc_logs_list');
  assert.equal(logs.ok, true);
  assert.ok((logs.files as string[]).includes('12.CSV'));

  const system = await callTool(client, 'oc_system_status');
  assert.equal(system.ok, true);
  assert.equal(typeof system.system.free_heap, 'number');

  // --- 8. Политика: разрушительное действие выключено --------------------
  const denied = await callTool(client, 'oc_profile_delete', {
    id: 'p1',
    confirm: true,
    reason: 'e2e-проверка политики разрушительных операций',
  });
  assert.equal(denied.isError, true);
  assert.equal(denied.ok, false);
  assert.equal(denied.error.code, 'disabled_by_policy');
  assert.equal(denied.error.details.flag, 'SSVC_ALLOW_DESTRUCTIVE');

  // --- 9. Сессия закрывается, сервис освобождает слот --------------------
  // `client.close()` только закрывает транспорт; сессию завершает DELETE /mcp,
  // который отправляет `terminateSession()` (Streamable HTTP, mcp-server.md §3.4).
  await transport.terminateSession();
  for (let attempt = 0; attempt < 40 && harness.app.sessions.size > 0; attempt += 1) {
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  assert.equal(harness.app.sessions.size, 0, 'после завершения сессии сессий не осталось');
  await client.close();
});

test('e2e: /healthz без токена и /readyz с токеном видят живое устройство', async (t) => {
  const harness = await startHarness();
  t.after(() => harness.close());

  const health = await fetch(`${harness.baseUrl}/healthz`);
  assert.equal(health.status, 200);
  assert.equal((await health.json() as { ok?: boolean }).ok, true);

  const ready = await fetch(`${harness.baseUrl}/readyz`, {
    headers: { Authorization: `Bearer ${E2E_MCP_TOKEN}` },
  });
  assert.equal(ready.status, 200);
  const body = (await ready.json()) as { ok?: boolean; ready?: boolean; device?: { firmware?: string } };
  assert.equal(body.ready, true);
  assert.equal(body.device?.firmware, '0.2.9.2');
});
