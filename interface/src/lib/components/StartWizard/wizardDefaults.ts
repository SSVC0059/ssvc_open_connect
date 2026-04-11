import type { SsvcSettings } from '$lib/types/ssvc';

/**
 * Нормализует настройки для мастера запуска ректификации.
 * Issue #75: поле «Время сброса» должно быть 0 при открытии мастера, а не -1.
 */
export function normalizeSettingsForWizard(
	settings: SsvcSettings | undefined
): SsvcSettings | undefined {
	if (!settings) return undefined;
	if (settings.release_timer < 0) {
		return { ...settings, release_timer: 0 };
	}
	return settings;
}
