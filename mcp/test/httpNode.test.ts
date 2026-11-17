/**
 * Тесты реального HTTP-транспорта (`createNodeHttp`) на настоящем сокете.
 *
 * Зачем не двойник: у прошивки есть особенность, которую `fakeHttp` не поймает —
 * ESPAsyncWebServer не понимает `Transfer-Encoding: chunked` и рвёт соединение
 * (ECONNRESET). Node выбирает chunked, когда тело отправляется без `Content-Length`
 * (достаточно `req.write()/req.end()` без заголовка), поэтому проверяем именно то,
 * что уходит на провод.
 */

import assert from 'node:assert/strict';
import { createServer, type IncomingHttpHeaders } from 'node:http';
import { test } from 'node:test';
import { createNodeHttp } from '../src/device/http.js';

interface Captured {
  headers?: IncomingHttpHeaders;
  body?: string;
}

async function startCaptureServer(t: { after: (fn: () => void) => void }): Promise<{
  url: string;
  captured: Captured;
}> {
  const captured: Captured = {};
  const server = createServer((req, res) => {
    const chunks: Buffer[] = [];
    req.on('data', (chunk: Buffer) => chunks.push(chunk));
    req.on('end', () => {
      captured.headers = req.headers;
      captured.body = Buffer.concat(chunks).toString('utf8');
      res.writeHead(200, { 'content-type': 'application/json' });
      res.end('{"ok":true}');
    });
  });
  await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', resolve));
  t.after(() => server.close());
  const address = server.address();
  assert.ok(address !== null && typeof address === 'object', 'сервер должен слушать порт');
  return { url: `http://127.0.0.1:${address.port}`, captured };
}

test('POST с телом уходит с Content-Length, без chunked, с JSON по умолчанию', async (t) => {
  const { url, captured } = await startCaptureServer(t);

  const result = await createNodeHttp().request({
    method: 'POST',
    url: `${url}/rest/commands`,
    body: '{"commands":"status"}',
  });

  assert.equal(result.status, 200);
  assert.equal(captured.headers?.['transfer-encoding'], undefined, 'chunked недопустим: прошивка рвёт соединение');
  assert.equal(
    captured.headers?.['content-length'],
    String(Buffer.byteLength('{"commands":"status"}')),
    'длина тела должна быть явной',
  );
  assert.equal(captured.headers?.['content-type'], 'application/json', 'без заголовка по умолчанию JSON');
  assert.equal(captured.body, '{"commands":"status"}');
});

test('POST уважает явный Content-Type и не перезаписывает Content-Length', async (t) => {
  const { url, captured } = await startCaptureServer(t);

  await createNodeHttp().request({
    method: 'PUT',
    url: `${url}/rest/settings`,
    headers: { 'Content-Type': 'application/json; charset=utf-8', 'Content-Length': '2' },
    body: '{}',
  });

  assert.equal(captured.headers?.['content-length'], '2');
  assert.equal(captured.headers?.['content-type'], 'application/json; charset=utf-8');
});

test('GET без тела не отправляет Content-Length', async (t) => {
  const { url, captured } = await startCaptureServer(t);

  await createNodeHttp().request({ method: 'GET', url: `${url}/rest/oc/info` });

  assert.equal(captured.headers?.['content-length'], undefined);
  assert.equal(captured.headers?.['transfer-encoding'], undefined);
});
