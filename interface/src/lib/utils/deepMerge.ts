import type { OpenConnectRelayRules, Profile } from '$lib/types/ssvc';
import { createDefaultProfile } from '$lib/actions/DistillationCycleModel';
import { sanitizeIncomingProfileData } from './profileSanitize';

export { sanitizeIncomingProfileData, stripNumericKeysAtRoot } from './profileSanitize';

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
	const incoming = sanitizeIncomingProfileData(savedData as unknown);

	const merged: Profile = {
		...base,
		...incoming,

		// Гарантируем наличие вложенных объектов фракций
		heads: mergeWithDefaults(incoming.heads, base.heads),
		late_heads: mergeWithDefaults(incoming.late_heads, base.late_heads),
		hearts: mergeWithDefaults(incoming.hearts, base.hearts),
		tails: mergeWithDefaults(incoming.tails, base.tails),

		ssvcSettings: mergeWithDefaults(incoming.ssvcSettings, base.ssvcSettings),

		openConnectRelayRules: normalizeRelayRules(
			incoming.openConnectRelayRules,
			base.openConnectRelayRules as OpenConnectRelayRules
		)
	};

	// Обработка отключенных фракций для математической модели
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
function normalizeRelayRules(
	incoming: Partial<OpenConnectRelayRules> | undefined,
	base: OpenConnectRelayRules
): OpenConnectRelayRules {
	return {
		schemaVersion: incoming?.schemaVersion ?? base.schemaVersion ?? 1,
		rules: Array.isArray(incoming?.rules) ? incoming.rules : base.rules ?? []
	};
}

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

export function deepClone<T>(obj: T): T {
    if (obj === null || typeof obj !== 'object') {
        return obj;
    }

    if (obj instanceof Date) {
        return new Date(obj.getTime()) as any;
    }

    if (Array.isArray(obj)) {
        const arrCopy = [] as any[];
        for (const item of obj) {
            arrCopy.push(deepClone(item));
        }
        return arrCopy as any;
    }

    const objCopy = {} as { [key: string]: any };
    for (const key in obj) {
        if (Object.prototype.hasOwnProperty.call(obj, key)) {
            objCopy[key] = deepClone((obj as any)[key]);
        }
    }

    return objCopy as T;
}
