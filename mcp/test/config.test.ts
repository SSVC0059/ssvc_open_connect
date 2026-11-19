/**
 * Конфигурация: безопасные умолчания и разница требований между транспортами.
 *
 *  - HTTP открывает порт, поэтому без `SSVC_MCP_TOKEN` не стартует;
 *  - stdio-процесс запускает сам клиент: токен и пароль контроллера не обязательны
 *    (пароль нужен только при включённой на прошивке авторизации);
 *  - адрес прослушивания по умолчанию — петлевой: открыть сервис в сеть можно
 *    только явным `SSVC_MCP_BIND`.
 */

import assert from 'node:assert/strict';
import test from 'node:test';
import { ConfigError, DEFAULTS, loadConfig } from '../src/config.js';

const DEVICE_ENV: NodeJS.ProcessEnv = {
  SSVC_DEVICE_URL: 'http://192.0.2.10',
};

test('HTTP: без SSVC_MCP_TOKEN конфигурация не собирается', () => {
  assert.throws(
    () => loadConfig({ ...DEVICE_ENV, SSVC_PASSWORD: 'secret' }),
    (error: unknown) =>
      error instanceof ConfigError && /SSVC_MCP_TOKEN/.test(error.message),
  );
});

test('HTTP: без SSVC_PASSWORD конфигурация не собирается', () => {
  assert.throws(
    () => loadConfig({ ...DEVICE_ENV, SSVC_MCP_TOKEN: 'token' }),
    (error: unknown) =>
      error instanceof ConfigError && /SSVC_PASSWORD/.test(error.message),
  );
});

test('stdio: запускается без токена и пароля — только адрес контроллера обязателен', () => {
  const cfg = loadConfig({ ...DEVICE_ENV }, { transport: 'stdio' });
  assert.equal(cfg.transport, 'stdio');
  assert.equal(cfg.mcp.token, '');
  assert.equal(cfg.device.password, '');
  assert.equal(cfg.device.url, 'http://192.0.2.10');
});

test('stdio: SSVC_DEVICE_URL остаётся обязательной', () => {
  assert.throws(
    () => loadConfig({}, { transport: 'stdio' }),
    (error: unknown) =>
      error instanceof ConfigError && /SSVC_DEVICE_URL/.test(error.message),
  );
});

test('stdio: явные токен и пароль по-прежнему читаются', () => {
  const cfg = loadConfig(
    { ...DEVICE_ENV, SSVC_MCP_TOKEN: 'token', SSVC_PASSWORD: 'secret' },
    { transport: 'stdio' },
  );
  assert.equal(cfg.mcp.token, 'token');
  assert.equal(cfg.device.password, 'secret');
});

test('адрес прослушивания по умолчанию — петлевой, а не 0.0.0.0', () => {
  assert.equal(DEFAULTS.host, '127.0.0.1');
  const cfg = loadConfig({
    ...DEVICE_ENV,
    SSVC_MCP_TOKEN: 'token',
    SSVC_PASSWORD: 'secret',
  });
  assert.equal(cfg.mcp.host, '127.0.0.1');
  // Явное значение не перетирается: Docker/compose задают 0.0.0.0 осознанно.
  const explicit = loadConfig({
    ...DEVICE_ENV,
    SSVC_MCP_TOKEN: 'token',
    SSVC_PASSWORD: 'secret',
    SSVC_MCP_BIND: '0.0.0.0',
  });
  assert.equal(explicit.mcp.host, '0.0.0.0');
});
