import { describe, it, expect } from 'vitest';
import {
	FORMULA_END_TEMP,
	FORMULA_START_TEMP_DEFAULT,
	formulaModeHint,
	formulaModeLabel,
	formulaStartTempForMode,
	minHysteresisFor,
	normalizeFormulaStartTemp,
	predecEventDelta,
	predecLabel,
	predecState,
	predecVerdict,
	predecVerdictHint,
	predecWillWork,
	protectionThresholds
} from '$lib/actions/rectificationRules';

describe('rectificationRules: минимальный гистерезис', () => {
	it('возвращает минимум для включённого предекремента и null для выключенного', () => {
		expect(minHysteresisFor(7)).toBe(0.12);
		expect(minHysteresisFor(13)).toBe(0.18);
		expect(minHysteresisFor(0)).toBeNull();
		expect(minHysteresisFor(5)).toBeNull();
		expect(minHysteresisFor(undefined)).toBeNull();
	});

	it('считает ровно минимальный гистерезис достаточным', () => {
		expect(predecState(7, 0.12)).toBe('ok');
		expect(predecState(13, 0.18)).toBe('ok');
	});

	it('отличает нехватку гистерезиса от неизвестного значения', () => {
		expect(predecState(7, 0.1199)).toBe('hysteresis-too-small');
		expect(predecState(13, 0.17)).toBe('hysteresis-too-small');
		expect(predecState(7, undefined)).toBe('unknown');
		expect(predecState(13, Number.NaN)).toBe('unknown');
	});

	it('не считает выключенный предекремент проблемой', () => {
		expect(predecState(0, 0.06)).toBe('off');
		expect(predecWillWork(0, 0.25)).toBe(false);
		expect(predecWillWork(13, 0.18)).toBe(true);
		expect(predecWillWork(7, 0.11)).toBe(false);
	});

	it('подписывает значения predec для селекта', () => {
		expect(predecLabel(0)).toBe('Выкл');
		expect(predecLabel(7)).toBe('0,07 °C');
		expect(predecLabel(13)).toBe('0,13 °C');
	});
});

describe('rectificationRules: пороги защиты', () => {
	it('считает стоп и предекремент от Т отбора', () => {
		expect(protectionThresholds(78.4, 0.25, 13)).toEqual({
			stopAt: 78.65,
			predecAt: 78.52
		});
	});

	it('округляет до сотых, как на дисплее', () => {
		expect(protectionThresholds(78.41, 0.25, 7)).toEqual({
			stopAt: 78.66,
			predecAt: 78.59
		});
	});

	it('не показывает порог предекремента, если гистерезиса недостаточно', () => {
		expect(protectionThresholds(78.4, 0.1, 13)).toEqual({
			stopAt: 78.5,
			predecAt: null
		});
	});

	it('не показывает порог предекремента, если он выключен', () => {
		expect(protectionThresholds(78.4, 0.25, 0)).toEqual({
			stopAt: 78.65,
			predecAt: null
		});
	});

	it('возвращает null без Т отбора или гистерезиса', () => {
		expect(protectionThresholds(undefined, 0.25, 13)).toBeNull();
		expect(protectionThresholds(78.4, undefined, 13)).toBeNull();
		expect(protectionThresholds(Number.NaN, 0.25, 13)).toBeNull();
	});
});

describe('rectificationRules: температура начала формулы', () => {
	it('нормализует пустое значение в 92 °C', () => {
		expect(normalizeFormulaStartTemp(0)).toBe(FORMULA_START_TEMP_DEFAULT);
		expect(normalizeFormulaStartTemp(undefined)).toBe(FORMULA_START_TEMP_DEFAULT);
		expect(normalizeFormulaStartTemp(Number.NaN)).toBe(FORMULA_START_TEMP_DEFAULT);
	});

	it('зажимает значение в диапазон 84…100 °C', () => {
		expect(normalizeFormulaStartTemp(83)).toBe(84);
		expect(normalizeFormulaStartTemp(101)).toBe(100);
		expect(normalizeFormulaStartTemp(95)).toBe(95);
	});

	it('для «авто 92+» использует фиксированные 92 °C', () => {
		expect(formulaStartTempForMode(2, 95)).toBe(FORMULA_START_TEMP_DEFAULT);
		expect(formulaStartTempForMode(1, 95)).toBe(95);
		expect(formulaStartTempForMode(0, 0)).toBe(FORMULA_START_TEMP_DEFAULT);
	});

	it('даёт разные подписи и пояснения для режимов', () => {
		expect(formulaModeLabel(0)).toBe('Выкл');
		expect(formulaModeLabel(1)).toBe('Формула');
		expect(formulaModeLabel(2)).toBe('Авто 92+');
		expect(formulaModeHint(2)).toContain('предекремента');
		expect(formulaModeHint(1)).toContain('99');
		expect(formulaModeHint(0)).toContain('выключено');
		expect(FORMULA_END_TEMP).toBe(99);
	});
});

describe('rectificationRules: события предекремента', () => {
	it('считает прирост счётчика', () => {
		expect(predecEventDelta(3, 4)).toBe(1);
		expect(predecEventDelta(3, 6)).toBe(3);
	});

	it('не считает событием первый опрос, сброс и отсутствие данных', () => {
		expect(predecEventDelta(undefined, 1)).toBe(0);
		expect(predecEventDelta(3, 3)).toBe(0);
		expect(predecEventDelta(4, 3)).toBe(0);
		expect(predecEventDelta(3, undefined)).toBe(0);
		expect(predecEventDelta(Number.NaN, 5)).toBe(0);
	});

	it('оценивает первый предекремент по диапазону 92…94 °C', () => {
		expect(predecVerdict(91.9)).toBe('early');
		expect(predecVerdict(92)).toBe('normal');
		expect(predecVerdict(93.4)).toBe('normal');
		expect(predecVerdict(94)).toBe('normal');
		expect(predecVerdict(94.1)).toBe('late');
		expect(predecVerdict(undefined)).toBeNull();
	});

	it('даёт пояснение к каждой оценке', () => {
		expect(predecVerdictHint('normal')).toContain('92–94');
		expect(predecVerdictHint('early')).toContain('завышена');
		expect(predecVerdictHint('late')).toContain('занижена');
	});
});
