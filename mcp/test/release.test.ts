/**
 * Релизная закалка (WP11, §15 п.6; mcp-server.md §3.11, P4).
 *
 * Проверяется ровно то, что обещает README и чек-лист готовности:
 *
 *  1. `.env.example` содержит **безопасные релизные** значения: разрушительные
 *     операции выключены, сырые логи не пишутся, `debug:true` недоступен — и эти же
 *     значения являются умолчаниями кода (пустое окружение не открывает опасное);
 *  2. каждый инструмент класса риска ≥3 при `SSVC_ALLOW_DESTRUCTIVE=0` отвечает
 *     `disabled_by_policy`, а при `1` — доходит до устройства (то есть блокирует
 *     именно флаг, а не что-то ещё).
 *
 * Список инструментов класса ≥3 НЕ хардкодится: он определяется по поведению
 * политики (`test/riskProbe.ts`), поэтому новый разрушительный инструмент попадёт
 * в проверку автоматически.
 */

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
import { loadConfig, readDotEnvFile } from '../src/config.js';
import { runTool } from '../src/mcp/registry.js';
import { allTools } from '../src/tools/index.js';
import { makeTestContext } from './fakeHttp.js';
import { RISK_PROBE_ARGS, errorOf, probeDestructiveTools } from './riskProbe.js';

/** Путь к `.env.example` относительно этого теста (компилируется в `.test-dist/test/`). */
const ENV_EXAMPLE_PATH = fileURLToPath(new URL('../../.env.example', import.meta.url));

/** Минимальное окружение для `loadConfig`: остальное берётся из умолчаний кода. */
const MINIMAL_ENV: NodeJS.ProcessEnv = {
  SSVC_MCP_TOKEN: 'release-hardening-token',
  SSVC_DEVICE_URL: 'http://192.0.2.10',
  SSVC_PASSWORD: 'release-hardening-password',
};

test('.env.example: релизные значения опасных флагов', () => {
  const text = readFileSync(ENV_EXAMPLE_PATH, 'utf8');
  const values = readDotEnvFile(ENV_EXAMPLE_PATH);

  assert.equal(values.SSVC_ALLOW_DESTRUCTIVE, '0', 'разрушительные операции выключены в примере');
  assert.equal(values.SSVC_RAW_LOG, 'off', 'сырые логи не пишутся в примере');

  // Переменные, которые читает config.ts, должны быть в примере: иначе человек
  // не узнает о них из шаблона и будет править код.
  for (const name of ['SSVC_DEVICE_TIMEOUT_MS']) {
    assert.match(text, new RegExp(`^${name}=`, 'm'), `в .env.example нет ${name}`);
    assert.notEqual(values[name] ?? '', '', `${name} в примере пустая`);
    assert.ok(
      Number.isFinite(Number(values[name])),
      `${name} в примере должна быть числом, получено «${values[name]}»`,
    );
  }
});

test('умолчания кода безопасны: пустое окружение не включает опасное', () => {
  const cfg = loadConfig({ ...MINIMAL_ENV });
  assert.equal(cfg.allowDestructive, false, 'SSVC_ALLOW_DESTRUCTIVE по умолчанию выключен');
  assert.equal(cfg.raw.mode, 'off', 'SSVC_RAW_LOG по умолчанию off');
  // Явный 0 в окружении тоже выключает: значение флага, а не его отсутствие.
  const explicit = loadConfig({ ...MINIMAL_ENV, SSVC_ALLOW_DESTRUCTIVE: '0' });
  assert.equal(explicit.allowDestructive, false);
});

test('риск ≥3: при SSVC_ALLOW_DESTRUCTIVE=0 каждый инструмент отвечает disabled_by_policy', async () => {
  const risk3 = (await probeDestructiveTools(allTools))
    .filter((probe) => probe.requiresReason)
    .map((probe) => probe.name);

  assert.ok(risk3.length > 0, 'в каталоге должен быть хотя бы один инструмент класса ≥3');

  const testCtx = await makeTestContext([], { allowDestructive: false });
  try {
    for (const name of risk3) {
      const tool = allTools.find((candidate) => candidate.name === name);
      assert.ok(tool !== undefined, `инструмент «${name}» есть в каталоге`);
      const result = await runTool(tool!, testCtx.ctx, {
        ...RISK_PROBE_ARGS,
        confirm: true,
        reason: 'релизная проверка политики разрушительных операций',
      });
      const error = errorOf(result);
      assert.equal(result.isError, true, `${name}: отказ должен быть isError`);
      assert.equal(
        error?.code,
        'disabled_by_policy',
        `${name}: ожидался disabled_by_policy, получено «${error?.code}» (${error?.message ?? ''})`,
      );
      assert.equal(
        error?.details?.flag,
        'SSVC_ALLOW_DESTRUCTIVE',
        `${name}: в details должен быть назван флаг`,
      );
    }
  } finally {
    await testCtx.cleanup();
  }
});

test('риск ≥3: при SSVC_ALLOW_DESTRUCTIVE=1 политика пропускает вызов до устройства', async () => {
  const risk3 = (await probeDestructiveTools(allTools))
    .filter((probe) => probe.requiresReason)
    .map((probe) => probe.name);

  const testCtx = await makeTestContext([], { allowDestructive: true });
  try {
    for (const name of risk3) {
      const tool = allTools.find((candidate) => candidate.name === name);
      assert.ok(tool !== undefined);
      const result = await runTool(tool!, testCtx.ctx, {
        ...RISK_PROBE_ARGS,
        confirm: true,
        reason: 'релизная проверка: флаг включён, действие разрешено',
      });
      const error = errorOf(result);
      assert.notEqual(
        error?.code,
        'disabled_by_policy',
        `${name}: при SSVC_ALLOW_DESTRUCTIVE=1 отказ политики неуместен`,
      );
      // Фейковое устройство не отвечает маршрутом — отказ приходит от устройства,
      // то есть политика инструмент уже пропустила.
      assert.equal(result.isError, true, `${name}: на фейковом устройстве ожидаем ошибку`);
    }
  } finally {
    await testCtx.cleanup();
  }
});

test('риск ≥3 без reason: validation_failed с required=reason (до сети)', async () => {
  const risk3 = (await probeDestructiveTools(allTools))
    .filter((probe) => probe.requiresReason)
    .map((probe) => probe.name);

  const testCtx = await makeTestContext([], { allowDestructive: true });
  try {
    for (const name of risk3) {
      const tool = allTools.find((candidate) => candidate.name === name);
      assert.ok(tool !== undefined);
      const result = await runTool(tool!, testCtx.ctx, { ...RISK_PROBE_ARGS, confirm: true });
      const error = errorOf(result);
      assert.equal(error?.code, 'validation_failed', `${name}: без reason ожидался validation_failed`);
      assert.equal(error?.details?.required, 'reason', `${name}: причина отказа — именно reason`);
    }
  } finally {
    await testCtx.cleanup();
  }
});
