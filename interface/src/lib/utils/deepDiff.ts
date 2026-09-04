/**
 * Возвращает subset объекта `current`, содержащий только те ключи, значения
 * которых отличаются от соответствующих ключей в `original`.
 *
 * Используется для partial update в StartWizard: на контроллер ssvc0059v2
 * уходят только реально изменённые пользователем поля, чтобы не затирать
 * остальные настройки, если мастер запустился до полной синхронизации
 * локального зеркала с контроллером.
 *
 * Поддерживает:
 * - примитивы (number/string/boolean/null/undefined) — сравнение по значению
 * - вложенные объекты — рекурсивный diff с сохранением только изменённых ключей
 * - массивы — поэлементное сравнение (по индексам); если длина отличается
 *   или хотя бы один элемент изменился — массив целиком включается в результат
 *
 * Ключи, присутствующие в `current`, но отсутствующие в `original`, считаются
 * изменёнными (включаются в результат). Ключи, удалённые из `current`
 * относительно `original`, в результат НЕ включаются (semantics — merge).
 */
export function deepDiff<T extends Record<string, any>>(
	current: T,
	original: Partial<T> | undefined
): Partial<T> {
	if (original === undefined || original === null) {
		return { ...current };
	}

	const result: Record<string, any> = {};

	for (const key of Object.keys(current)) {
		const curVal = (current as any)[key];
		const origVal = (original as any)[key];

		if (!deepEqual(curVal, origVal)) {
			result[key] = isPlainObject(curVal) && isPlainObject(origVal)
				? deepDiff(curVal, origVal)
				: curVal;
		}
	}

	return result as Partial<T>;
}

export function deepEqual(a: any, b: any): boolean {
	if (a === b) return true;
	if (a === null || b === null || a === undefined || b === undefined) return a === b;
	if (typeof a !== typeof b) return false;

	if (Array.isArray(a) && Array.isArray(b)) {
		if (a.length !== b.length) return false;
		for (let i = 0; i < a.length; i++) {
			if (!deepEqual(a[i], b[i])) return false;
		}
		return true;
	}

	if (isPlainObject(a) && isPlainObject(b)) {
		const keysA = Object.keys(a);
		const keysB = Object.keys(b);
		if (keysA.length !== keysB.length) return false;
		for (const key of keysA) {
			if (!Object.prototype.hasOwnProperty.call(b, key)) return false;
			if (!deepEqual(a[key], b[key])) return false;
		}
		return true;
	}

	return false;
}

function isPlainObject(v: any): v is Record<string, any> {
	return v !== null && typeof v === 'object' && !Array.isArray(v);
}
