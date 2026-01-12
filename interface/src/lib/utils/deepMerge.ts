import type { Profile } from '$lib/types/ssvc';
import { createDefaultProfile } from '$lib/actions/DistillationCycleModel';

export function isObject(item: any): item is Record<string, any> {
	return (item && typeof item === 'object' && !Array.isArray(item));
}

export function deepMerge(target: any, source: any): any {
	const output = { ...target };
	if (isObject(target) && isObject(source)) {
		Object.keys(source).forEach((key) => {
			if (isObject(source[key]) && key in target && isObject(target[key])) {
				output[key] = deepMerge(target[key], source[key]);
			} else {
				output[key] = source[key];
				output[key] = source[key];
			}
		});
	}
	return output;
}
/**
 * Нормализует профиль, гарантируя наличие всех фракций и настроек SSVC.
 * Если фракция отключена, её параметры обнуляются для корректного расчета.
 */
export function normalizeProfile(savedData: Partial<Profile>): Profile {
	const base = createDefaultProfile();

	const merged: Profile = {
		...base,
		...savedData,

		// Гарантируем наличие вложенных объектов фракций
		heads: mergeWithDefaults(savedData.heads, base.heads),
		late_heads: mergeWithDefaults(savedData.late_heads, base.late_heads),
		hearts: mergeWithDefaults(savedData.hearts, base.hearts),
		tails: mergeWithDefaults(savedData.tails, base.tails),

		// ИСПРАВЛЕНИЕ: Глубокое слияние для ssvcSettings
		ssvcSettings: {
			...base.ssvcSettings,
			...(savedData.ssvcSettings || {})
		}
	};

	// Обработка отключенных фракций для математической модели
	// Используем опциональную цепочку ?. для безопасности
	if (!merged.late_heads?.enabled) {
		if (merged.late_heads) {
			merged.late_heads.percent = 0;
			merged.late_heads.targetFlowMlh = 0;
		}
	}

	if (!merged.tails?.enabled) {
		if (merged.tails) {
			merged.tails.percent = 0;
			merged.tails.targetFlowMlh = 0;
		}
	}

	return merged;
}

/**
 * Вспомогательная функция: если значение в объекте 'data' отсутствует,
 * берет значение из 'defaults'.
 */
function mergeWithDefaults(data: any, defaults: any) {
	if (!data) return { ...defaults };

	const result = { ...defaults, ...data };

	// Дополнительная проверка на "пустые" значения (0 или пустые строки),
	// если вы хотите заменять нули на дефолты:
	for (const key in defaults) {
		if (result[key] === 0 || result[key] === undefined || result[key] === null) {
			result[key] = defaults[key];
		}
	}

	return result;
}