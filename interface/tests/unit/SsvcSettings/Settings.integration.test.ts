/**
 * Интеграционный тест страницы Settings: проверка вызова updateSetting (REST API)
 * при сохранении из вкладки «Параметры отбора».
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor, within } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';
import { SPEED_SETTINGS_EXPECTED_REQUESTS } from '../../lib/speedSettingsExpectedRequests';

const pageUrlRef = vi.hoisted(() => ({ href: 'http://test/oc/ssvc?tab=speed' }));

// Предотвращает загрузку @sveltejs/kit и ошибку "Cannot find package '__sveltekit'"
vi.mock('$app/stores', () => ({
	page: {
		subscribe: (fn: (v: unknown) => void) => {
			fn({
				url: new URL(pageUrlRef.href),
				params: {},
				data: {},
				status: 200,
				error: null
			});
			return () => {};
		}
	},
	navigating: { subscribe: (fn: (v: unknown) => void) => { fn(null); return () => {}; } },
	updated: { subscribe: (fn: (v: unknown) => void) => { fn(false); return () => {}; } }
}));

vi.mock('$app/navigation', () => ({
	goto: vi.fn()
}));

vi.mock('$app/environment', () => ({
	browser: true
}));

vi.mock('$lib/api/ssvcApi', () => ({
	fetchSettings: vi.fn(),
	updateSetting: vi.fn()
}));

vi.mock('$lib/components/toasts/notifications', () => ({
	notifications: { success: vi.fn(), error: vi.fn() }
}));

import Settings from '../../../src/routes/oc/ssvc/Settings.svelte';
import { fetchSettings, updateSetting } from '$lib/api/ssvcApi';
import { notifications } from '$lib/components/toasts/notifications';

describe('Settings — Параметры отбора (updateSetting)', () => {
	const headsSpec = SPEED_SETTINGS_EXPECTED_REQUESTS.heads;

	beforeEach(() => {
		pageUrlRef.href = 'http://test/oc/ssvc?tab=speed';
		vi.mocked(updateSetting).mockReset();
		vi.mocked(updateSetting).mockResolvedValue(true);
		vi.mocked(fetchSettings).mockReset();
		vi.mocked(fetchSettings).mockResolvedValue(
			createMockSsvcSettings({ heads: headsSpec.expectedRequestBody })
		);
		vi.mocked(notifications.error).mockClear();
	});

	it('при сохранении параметров отбора вызывает updateSetting с телом запроса из реестра', async () => {
		render(Settings);

		await waitFor(() => {
			expect(screen.getAllByText(headsSpec.sectionTitle).length).toBeGreaterThan(0);
		});

		// На md+ и max-md оба дерева могут присутствовать в jsdom; берём первый видимый экземпляр
		const section = screen.getAllByText(headsSpec.sectionTitle)[0].closest('.settings-section');
		const editButton = section?.querySelector('button.edit-button');
		expect(editButton).toBeTruthy();
		await userEvent.setup().click(editButton!);

		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});

		const dialog = screen.getByRole('dialog', { hidden: true });
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));

		await waitFor(() => {
			expect(updateSetting).toHaveBeenCalledTimes(1);
		});

		expect(updateSetting).toHaveBeenCalledWith(headsSpec.field, headsSpec.expectedRequestBody);
	});

	it('при нажатии «Перезагрузить настройки» вызывается fetchSettings снова', async () => {
		render(Settings);
		await waitFor(() => {
			expect(screen.getAllByTitle(/Перезагрузить настройки/i).length).toBeGreaterThan(0);
		});
		const callCount = vi.mocked(fetchSettings).mock.calls.length;
		const refreshBtn = screen.getAllByTitle(/Перезагрузить настройки/i)[0];
		await userEvent.setup().click(refreshBtn);
		await waitFor(() => {
			expect(fetchSettings).toHaveBeenCalledTimes(callCount + 1);
		});
	});

	it('без query в URL рендерятся все секции с якорями', async () => {
		pageUrlRef.href = 'http://test/oc/ssvc';
		const { container } = render(Settings);
		await waitFor(() => {
			expect(screen.getByRole('heading', { level: 1, name: /Настройки SSVC/i })).toBeInTheDocument();
		});
		expect(container.querySelector('#oc-ssvc-section-general')).toBeTruthy();
		expect(container.querySelector('#oc-ssvc-section-speed')).toBeTruthy();
	});

	it('при ошибке updateSetting (false) показывается notifications.error', async () => {
		vi.mocked(updateSetting).mockResolvedValue(false);
		pageUrlRef.href = 'http://test/oc/ssvc?tab=general';
		render(Settings);

		await waitFor(() => {
			expect(screen.getAllByText('Гистерезис при отборе тела').length).toBeGreaterThan(0);
		});
		const hystItem = screen.getAllByText('Гистерезис при отборе тела')[0].closest('.settings-item');
		await userEvent.setup().click(hystItem!.querySelector('button.edit-button')!);
		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));

		await waitFor(() => {
			expect(notifications.error).toHaveBeenCalledWith('Ошибка сохранения настроек', 5000);
		});
	});
});
