/**
 * stdio-транспорт: DSH/IDE сами запускают процесс и общаются по stdin/stdout.
 *
 * Проверяем именно собранный `.test-dist/src/index.js` с флагом `--stdio`:
 *  - сервер стартует без `SSVC_MCP_TOKEN` и `SSVC_PASSWORD` (заявленный путь DSH);
 *  - инструменты и `oc_calculate_plan` доступны без HTTP и токена;
 *  - в stdout нет логов (иначе JSON-RPC поток был бы испорчен);
 *  - процесс завершается после закрытия клиента.
 */

import assert from 'node:assert/strict';
import { join } from 'node:path';
import test from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { startStdioService } from '../src/stdio.js';

test('stdio: 51 инструмент, расчёт работает, stdout — только протокол', async () => {
  const env: Record<string, string> = {};
  for (const [key, value] of Object.entries(process.env)) {
    if (value !== undefined) env[key] = value;
  }
  env.SSVC_DEVICE_URL = 'http://127.0.0.1:9';
  env.SSVC_USER = 'admin';
  env.SSVC_RAW_LOG = 'off';
  env.SSVC_LOG_LEVEL = 'silent';
  // Секреты не подставляем: именно их отсутствие проверяет этот тест.
  delete env.SSVC_MCP_TOKEN;
  delete env.SSVC_PASSWORD;

  const transport = new StdioClientTransport({
    command: process.execPath,
    args: [join(process.cwd(), '.test-dist', 'src', 'index.js'), '--stdio'],
    cwd: process.cwd(),
    env,
    stderr: 'ignore',
  });

  const client = new Client({ name: 'stdio-test', version: '1.0.0' });
  try {
    await client.connect(transport);
    const tools = await client.listTools();
    assert.equal(tools.tools.length, 51);

    const calculated = await client.callTool({
      name: 'oc_calculate_plan',
      arguments: { volume_l: 20, strength_pct: 40 },
    });
    assert.equal(calculated.isError, undefined);
    assert.equal(
      (calculated.structuredContent as Record<string, any>).plan.total_as_ml,
      8000,
    );
  } finally {
    await client.close();
  }
});

test('stdio: до первого сообщения клиента в stdout не пишется ни строки', async () => {
  const writes: string[] = [];
  const stdout = process.stdout as unknown as { write: (...args: any[]) => boolean };
  const original = stdout.write;
  stdout.write = (chunk: unknown) => {
    writes.push(String(chunk));
    return true;
  };

  const env: NodeJS.ProcessEnv = {
    SSVC_DEVICE_URL: 'http://127.0.0.1:9',
    SSVC_USER: 'admin',
    SSVC_LOG_LEVEL: 'silent',
    SSVC_RAW_LOG: 'off',
  };
  delete env.SSVC_MCP_TOKEN;
  delete env.SSVC_PASSWORD;

  try {
    const service = await startStdioService({ env });
    try {
      // Транспорт пишет в stdout только протокольные сообщения; старт и любой лог
      // обязаны уходить в stderr, иначе JSON-RPC поток клиента становится невалидным.
      assert.deepEqual(writes, [], 'stdout испорчен до начала протокола');
    } finally {
      await service.stop();
    }
  } finally {
    stdout.write = original;
  }
});
