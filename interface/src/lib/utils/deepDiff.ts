type PlainObject = Record<string, unknown>;

function isPlainObject(value: unknown): value is PlainObject {
	return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function isEqual(left: unknown, right: unknown): boolean {
	if (left === right) return true;

	if (Array.isArray(left) && Array.isArray(right)) {
		return left.length === right.length && left.every((item, index) => isEqual(item, right[index]));
	}

	if (isPlainObject(left) && isPlainObject(right)) {
		const leftKeys = Object.keys(left);
		const rightKeys = Object.keys(right);
		return (
			leftKeys.length === rightKeys.length &&
			leftKeys.every((key) => isEqual(left[key], right[key]))
		);
	}

	return false;
}

/**
 * Возвращает только те поля `current`, которые отличаются от `baseline`.
 *
 * Нужен для частичного обновления настроек: контроллер — источник истины, и
 * отправлять ему значения, которые пользователь не менял, нельзя. Иначе
 * настройка, изменённая на самом контроллере после открытия формы, будет
 * затёрта устаревшим значением из интерфейса.
 *
 * Массивы сравниваются целиком (это кортежи параметров), вложенные объекты —
 * рекурсивно. Удаление ключей не выражается: набор параметров фиксирован.
 */
export function deepDiff<T extends object>(
	current: T | undefined,
	baseline: T | undefined
): Partial<T> {
	if (!current) return {};
	if (!baseline) return { ...current };

	const diff: PlainObject = {};
	const currentRecord = current as PlainObject;
	const baselineRecord = baseline as PlainObject;

	for (const [key, value] of Object.entries(currentRecord)) {
		const previous = baselineRecord[key];

		if (isPlainObject(value) && isPlainObject(previous)) {
			const nested = deepDiff(value, previous);
			if (Object.keys(nested).length > 0) {
				diff[key] = nested;
			}
			continue;
		}

		if (!isEqual(value, previous)) {
			diff[key] = value;
		}
	}

	return diff as Partial<T>;
}
