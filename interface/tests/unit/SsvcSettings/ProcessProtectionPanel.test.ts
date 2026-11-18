/**
 * Тесты панели «Защита от стопа»: реальные пороги по телеметрии,
 * предупреждения о паре «предекремент + гистерезис» и о «Авто 92+».
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/svelte';
import userEvent from '@testing-library/user-event';
import ProcessProtectionPanel from '$lib/components/SsvcSettings/ProcessProtectionPanel.svelte';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';

describe('ProcessProtectionPanel (Защита от стопа)', () => {
	let mockOnSave: ReturnType<typeof vi.fn<(field: string, value: unknown) => void>>;

	beforeEach(() => {
		mockOnSave = vi.fn<(field: string, value: unknown) => void>();
	});

	it('с известной Т отбора показывает пороги стопа и предекремента числами', () => {
		const settings = createMockSsvcSettings({ hyst: 0.25, predec: 13, formula: 1 });
		const { container } = render(ProcessProtectionPanel, {
			props: { settings, onSave: mockOnSave, tp1Target: 78.4 }
		});

		const text = container.textContent ?? '';
		expect(text).toContain('78,65'); // стоп = 78,4 + 0,25
		expect(text).toContain('78,52'); // предекремент = 78,65 − 0,13
		expect(text).toContain('92 → 99 °C');
	});

	it('без телеметрии показывает пороги формулой в символах, без выдуманных чисел', () => {
		const settings = createMockSsvcSettings({ hyst: 0.25, predec: 13 });
		const { container } = render(ProcessProtectionPanel, {
			props: { settings, onSave: mockOnSave }
		});

		expect(container.textContent).toContain('Т + 0,25 °C');
		expect(container.textContent).toContain('стоп − 0,13 °C');
	});

	it('при недостаточном гистерезисе предупреждает и предлагает поднять его до 0,18', async () => {
		const settings = createMockSsvcSettings({ hyst: 0.1, predec: 13 });
		const { container } = render(ProcessProtectionPanel, {
			props: { settings, onSave: mockOnSave, tp1Target: 78.4 }
		});

		expect(container.textContent).toContain('контроллер отключит предекремент');
		expect(container.textContent).toContain('Предекремент не сработает');

		const button = screen.getByRole('button', { name: 'Поднять гистерезис до 0,18 °C' });
		await userEvent.setup().click(button);
		expect(mockOnSave).toHaveBeenCalledWith('hyst', 0.18);
	});

	it('для «Авто 92+» без предекремента предупреждает и предлагает включить 0,07', async () => {
		const settings = createMockSsvcSettings({ formula: 2, predec: 0 });
		render(ProcessProtectionPanel, {
			props: { settings, onSave: mockOnSave, tp1Target: 78.4 }
		});

		expect(
			screen.getByText(/Авто 92\+ не сработает без предекремента/)
		).toBeInTheDocument();

		const button = screen.getByRole('button', { name: 'Включить предекремент 0,07' });
		await userEvent.setup().click(button);
		expect(mockOnSave).toHaveBeenCalledWith('predec', 7);
	});
});
