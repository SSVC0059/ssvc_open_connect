import type { Profile } from '$lib/types/ssvc';

/**
 * Убирает артефакты вида { 0: ..., 1: ... }, появляющиеся при `...` массива в объект.
 * Также отбрасывает корневой массив ответа API, если по ошибке пришёл `[profile]`.
 */
export function sanitizeIncomingProfileData(data: unknown): Partial<Profile> {
	if (data == null || typeof data !== 'object') {
		return {};
	}
	let raw: Record<string, unknown>;
	if (Array.isArray(data)) {
		const first = data[0];
		if (first && typeof first === 'object' && !Array.isArray(first)) {
			raw = first as Record<string, unknown>;
		} else {
			return {};
		}
	} else {
		raw = data as Record<string, unknown>;
	}
	const cleaned: Record<string, unknown> = {};
	for (const k of Object.keys(raw)) {
		if (/^\d+$/.test(k)) {
			continue;
		}
		cleaned[k] = raw[k];
	}
	return cleaned as Partial<Profile>;
}

/** Удаляет из корня объекта ключи "0","1",… (артефакт spread массива в объект). */
export function stripNumericKeysAtRoot<T extends Record<string, unknown>>(obj: T): T {
	const out = { ...obj };
	for (const k of Object.keys(out)) {
		if (/^\d+$/.test(k)) {
			delete (out as Record<string, unknown>)[k];
		}
	}
	return out as T;
}
