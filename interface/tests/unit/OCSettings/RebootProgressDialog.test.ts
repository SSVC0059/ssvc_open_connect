/**
 * Тесты диалога перезагрузки контроллера (RebootProgressDialog).
 *
 * Машина состояний проверяется без реального перезапуска устройства:
 * команда отправляется один раз, шкала появляется сразу после подтверждения,
 * а возвратом считается только ответ контроллера ПОСЛЕ пропадания связи —
 * мгновенный успешный опрос ещё не означает, что контроллер перезагрузился.
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import { modals } from 'svelte-modals';
import RebootProgressDialog from '$lib/components/RebootProgressDialog.svelte';
import { pingDevice, requestRestart } from '$lib/api/ssvcApi';

vi.mock('$lib/api/ssvcApi', () => ({
	requestRestart: vi.fn(),
	pingDevice: vi.fn()
}));

const modalState = vi.hoisted(() => ({
	beforeClose: undefined as undefined | (() => boolean)
}));

vi.mock('svelte-modals', () => ({
	modals: { close: vi.fn() },
	onBeforeClose: (fn: () => boolean) => {
		modalState.beforeClose = fn;
	}
}));

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

/** Интервал опроса в компоненте — 1 с, поэтому ожидания здесь длиннее обычных. */
const PROBE_WAIT_MS = 5000;

const clickReboot = () =>
	userEvent.setup().click(screen.getByRole('button', { name: /Перезагрузить/ }));

describe('RebootProgressDialog', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		modalState.beforeClose = undefined;
		vi.mocked(requestRestart).mockResolvedValue({ ok: true });
		vi.mocked(pingDevice).mockResolvedValue(false);
	});

	it('до подтверждения показывает вопрос, не показывает шкалу и не трогает контроллер', async () => {
		render(RebootProgressDialog, {
			props: { isOpen: true, message: 'Перезагрузить сейчас?' }
		});

		expect(screen.getByText('Перезагрузить сейчас?')).toBeInTheDocument();
		expect(screen.queryByRole('progressbar')).toBeNull();
		expect(requestRestart).not.toHaveBeenCalled();

		await userEvent.setup().click(screen.getByRole('button', { name: /Позже/ }));

		expect(requestRestart).not.toHaveBeenCalled();
		expect(modals.close).toHaveBeenCalled();
	});

	it('после подтверждения отправляет команду один раз и показывает шкалу перезагрузки', async () => {
		render(RebootProgressDialog, { props: { isOpen: true } });

		await clickReboot();

		await waitFor(() => expect(requestRestart).toHaveBeenCalledTimes(1));
		expect(screen.getByRole('progressbar')).toBeInTheDocument();
		expect(screen.getByText(/Ждём перезапуска контроллера/)).toBeInTheDocument();
		// Пока перезагрузка идёт, кнопок решения в окне нет.
		expect(screen.queryByRole('button', { name: /Перезагрузить/ })).toBeNull();
		expect(screen.queryByRole('button', { name: /Продолжить/ })).toBeNull();
	});

	it('возвратом считает ответ после пропадания связи, окно до этого не закрывается', async () => {
		const onFinished = vi.fn();
		// Контроллер какое-то время продолжает отвечать, потом уходит и возвращается.
		let probeNumber = 0;
		vi.mocked(pingDevice).mockImplementation(async () => {
			probeNumber += 1;
			if (probeNumber <= 2) return true;
			if (probeNumber === 3) return false;
			return true;
		});

		render(RebootProgressDialog, { props: { isOpen: true, onFinished } });
		await clickReboot();

		// Успешные опросы при живом контроллере не должны выдаваться за возврат.
		await sleep(2500);
		expect(screen.getByText(/Ждём перезапуска контроллера/)).toBeInTheDocument();
		expect(onFinished).not.toHaveBeenCalled();
		expect(modalState.beforeClose?.()).toBe(false);

		await waitFor(() => expect(screen.getByText(/перезагружается/)).toBeInTheDocument(), {
			timeout: PROBE_WAIT_MS
		});
		expect(modalState.beforeClose?.()).toBe(false);

		await waitFor(() => expect(screen.getByText(/снова на связи/)).toBeInTheDocument(), {
			timeout: PROBE_WAIT_MS
		});
		expect(onFinished).toHaveBeenCalledTimes(1);
		expect(screen.getByRole('progressbar')).toHaveAttribute('value', '100');
		expect(modalState.beforeClose?.()).toBe(true);
	});

	it('отказ контроллера показывает ошибку и не запускает ожидание', async () => {
		vi.mocked(requestRestart).mockResolvedValue({ ok: false, error: 'HTTP 401: Unauthorized' });

		render(RebootProgressDialog, { props: { isOpen: true } });
		await clickReboot();

		await waitFor(() => expect(screen.getByText(/HTTP 401: Unauthorized/)).toBeInTheDocument());
		expect(pingDevice).not.toHaveBeenCalled();
		// Ошибка не блокирует окно: пользователь может его закрыть.
		expect(modalState.beforeClose?.()).toBe(true);
		await userEvent.setup().click(screen.getByRole('button', { name: /Продолжить/ }));
		expect(modals.close).toHaveBeenCalled();
	});
});
