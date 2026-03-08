/**
 * Тесты компонента «Клапаны» (ValveBandwidth).
 * Проверяют корректность данных, отправляемых через onSave -> updateSetting (REST API).
 * Ожидаемые запросы заданы в valveBandwidthExpectedRequests.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor, within } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import ValveBandwidth from '$lib/components/SsvcSettings/ValveBandwidth.svelte';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';
import { VALVE_BANDWIDTH_EXPECTED_REQUESTS } from '../../lib/valveBandwidthExpectedRequests';

describe('ValveBandwidth (Клапаны)', () => {
	let mockOnSave: ReturnType<typeof vi.fn<(field: string, value: unknown) => void>>;

	beforeEach(() => {
		mockOnSave = vi.fn<(field: string, value: unknown) => void>();
	});

	it('при сохранении пропускной способности клапанов вызывает onSave с полем valve_bw и массивом из реестра', async () => {
		const spec = VALVE_BANDWIDTH_EXPECTED_REQUESTS.valve_bw;
		const settings = createMockSsvcSettings({ valve_bw: spec.expectedRequestBody });
		render(ValveBandwidth, { props: { settings, onSave: mockOnSave } });

		const section = screen.getByText(spec.sectionTitle).closest('.settings-section');
		const editBtn = section?.querySelector('button.edit-button, button.edit_button');
		expect(editBtn).toBeTruthy();
		await userEvent.setup().click(editBtn!);

		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении с изменёнными значениями отправляет введённый массив [головы, тело, подголовники|хвосты]', async () => {
		const spec = VALVE_BANDWIDTH_EXPECTED_REQUESTS.valve_bw;
		const settings = createMockSsvcSettings({ valve_bw: [1, 2, 3] });
		render(ValveBandwidth, { props: { settings, onSave: mockOnSave } });

		const section = screen.getByText(spec.sectionTitle).closest('.settings-section');
		const editBtn = section?.querySelector('button.edit-button, button.edit_button');
		await userEvent.setup().click(editBtn!);

		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		const inputs = within(dialog).getAllByRole('spinbutton', { hidden: true });
		expect(inputs.length).toBeGreaterThanOrEqual(3);
		await userEvent.setup().clear(inputs[0]);
		await userEvent.setup().type(inputs[0], '500');
		await userEvent.setup().clear(inputs[1]);
		await userEvent.setup().type(inputs[1], '1000');
		await userEvent.setup().clear(inputs[2]);
		await userEvent.setup().type(inputs[2], '750');
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledTimes(1);
		});
		expect(mockOnSave).toHaveBeenCalledWith(spec.field, [500, 1000, 750]);
	});

	it('при нажатии «Отменить» не вызывает onSave', async () => {
		const spec = VALVE_BANDWIDTH_EXPECTED_REQUESTS.valve_bw;
		const settings = createMockSsvcSettings({ valve_bw: spec.expectedRequestBody });
		render(ValveBandwidth, { props: { settings, onSave: mockOnSave } });

		const section = screen.getByText(spec.sectionTitle).closest('.settings-section');
		await userEvent.setup().click(section!.querySelector('button.edit-button, button.edit_button')!);
		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Отменить/i, hidden: true }));
		expect(mockOnSave).not.toHaveBeenCalled();
	});
});
