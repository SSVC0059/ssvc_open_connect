import { describe, it, expect } from 'vitest';
import { deepDiff } from '$lib/utils/deepDiff';

describe('deepDiff', () => {
	it('возвращает пустой объект, если ничего не изменилось', () => {
		const baseline = { heads: [1.5, 10], hyst: 0.25, sound: 1 };
		expect(deepDiff(structuredClone(baseline), baseline)).toEqual({});
	});

	it('возвращает только изменённые примитивы', () => {
		const baseline = { heads: [1.5, 10], hyst: 0.25, sound: 1 };
		const current = { ...baseline, hyst: 0.5 };

		expect(deepDiff(current, baseline)).toEqual({ hyst: 0.5 });
	});

	it('сравнивает массивы целиком', () => {
		const baseline = { heads: [1.5, 10], valve_bw: [10, 20, 30] };
		const current = { ...baseline, heads: [1.5, 12] };

		expect(deepDiff(current, baseline)).toEqual({ heads: [1.5, 12] });
	});

	it('рекурсивно обходит вложенные объекты', () => {
		const baseline = { nested: { a: 1, b: 2 }, flat: 0 };
		const current = { nested: { a: 1, b: 3 }, flat: 0 };

		expect(deepDiff(current, baseline)).toEqual({ nested: { b: 3 } });
	});

	it('без baseline возвращает все поля текущего состояния', () => {
		const current = { hyst: 0.25, sound: 1 };
		expect(deepDiff(current, undefined)).toEqual(current);
	});

	it('без текущего состояния не возвращает ничего', () => {
		expect(deepDiff(undefined, { hyst: 0.25 })).toEqual({});
	});
});
