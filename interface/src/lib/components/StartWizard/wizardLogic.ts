// WizardSettings/logic.ts
import { fetchSettings } from '$lib/api/settingsApi';
import type { SsvcSettings } from '$lib/types/models';

export async function loadSettings(): Promise<SsvcSettings | null> {
	return await fetchSettings();
}
