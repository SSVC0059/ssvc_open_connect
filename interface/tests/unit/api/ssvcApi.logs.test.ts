/**
 * Тесты API-клиента журналов ректификаций (GET_LOG / REST).
 * Проверяют URL, методы и разбор ответов для
 * fetchLogStatus / startLogDownload / downloadLog / cancelLogDownload.
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

vi.mock('$app/state', () => ({
	page: {
		data: { features: { security: false } }
	}
}));

vi.mock('$lib/stores/user', () => ({
	user: {
		subscribe: () => () => {}
	}
}));

import {
	fetchLogStatus,
	refreshLogList,
	startLogDownload,
	downloadLog,
	cancelLogDownload
} from '$lib/api/ssvcApi';

const jsonResponse = (body: unknown, status = 200): Response =>
	new Response(JSON.stringify(body), {
		status,
		headers: { 'content-type': 'application/json' }
	});

const csvResponse = (csv: string, status = 200): Response =>
	new Response(csv, { status, headers: { 'content-type': 'text/csv' } });

describe('ssvcApi — журналы ректификаций (GET_LOG)', () => {
	let fetchMock: ReturnType<typeof vi.fn>;

	beforeEach(() => {
		fetchMock = vi.fn();
		vi.stubGlobal('fetch', fetchMock);
	});

	afterEach(() => {
		vi.unstubAllGlobals();
	});

	describe('fetchLogStatus', () => {
		it('запрашивает GET /rest/logs и возвращает данные', async () => {
			fetchMock.mockResolvedValueOnce(
				jsonResponse({ status: 'list', total: 0, received: 0, files: ['15.CSV'] })
			);
			const result = await fetchLogStatus();
			expect(fetchMock).toHaveBeenCalledTimes(1);
			const [url, init] = fetchMock.mock.calls[0];
			expect(url).toBe('/rest/logs');
			expect(init.method).toBe('GET');
			expect(result).toEqual({
				ok: true,
				data: { status: 'list', total: 0, received: 0, files: ['15.CSV'] }
			});
		});

		it('сохраняет HTTP-статус отказа вместо null', async () => {
			fetchMock.mockResolvedValueOnce(new Response('fail', { status: 500 }));
			const result = await fetchLogStatus();
			expect(result.ok).toBe(false);
			if (!result.ok) {
				expect(result.httpStatus).toBe(500);
				expect(result.message).toContain('500');
			}
		});

		it('разбирает код ошибки backend', async () => {
			fetchMock.mockResolvedValueOnce(jsonResponse({ error: 'log_transfer_busy' }, 409));
			const result = await fetchLogStatus();
			expect(result.ok).toBe(false);
			if (!result.ok) {
				expect(result.code).toBe('log_transfer_busy');
			}
		});

		it('передаёт AbortSignal в fetch', async () => {
			fetchMock.mockResolvedValueOnce(jsonResponse({ status: 'idle', total: 0, received: 0 }));
			const controller = new AbortController();
			await fetchLogStatus(controller.signal);
			expect(fetchMock.mock.calls[0][1].signal).toBeDefined();
		});

		it('statusOnly не даёт опросу запускать список', async () => {
			fetchMock.mockResolvedValueOnce(jsonResponse({ status: 'receiving', total: 4, received: 1 }));
			await fetchLogStatus(undefined, { statusOnly: true });
			expect(fetchMock.mock.calls[0][0]).toBe('/rest/logs?statusOnly=1');
		});

		it('refreshLogList запрашивает список заново', async () => {
			fetchMock.mockResolvedValueOnce(
				jsonResponse({ status: 'list', total: 0, received: 0, files: ['15.CSV'] })
			);
			const result = await refreshLogList();
			expect(fetchMock.mock.calls[0][0]).toBe('/rest/logs?refresh=1');
			expect(result.ok).toBe(true);
		});

		it('отдаёт network_error, если запрос не выполнен', async () => {
			fetchMock.mockRejectedValueOnce(new TypeError('Failed to fetch'));
			const result = await fetchLogStatus();
			expect(result.ok).toBe(false);
			if (!result.ok) {
				expect(result.code).toBe('network_error');
			}
		});
	});

	describe('startLogDownload', () => {
		it('202 → accepted со снапшотом статуса', async () => {
			fetchMock.mockResolvedValueOnce(
				jsonResponse({ status: 'receiving', total: 12, received: 3 }, 202)
			);
			const result = await startLogDownload(15);
			const [url, init] = fetchMock.mock.calls[0];
			expect(url).toBe('/rest/logs/15');
			expect(init.method).toBe('GET');
			expect(result).toEqual({
				kind: 'accepted',
				status: { status: 'receiving', total: 12, received: 3 }
			});
		});

		it('200 text/csv → ready с Blob', async () => {
			fetchMock.mockResolvedValueOnce(csvResponse('a;b\n1;2'));
			const result = await startLogDownload(15);
			expect(result.kind).toBe('ready');
			if (result.kind === 'ready') {
				expect(result.blob.size).toBe('a;b\n1;2'.length);
			}
		});

		it('409 → error с кодом backend', async () => {
			fetchMock.mockResolvedValueOnce(jsonResponse({ error: 'log_transfer_busy' }, 409));
			const result = await startLogDownload(15);
			expect(result.kind).toBe('error');
			if (result.kind === 'error') {
				expect(result.httpStatus).toBe(409);
				expect(result.code).toBe('log_transfer_busy');
			}
		});

		it('500 → error с HTTP-статусом', async () => {
			fetchMock.mockResolvedValueOnce(new Response('', { status: 500 }));
			const result = await startLogDownload(15);
			expect(result.kind).toBe('error');
			if (result.kind === 'error') {
				expect(result.httpStatus).toBe(500);
			}
		});

		it('сетевой сбой → error, а не тихий false', async () => {
			fetchMock.mockRejectedValueOnce(new TypeError('Failed to fetch'));
			const result = await startLogDownload(15);
			expect(result.kind).toBe('error');
			if (result.kind === 'error') {
				expect(result.code).toBe('network_error');
			}
		});
	});

	describe('downloadLog', () => {
		it('возвращает Blob для text/csv', async () => {
			fetchMock.mockResolvedValueOnce(csvResponse('a;b;c\n1;2;3'));
			const blob = await downloadLog(15);
			const [url] = fetchMock.mock.calls[0];
			expect(url).toBe('/rest/logs/15');
			expect(blob.size).toBe('a;b;c\n1;2;3'.length);
		});

		it('выбрасывает ошибку при неверном content-type', async () => {
			fetchMock.mockResolvedValueOnce(
				new Response('not csv', {
					status: 200,
					headers: { 'content-type': 'text/plain' }
				})
			);
			await expect(downloadLog(15)).rejects.toThrow();
		});

		it('выбрасывает ошибку при HTTP 500', async () => {
			fetchMock.mockResolvedValueOnce(new Response('', { status: 500 }));
			await expect(downloadLog(15)).rejects.toThrow();
		});
	});

	describe('cancelLogDownload', () => {
		it('отправляет DELETE /rest/logs', async () => {
			fetchMock.mockResolvedValueOnce(jsonResponse({ status: 'idle' }));
			const ok = await cancelLogDownload();
			const [url, init] = fetchMock.mock.calls[0];
			expect(url).toBe('/rest/logs');
			expect(init.method).toBe('DELETE');
			expect(ok).toBe(true);
		});

		it('возвращает false при ошибке', async () => {
			fetchMock.mockResolvedValueOnce(new Response('fail', { status: 500 }));
			expect(await cancelLogDownload()).toBe(false);
		});
	});
});
