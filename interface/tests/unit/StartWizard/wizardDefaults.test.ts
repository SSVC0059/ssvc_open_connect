/**
 * Юнит-тесты для нормализации настроек мастера (issue #75).
 */
import { describe, it, expect } from 'vitest';
import { normalizeSettingsForWizard } from '$lib/components/StartWizard/wizardDefaults';
import { createMockSsvcSettings } from '../../lib/createMockSsvcSettings';

describe('normalizeSettingsForWizard', () => {
	it('подменяет release_timer -1 на 0 (issue #75)', () => {
		const settings = createMockSsvcSettings({ release_timer: -1 });
		const result = normalizeSettingsForWizard(settings);
		expect(result).toBeDefined();
		expect(result!.release_timer).toBe(0);
	});

	it('оставляет release_timer 0 без изменений', () => {
		const settings = createMockSsvcSettings({ release_timer: 0 });
		const result = normalizeSettingsForWizard(settings);
		expect(result).toBeDefined();
		expect(result!.release_timer).toBe(0);
	});

	it('оставляет положительный release_timer без изменений', () => {
		const settings = createMockSsvcSettings({ release_timer: 120 });
		const result = normalizeSettingsForWizard(settings);
		expect(result).toBeDefined();
		expect(result!.release_timer).toBe(120);
	});

	it('возвращает undefined при undefined на входе', () => {
		expect(normalizeSettingsForWizard(undefined)).toBeUndefined();
	});
});
