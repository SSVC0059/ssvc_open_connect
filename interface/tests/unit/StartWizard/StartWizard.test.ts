/**
 * Тесты для StartWizard.svelte (Svelte 5).
 * Нормализацию release_timer (issue #75) покрывают wizardDefaults.test.ts.
 *
 * Шаги мастера заменены моками (MockStep), чтобы тестировать навигацию и
 * сохранение целиком. Настоящий WizardStep1 проверяется отдельным блоком через
 * `vi.importActual` — иначе его полей (предекремент, формула) в этом файле не видно.
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import { writable } from 'svelte/store';
import type { SsvcOpenConnectInfo, SsvcSettings } from '$lib/types/ssvc';
import StartWizard from '$lib/components/StartWizard/StartWizard.svelte';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';

vi.mock('$lib/api/ssvcApi', () => ({
	fetchSettings: vi.fn(),
	saveSettings: vi.fn(),
	sendCommand: vi.fn(),
	getInfo: vi.fn(),
	infoCacheVersion: writable(0)
}));

vi.mock('svelte-modals', () => ({
	modals: { close: vi.fn() }
}));

vi.mock('$lib/components/StartWizard/steps/WizardStep1.svelte', async () => {
	const { default: M } = await import('../../mocks/MockStep.svelte');
	return { default: M };
});
vi.mock('$lib/components/StartWizard/steps/WizardStep2.svelte', async () => {
	const { default: M } = await import('../../mocks/MockStep.svelte');
	return { default: M };
});
vi.mock('$lib/components/StartWizard/steps/WizardStep3.svelte', async () => {
	const { default: M } = await import('../../mocks/MockStep.svelte');
	return { default: M };
});
vi.mock('$lib/components/StartWizard/steps/WizardStep4.svelte', async () => {
	const { default: M } = await import('../../mocks/MockStep.svelte');
	return { default: M };
});
vi.mock('$lib/components/StartWizard/steps/WizardStep5.svelte', async () => {
	const { default: M } = await import('../../mocks/MockStep.svelte');
	return { default: M };
});
vi.mock('$lib/components/StartWizard/steps/WizardStep6.svelte', async () => {
	const { default: M } = await import('../../mocks/MockStep.svelte');
	return { default: M };
});
vi.mock('$lib/components/StartWizard/steps/WizardStep7.svelte', async () => {
	const { default: M } = await import('../../mocks/MockStep.svelte');
	return { default: M };
});

import { fetchSettings, getInfo, saveSettings, sendCommand } from '$lib/api/ssvcApi';
import { modals } from 'svelte-modals';

/** /rest/oc/info на API 1.9: доступны предекремент и «Авто 92+». */
const API_INFO_1_9: SsvcOpenConnectInfo = {
	ssvc: { version: '2.0', api: '1.9', api_code: 109, mode: 'late_heads' },
	oc: {
		version: 'test',
		is_support_api: true,
		api_features: [
			{ name: 'predec', min_api: '1.9', available: true },
			{ name: 'formula_auto92', min_api: '1.9', available: true }
		]
	}
};

/** /rest/oc/info на API 1.7: ни предекремента, ни «Авто 92+». */
const API_INFO_1_7: SsvcOpenConnectInfo = {
	ssvc: { version: '1.2', api: '1.7', api_code: 107, mode: 'late_heads' },
	oc: { version: 'test', is_support_api: true, api_features: [] }
};

