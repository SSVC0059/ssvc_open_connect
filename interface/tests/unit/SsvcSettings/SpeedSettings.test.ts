/**
 * Тесты компонента «Параметры отбора» (SpeedSettings).
 * Проверяют корректность данных, отправляемых через onSave -> updateSetting (REST API).
 * Ожидаемые запросы заданы в одном месте: speedSettingsExpectedRequests.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor, within } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import SpeedSettings from '$lib/components/SsvcSettings/SpeedSettings.svelte';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';
import {
	SPEED_SETTINGS_EXPECTED_REQUESTS,
	type SpeedSettingsFieldKey
} from '../../lib/speedSettingsExpectedRequests';

/** Открыть модалку секции, нажать «Сохранить». Ожидания берутся из реестра по ключу. */
async function saveSectionWithoutChanges(key: SpeedSettingsFieldKey, settings: ReturnType<typeof createMockSsvcSettings>) {
	const spec = SPEED_SETTINGS_EXPECTED_REQUESTS[key];
	const section = screen.getByText(spec.sectionTitle).closest('.settings-section');
	const editBtn = section?.querySelector('button.edit-button, button.edit_button');
	expect(editBtn).toBeTruthy();
	await userEvent.setup().click(editBtn!);
	await waitFor(() => {
		expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
	});
	const dialog = screen.getByRole('dialog', { hidden: true });
	await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));
	return spec;
}

describe('SpeedSettings (Параметры отбора)', () => {
	let mockOnSave: ReturnType<typeof vi.fn<(field: string, value: unknown) => void>>;

	beforeEach(() => {
		mockOnSave = vi.fn<(field: string, value: unknown) => void>();
	});

	it('при сохранении параметров голов с изменением значений вызывает onSave с полем и введённым значением', async () => {
		const spec = SPEED_SETTINGS_EXPECTED_REQUESTS.heads;
		const settings = createMockSsvcSettings({ heads: spec.expectedRequestBody });
		render(SpeedSettings, { props: { settings, onSave: mockOnSave } });

		const editButtons = screen.getAllByRole('button').filter((btn) => !btn.textContent?.includes('Перезагрузить'));
		await userEvent.setup().click(editButtons[0]);
		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		const inputs = within(dialog).getAllByRole('spinbutton', { hidden: true });
		expect(inputs.length).toBeGreaterThanOrEqual(2);
		await userEvent.setup().clear(inputs[0]);
		await userEvent.setup().type(inputs[0], '2.5');
		await userEvent.setup().clear(inputs[1]);
		await userEvent.setup().type(inputs[1], '15');
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledTimes(1);
		});
		expect(mockOnSave).toHaveBeenCalledWith(spec.field, [2.5, 15]);
	});

	it('при сохранении без изменений передаёт тело запроса из реестра (heads)', async () => {
		const spec = SPEED_SETTINGS_EXPECTED_REQUESTS.heads;
		const settings = createMockSsvcSettings({ heads: spec.expectedRequestBody });
		render(SpeedSettings, { props: { settings, onSave: mockOnSave } });

		const editButtons = screen.getAllByRole('button').filter((btn) => !btn.textContent?.includes('Перезагрузить'));
		await userEvent.setup().click(editButtons[0]);
		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('тело запроса для heads задаётся в реестре (expectedRequestBody)', async () => {
		const spec = SPEED_SETTINGS_EXPECTED_REQUESTS.heads;
		const settings = createMockSsvcSettings({ heads: spec.expectedRequestBody });
		render(SpeedSettings, { props: { settings, onSave: mockOnSave } });

		const editButtons = screen.getAllByRole('button').filter((btn) => !btn.textContent?.includes('Перезагрузить'));
		await userEvent.setup().click(editButtons[0]);
		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledTimes(1);
		});
		expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
	});

	it('при сохранении параметров подголовников тело запроса из реестра (late_heads)', async () => {
		const spec = SPEED_SETTINGS_EXPECTED_REQUESTS.late_heads;
		const settings = createMockSsvcSettings({ late_heads: spec.expectedRequestBody });
		render(SpeedSettings, { props: { settings, onSave: mockOnSave } });
		await saveSectionWithoutChanges('late_heads', settings);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении параметров тела тело запроса из реестра (hearts)', async () => {
		const spec = SPEED_SETTINGS_EXPECTED_REQUESTS.hearts;
		const settings = createMockSsvcSettings({ hearts: spec.expectedRequestBody });
		render(SpeedSettings, { props: { settings, onSave: mockOnSave } });
		await saveSectionWithoutChanges('hearts', settings);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении параметров хвостов тело запроса из реестра (tails)', async () => {
		const spec = SPEED_SETTINGS_EXPECTED_REQUESTS.tails;
		const settings = createMockSsvcSettings({ tails: spec.expectedRequestBody });
		render(SpeedSettings, { props: { settings, onSave: mockOnSave } });
		await saveSectionWithoutChanges('tails', settings);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при нажатии «Отменить» в модалке не вызывает onSave', async () => {
		const spec = SPEED_SETTINGS_EXPECTED_REQUESTS.heads;
		const settings = createMockSsvcSettings({ heads: spec.expectedRequestBody });
		render(SpeedSettings, { props: { settings, onSave: mockOnSave } });

		const editButtons = screen.getAllByRole('button').filter((btn) => !btn.textContent?.includes('Перезагрузить'));
		await userEvent.setup().click(editButtons[0]);
		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Отменить/i, hidden: true }));

		expect(mockOnSave).not.toHaveBeenCalled();
	});
});
