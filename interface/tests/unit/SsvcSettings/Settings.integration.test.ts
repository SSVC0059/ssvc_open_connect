/**
 * Интеграционный тест страницы Settings: проверка вызова updateSetting (REST API)
 * при сохранении из вкладки «Параметры отбора».
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor, within } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';
import { SPEED_SETTINGS_EXPECTED_REQUESTS } from '../../lib/speedSettingsExpectedRequests';

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
			const tabs = screen.getAllByRole('button', { name: /Параметры отбора/i });
			expect(tabs.length).toBeGreaterThanOrEqual(1);
		});

		const tabButtons = screen.getAllByRole('button', { name: /Параметры отбора/i });
		await userEvent.setup().click(tabButtons[0]);

		await waitFor(() => {
			expect(screen.getByText(headsSpec.sectionTitle)).toBeInTheDocument();
		});

		const section = screen.getByText(headsSpec.sectionTitle).closest('.settings-section');
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
			expect(fetchSettings).toHaveBeenCalled();
		});
		const callCount = vi.mocked(fetchSettings).mock.calls.length;
		const refreshBtn = screen.getByTitle(/Перезагрузить настройки/i);
		await userEvent.setup().click(refreshBtn);
		await waitFor(() => {
			expect(fetchSettings).toHaveBeenCalledTimes(callCount + 1);
		});
	});

	it('мобильное меню: открытие и переключение вкладки на «Параметры отбора»', async () => {
		render(Settings);
		await waitFor(() => {
			expect(screen.getByText('Гистерезис при отборе тела')).toBeInTheDocument();
		});
		const menuToggle = document.querySelector('.mobile-tabs-header .mobile-menu-toggle');
		expect(menuToggle).toBeTruthy();
		await userEvent.setup().click(menuToggle as HTMLElement);
		await waitFor(() => {
			const speedTab = screen.getAllByRole('button', { name: /^Параметры отбора$/i });
			expect(speedTab.length).toBeGreaterThanOrEqual(1);
		});
		const speedTabs = screen.getAllByRole('button', { name: /^Параметры отбора$/i });
		await userEvent.setup().click(speedTabs[speedTabs.length - 1]);
		await waitFor(() => {
			expect(screen.getByText(headsSpec.sectionTitle)).toBeInTheDocument();
		});
	});

	it('при ошибке updateSetting (false) показывается notifications.error', async () => {
		vi.mocked(updateSetting).mockResolvedValue(false);
		render(Settings);

		await waitFor(() => {
			expect(screen.getByText('Гистерезис при отборе тела')).toBeInTheDocument();
		});
		const hystItem = screen.getByText('Гистерезис при отборе тела').closest('.settings-item');
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