describe('StartWizard', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.mocked(fetchSettings).mockResolvedValue(createMockSsvcSettings());
		vi.mocked(saveSettings).mockResolvedValue(true);
		vi.mocked(sendCommand).mockResolvedValue({ success: true, message: 'ok' });
		vi.mocked(getInfo).mockResolvedValue(API_INFO_1_9);
	});

	it('рендерит заголовок мастера при isOpen=true', async () => {
		render(StartWizard, { props: { isOpen: true } });

		await waitFor(() => {
			expect(screen.getByRole('dialog')).toBeInTheDocument();
		});

		expect(screen.getByText('Мастер запуска ректификации')).toBeInTheDocument();
	});

	it('не рендерит контент при isOpen=false', () => {
		render(StartWizard, { props: { isOpen: false } });
		expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
	});

	it('показывает кнопку «Дальше» и переключает шаги', async () => {
		const user = userEvent.setup();
		render(StartWizard, { props: { isOpen: true } });

		await waitFor(() => {
			expect(screen.getByRole('dialog')).toBeInTheDocument();
		});

		// Ждём загрузки настроек и появления кнопки «Дальше» (7 шагов → не последний)
		const nextButton = await screen.findByRole('button', { name: /Дальше/i }, { timeout: 3000 });
		expect(nextButton).toBeInTheDocument();
		await user.click(nextButton);
		await user.click(nextButton);
		// Мок-шаг отображается
		expect(screen.getByTestId('mock-step')).toBeInTheDocument();
	});

	it('при открытии вызывает fetchSettings (REST /rest/settings)', async () => {
		render(StartWizard, { props: { isOpen: true } });

		await waitFor(
			() => {
				expect(fetchSettings).toHaveBeenCalled();
			},
			{ timeout: 2000 }
		);
	});

	it('без изменений не сохраняет настройки, но запускает ректификацию', async () => {
		const user = userEvent.setup();
		vi.mocked(fetchSettings).mockResolvedValue(createMockSsvcSettings({ release_timer: -1 }));

		render(StartWizard, { props: { isOpen: true } });

		// Переходим на последний шаг (6 раз «Дальше» при 7 шагах)
		const nextButton = await screen.findByRole('button', { name: /Дальше/i }, { timeout: 3000 });
		for (let i = 0; i < 6; i++) {
			await user.click(nextButton);
		}

		const submitButton = await screen.findByRole('button', { name: /Запустить/i }, { timeout: 2000 });
		await user.click(submitButton);

		// Частичное обновление: нечего отправлять — PUT не нужен, контроллер
		// сохраняет собственные настройки.
		expect(saveSettings).not.toHaveBeenCalled();

		await waitFor(() => {
			expect(sendCommand).toHaveBeenCalledWith('start');
		});
		await waitFor(() => {
			expect(modals.close).toHaveBeenCalled();
		});
	});

	it('отправляет только изменённые поля', async () => {
		const user = userEvent.setup();
		render(StartWizard, { props: { isOpen: true } });

		const changeButton = await screen.findByTestId('mock-change', {}, { timeout: 3000 });
		await user.click(changeButton);

		const nextButton = await screen.findByRole('button', { name: /Дальше/i }, { timeout: 3000 });
		for (let i = 0; i < 6; i++) {
			await user.click(nextButton);
		}

		const submitButton = await screen.findByRole('button', { name: /Запустить/i }, { timeout: 2000 });
		await user.click(submitButton);

		await waitFor(() => {
			expect(saveSettings).toHaveBeenCalledWith({ hyst: 1 });
		});
		await waitFor(() => {
			expect(sendCommand).toHaveBeenCalledWith('start');
		});
	});

	it('не закрывает мастер, если контроллер отклонил запуск', async () => {
		const user = userEvent.setup();
		vi.mocked(sendCommand).mockResolvedValue({ success: false, message: 'busy' });

		render(StartWizard, { props: { isOpen: true } });

		const nextButton = await screen.findByRole('button', { name: /Дальше/i }, { timeout: 3000 });
		for (let i = 0; i < 6; i++) {
			await user.click(nextButton);
		}

		const submitButton = await screen.findByRole('button', { name: /Запустить/i }, { timeout: 2000 });
		await user.click(submitButton);

		await waitFor(() => {
			expect(sendCommand).toHaveBeenCalledWith('start');
		});
		expect(modals.close).not.toHaveBeenCalled();
	});

	it('не запускает ректификацию, если настройки не приняты контроллером', async () => {
		const user = userEvent.setup();
		// 503 от PUT /rest/settings: часть SET не попала в очередь.
		vi.mocked(saveSettings).mockResolvedValue(false);

		render(StartWizard, { props: { isOpen: true } });

		const changeButton = await screen.findByTestId('mock-change', {}, { timeout: 3000 });
		await user.click(changeButton);

		const nextButton = await screen.findByRole('button', { name: /Дальше/i }, { timeout: 3000 });
		for (let i = 0; i < 6; i++) {
			await user.click(nextButton);
		}

		const submitButton = await screen.findByRole('button', { name: /Запустить/i }, { timeout: 2000 });
		await user.click(submitButton);

		await waitFor(() => {
			expect(saveSettings).toHaveBeenCalledWith({ hyst: 1 });
		});
		expect(sendCommand).not.toHaveBeenCalled();
		expect(modals.close).not.toHaveBeenCalled();
	});
});

/**
 * Настоящий первый шаг мастера: в блоке StartWizard он замокан для проверки
 * навигации, поэтому берём реальный модуль в обход мока (`vi.importActual`).
 * Мок `$lib/api/ssvcApi` при этом продолжает действовать.
 */
describe('WizardStep1: предекремент и формула', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.mocked(getInfo).mockResolvedValue(API_INFO_1_9);
	});

	async function renderStep1(settings: SsvcSettings) {
		const actual = (await vi.importActual(
			'$lib/components/StartWizard/steps/WizardStep1.svelte'
		)) as { default: any };
		return render(actual.default, { props: { settings } });
	}

	it('показывает предупреждение, если выбран «Авто 92+» без предекремента', async () => {
		await renderStep1(createMockSsvcSettings({ formula: 2, predec: 0, hyst: 0.2 }));

		expect(await screen.findByText(/«Авто 92\+» не сработает/)).toBeInTheDocument();
	});

	it('предупреждает о недостаточном гистерезисе для включённого предекремента', async () => {
		await renderStep1(createMockSsvcSettings({ formula: 1, predec: 13, hyst: 0.06 }));

		expect(await screen.findByText(/контроллер отключит его/)).toBeInTheDocument();
	});

	it('не показывает предупреждений для рабочего сочетания', async () => {
		await renderStep1(createMockSsvcSettings({ formula: 2, predec: 7, hyst: 0.12 }));

		// Предекремент становится доступен только после ответа /rest/oc/info.
		await waitFor(() => expect(screen.getByLabelText('Предекремент')).toBeEnabled());
		expect(screen.queryByRole('status')).not.toBeInTheDocument();
	});

	it('ограничивает температуру начала формулы диапазоном 84…100 °C', async () => {
		await renderStep1(createMockSsvcSettings({ formula: 1, formula_start_temp: 92 }));

		const input = screen.getByPlaceholderText('Начальная температура');
		expect(input).toHaveAttribute('min', '84');
		expect(input).toHaveAttribute('max', '100');
	});

	it('блокирует предекремент и «Авто 92+» на контроллере без API 1.9', async () => {
		vi.mocked(getInfo).mockResolvedValue(API_INFO_1_7);
		await renderStep1(createMockSsvcSettings());

		const predecSelect = screen.getByLabelText('Предекремент');
		await waitFor(() =>
			expect(predecSelect).toHaveAttribute('title', 'Требуется API 1.9')
		);
		expect(predecSelect).toBeDisabled();
		expect(screen.getByRole('option', { name: 'Авто 92+' })).toBeDisabled();
	});
});
