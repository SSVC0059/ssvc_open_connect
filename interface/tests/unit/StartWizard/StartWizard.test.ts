/**
 * Тесты для StartWizard.svelte (Svelte 5).
 * Нормализацию release_timer (issue #75) покрывают wizardDefaults.test.ts.
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import { writable } from 'svelte/store';
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

import { fetchSettings, saveSettings, sendCommand } from '$lib/api/ssvcApi';
import { modals } from 'svelte-modals';

describe('StartWizard', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.mocked(fetchSettings).mockResolvedValue(createMockSsvcSettings());
		vi.mocked(saveSettings).mockResolvedValue(true);
		vi.mocked(sendCommand).mockResolvedValue({ success: true, message: 'ok' });
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
});
