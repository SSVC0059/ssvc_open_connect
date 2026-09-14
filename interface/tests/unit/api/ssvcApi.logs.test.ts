/**
 * Тесты API-клиента журналов ректификаций (GET_LOG / REST).
 * Проверяют URL, методы и базовую обработку ответов для
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
	startLogDownload,
	downloadLog,
	cancelLogDownload
} from '$lib/api/ssvcApi';

describe('ssvcApi — журналы ректификаций (GET_LOG)', () => {
	let fetchMock: ReturnType<typeof vi.fn>;

	beforeEach(() => {
		fetchMock = vi.fn();
		vi.stubGlobal('fetch', fetchMock);
	});

	afterEach(() => {
		vi.unstubAllGlobals();
	});

	it('fetchLogStatus запрашивает GET /rest/logs и возвращает данные', async () => {
		fetchMock.mockResolvedValueOnce(
			new Response(JSON.stringify({ status: 'list', total: 0, received: 0, files: ['15.CSV'] }), {
				status: 200,
				headers: { 'content-type': 'application/json' }
			})
		);
		const result = await fetchLogStatus();
		expect(fetchMock).toHaveBeenCalledTimes(1);
		const [url, init] = fetchMock.mock.calls[0];
		expect(url).toBe('/rest/logs');
		expect(init.method).toBe('GET');
		expect(result).toEqual({ status: 'list', total: 0, received: 0, files: ['15.CSV'] });
	});

	it('fetchLogStatus возвращает null при ошибке HTTP', async () => {
		fetchMock.mockResolvedValueOnce(new Response('fail', { status: 500 }));
		const result = await fetchLogStatus();
		expect(result).toBeNull();
	});

	it('startLogDownload возвращает true при HTTP 202', async () => {
		fetchMock.mockResolvedValueOnce(new Response('', { status: 202 }));
		const ok = await startLogDownload(15);
		const [url, init] = fetchMock.mock.calls[0];
		expect(url).toBe('/rest/logs/15');
		expect(init.method).toBe('GET');
		expect(ok).toBe(true);
	});

	it('startLogDownload возвращает true при HTTP 200', async () => {
		fetchMock.mockResolvedValueOnce(new Response('', { status: 200 }));
		expect(await startLogDownload(15)).toBe(true);
	});

	it('startLogDownload возвращает false при HTTP 500', async () => {
		fetchMock.mockResolvedValueOnce(new Response('', { status: 500 }));
		expect(await startLogDownload(15)).toBe(false);
	});

	it('downloadLog возвращает Blob для text/csv', async () => {
		const csv = 'a;b;c\n1;2;3';
		const blobLike = {
			size: csv.length,
			type: 'text/csv',
			text: () => Promise.resolve(csv)
		};
		fetchMock.mockResolvedValueOnce({
			ok: true,
			status: 200,
			headers: {
				get: (name: string) =>
					name.toLowerCase() === 'content-type' ? 'text/csv' : null
			},
			blob: () => Promise.resolve(blobLike)
		});
		const blob = await downloadLog(15);
		const [url] = fetchMock.mock.calls[0];
		expect(url).toBe('/rest/logs/15');
		expect(blob).toBe(blobLike);
		expect(blob.size).toBe(csv.length);
	});

	it('downloadLog выбрасывает ошибку при неверном content-type', async () => {
		fetchMock.mockResolvedValueOnce(
			new Response('not csv', {
				status: 200,
				headers: { 'content-type': 'text/plain' }
			})
		);
		await expect(downloadLog(15)).rejects.toThrow();
	});

	it('downloadLog выбрасывает ошибку при HTTP 500', async () => {
		fetchMock.mockResolvedValueOnce(new Response('', { status: 500 }));
		await expect(downloadLog(15)).rejects.toThrow();
	});

	it('cancelLogDownload отправляет DELETE /rest/logs', async () => {
		fetchMock.mockResolvedValueOnce(
			new Response(JSON.stringify({ status: 'idle' }), {
				status: 200,
				headers: { 'content-type': 'application/json' }
			})
		);
		const ok = await cancelLogDownload();
		const [url, init] = fetchMock.mock.calls[0];
		expect(url).toBe('/rest/logs');
		expect(init.method).toBe('DELETE');
		expect(ok).toBe(true);
	});

	it('cancelLogDownload возвращает false при ошибке', async () => {
		fetchMock.mockResolvedValueOnce(new Response('fail', { status: 500 }));
		expect(await cancelLogDownload()).toBe(false);
	});
});
