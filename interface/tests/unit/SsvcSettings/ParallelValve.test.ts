/**
 * Тесты компонента «Параллельный отбор» (ParallelValve).
 * Проверяют корректность данных, отправляемых через onSave -> updateSetting (REST API).
 * Ожидаемые запросы заданы в parallelValveExpectedRequests.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, waitFor, within } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import ParallelValve from '$lib/components/SsvcSettings/ParallelValve.svelte';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';
import { PARALLEL_VALVE_EXPECTED_REQUESTS } from '../../lib/parallelValveExpectedRequests';

/** Найти секцию по заголовку; при нескольких — по индексу (0-based). */
function getSectionByTitle(title: string, index = 0): Element | null {
	const headings = screen.getAllByText(title);
	const el = headings[index];
	return el?.closest('.settings-section') ?? null;
}

/** Открыть модалку в секции, нажать «Сохранить». */
async function saveModalInSection(section: Element | null) {
	const editBtn = section?.querySelector('button');
	expect(editBtn).toBeTruthy();
	await userEvent.setup().click(editBtn!);
	await waitFor(() => {
		expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
	});
	const dialog = screen.getByRole('dialog', { hidden: true });
	await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));
}

/** Открыть модалку в секции, нажать «Отменить». */
async function cancelModalInSection(section: Element | null) {
	const editBtn = section?.querySelector('button');
	expect(editBtn).toBeTruthy();
	await userEvent.setup().click(editBtn!);
	await waitFor(() => {
		expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
	});
	const dialog = screen.getByRole('dialog', { hidden: true });
	await userEvent.setup().click(within(dialog).getByRole('button', { name: /Отменить/i, hidden: true }));
}

describe('ParallelValve (Параллельный отбор)', () => {
	let mockOnSave: ReturnType<typeof vi.fn<(field: string, value: unknown) => void>>;

	beforeEach(() => {
		mockOnSave = vi.fn<(field: string, value: unknown) => void>();
	});

	it('при сохранении параллельного отбора клапаном №1 вызывает onSave с полем parallel_v1 из реестра', async () => {
		const spec = PARALLEL_VALVE_EXPECTED_REQUESTS.parallel_v1;
		const settings = createMockSsvcSettings({ parallel_v1: spec.expectedRequestBody });
		render(ParallelValve, { props: { settings, onSave: mockOnSave } });

		const section = getSectionByTitle(spec.sectionTitle);
		expect(section).toBeTruthy();
		await saveModalInSection(section);

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении параллельного отбора клапаном №3 (parallel_v3) вызывает onSave с массивом точек из реестра', async () => {
		const spec = PARALLEL_VALVE_EXPECTED_REQUESTS.parallel_v3;
		const settings = createMockSsvcSettings({ parallel_v3: spec.expectedRequestBody as [number, number, number][] });
		render(ParallelValve, { props: { settings, onSave: mockOnSave } });

		const section = getSectionByTitle(spec.sectionTitle, spec.sectionIndex ?? 0);
		expect(section).toBeTruthy();
		await saveModalInSection(section);

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при сохранении parallel (вторая секция «клапаном №3») вызывает onSave с полем parallel из реестра', async () => {
		const spec = PARALLEL_VALVE_EXPECTED_REQUESTS.parallel;
		const settings = createMockSsvcSettings({
			parallel_v3: [[84, 1.2, 10]],
			parallel: spec.expectedRequestBody
		});
		render(ParallelValve, { props: { settings, onSave: mockOnSave } });

		const section = getSectionByTitle(spec.sectionTitle, spec.sectionIndex ?? 1);
		expect(section).toBeTruthy();
		await saveModalInSection(section);

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledWith(spec.field, spec.expectedRequestBody);
		});
	});

	it('при нажатии «Отменить» в модалке parallel_v1 не вызывает onSave', async () => {
		const spec = PARALLEL_VALVE_EXPECTED_REQUESTS.parallel_v1;
		const settings = createMockSsvcSettings({ parallel_v1: spec.expectedRequestBody });
		render(ParallelValve, { props: { settings, onSave: mockOnSave } });
		const section = getSectionByTitle(spec.sectionTitle);
		await cancelModalInSection(section);
		expect(mockOnSave).not.toHaveBeenCalled();
	});

	it('при нажатии «Отменить» в модалке parallel_v3 не вызывает onSave', async () => {
		const spec = PARALLEL_VALVE_EXPECTED_REQUESTS.parallel_v3;
		const settings = createMockSsvcSettings({ parallel_v3: spec.expectedRequestBody as [number, number, number][] });
		render(ParallelValve, { props: { settings, onSave: mockOnSave } });
		const section = getSectionByTitle(spec.sectionTitle, spec.sectionIndex ?? 0);
		await cancelModalInSection(section);
		expect(mockOnSave).not.toHaveBeenCalled();
	});

	it('при изменении точки в parallel_v3 и сохранении вызывает onSave с обновлённым массивом', async () => {
		const initial: [number, number, number][] = [[84, 1.2, 10]];
		const settings = createMockSsvcSettings({ parallel_v3: initial });
		render(ParallelValve, { props: { settings, onSave: mockOnSave } });

		const section = getSectionByTitle('Скорость параллельного отбора клапаном №3', 0);
		const editBtn = section?.querySelector('button');
		await userEvent.setup().click(editBtn!);
		await waitFor(() => {
			expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();
		});
		const dialog = screen.getByRole('dialog', { hidden: true });
		const inputs = within(dialog).getAllByRole('spinbutton', { hidden: true });
		expect(inputs.length).toBeGreaterThanOrEqual(3);
		await userEvent.setup().clear(inputs[0]);
		await userEvent.setup().type(inputs[0], '90');
		await userEvent.setup().click(within(dialog).getByRole('button', { name: /Сохранить/i, hidden: true }));

		await waitFor(() => {
			expect(mockOnSave).toHaveBeenCalledTimes(1);
		});
		expect(mockOnSave).toHaveBeenCalledWith('parallel_v3', [[90, 1.2, 10]]);
	});
});
