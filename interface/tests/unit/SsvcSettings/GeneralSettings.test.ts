/**
 * Тесты компонента «Общие» (GeneralSettings).
 * Проверяют корректность данных, отправляемых через onSave -> updateSetting (REST API).
 * Ожидаемые запросы заданы в generalSettingsExpectedRequests.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor, within } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import GeneralSettings from '$lib/components/SsvcSettings/GeneralSettings.svelte';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';
import { GENERAL_SETTINGS_EXPECTED_REQUESTS } from '../../lib/generalSettingsExpectedRequests';

/** Открыть модалку по подписи поля, нажать «Сохранить». */
async function saveModalByLabel(label: string) {
	const item = screen.getByText(label).closest('.settings-item');
	const editBtn = item?.querySelector('button.edit-button');
	expect(editBtn).toBeTruthy();
	await userEvent.setup().click(editBtn!);
	await waitFor(() => {
		expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
	});
	const dialog = screen.getByRole('dialog', { hidden: true });
	await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));
}

/** Открыть модалку по подписи, нажать «Отменить». */
async function cancelModalByLabel(label: string) {
	const item = screen.getByText(label).closest('.settings-item');
	const editBtn = item?.querySelector('button.edit-button');
	expect(editBtn).toBeTruthy();
	await userEvent.setup().click(editBtn!);
	await waitFor(() => {
		expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
	});
	const dialog = screen.getByRole('dialog', { hidden: true });
	await userEvent.setup().click(within(dialog).getByRole('button', { name: /Отменить/i, hidden: true }));
}

describe('GeneralSettings (Общие)', () => {
	let mockOnSave: ReturnType<typeof vi.fn<(field: string, value: unknown) => void>>;

	beforeEach(() => {
		mockOnSave = vi.fn<(field: string, value: unknown) => void>();
	});

	it('при сохранении гистерезиса вызывает onSave с полем и значением из реестра', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.hyst;
		const settings = createMockSsvcSettings({ hyst: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении декремента вызывает onSave согласно реестру', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.decrement;
		const settings = createMockSsvcSettings({ decrement: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении времени отбора голов вызывает onSave согласно реестру (heads_timer)', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.heads_timer;
		const settings = createMockSsvcSettings({ heads_timer: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении таймера фиксации тела вызывает onSave согласно реестру (hearts_timer)', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.hearts_timer;
		const settings = createMockSsvcSettings({ hearts_timer: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении поправки давления в кубе вызывает onSave согласно реестру (tank_mmhg)', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.tank_mmhg;
		const settings = createMockSsvcSettings({ tank_mmhg: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении температуры завершения тела вызывает onSave согласно реестру (tails_temp)', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.tails_temp;
		const settings = createMockSsvcSettings({ tails_temp: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при переключении чекбокса «Формула» вызывает onSave с полем formula и значением из реестра', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.formula;
		const settings = createMockSsvcSettings({ formula: false });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		const item = screen.getByText(spec.label).closest('.settings-item');
		const checkbox = item?.querySelector('input[type="checkbox"]');
		expect(checkbox).toBeTruthy();
		await userEvent.setup().click(checkbox!);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении температуры начала формулы вызывает onSave согласно реестру', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.formula_start_temp;
		const settings = createMockSsvcSettings({ formula_start_temp: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении времени сброса вызывает onSave согласно реестру (секция «Сброс и снижение»)', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.release_timer;
		const settings = createMockSsvcSettings({
			release_timer: 60,
			release_speed: 5,
			heads_final: 2.5
		});
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении скорости сброса вызывает onSave согласно реестру', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.release_speed;
		const settings = createMockSsvcSettings({
			release_timer: 60,
			release_speed: spec.expectedRequestBody as number,
			heads_final: 2.5
		});
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении скорости отбора к окончанию голов вызывает onSave согласно реестру (heads_final)', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.heads_final;
		const settings = createMockSsvcSettings({
			release_timer: 60,
			release_speed: 5,
			heads_final: spec.expectedRequestBody as number
		});
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await saveModalByLabel(spec.label);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при нажатии «Отменить» в модалке гистерезиса не вызывает onSave', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.hyst;
		const settings = createMockSsvcSettings({ hyst: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await cancelModalByLabel(spec.label);
		expect(mockOnSave).not.toHaveBeenCalled();
	});

	it('при нажатии «Отменить» в модалке времени отбора голов не вызывает onSave', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.heads_timer;
		const settings = createMockSsvcSettings({ heads_timer: spec.expectedRequestBody as number });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		await cancelModalByLabel(spec.label);
		expect(mockOnSave).not.toHaveBeenCalled();
	});

	it('при значении гистерезиса выше max кнопка «Сохранить» недоступна', async () => {
		const spec = GENERAL_SETTINGS_EXPECTED_REQUESTS.hyst;
		const settings = createMockSsvcSettings({ hyst: 1 });
		render(GeneralSettings, { props: { settings, onSave: mockOnSave } });
		const item = screen.getByText(spec.label).closest('.settings-item');
		await userEvent.setup().click(item!.querySelector('button.edit-button')!);
		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		const input = within(dialog).getByRole('spinbutton', { hidden: true });
		await userEvent.setup().clear(input);
		await userEvent.setup().type(input, '51'); // max 50
		const saveBtn = within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true });
		expect(saveBtn).toBeDisabled();
		await userEvent.setup().clear(input);
		await userEvent.setup().type(input, '50');
		await waitFor(() => {
			expect(saveBtn).not.toBeDisabled();
		});
		await userEvent.setup().click(saveBtn);
		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith('hyst', 50);
		});
	});
});
