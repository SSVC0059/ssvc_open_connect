/**
 * Правила предекремента и формулы, общие для настроек, мастера запуска,
 * редактора профилей, калькулятора и экрана телеметрии.
 *
 * Источник правил — руководство изготовителя «Предекремент и Формула авто 92+»:
 * минимальный гистерезис для 0,07 °C — 0,12 °C, для 0,13 °C — 0,18 °C;
 * рекомендуемый диапазон первого предекремента — 92…94 °C по ТД2.
 * Допустимые значения `predec`/`formula` — из спецификации UART API SSVC0059_V2.
 *
 * Модуль не импортирует Svelte и API: только чистые функции, чтобы одно и то же
 * правило не расходилось между экранами и проверялось юнит-тестами.
 */

/** Значения `predec` в UART API: 0 — выключен, 7 и 13 — сотые доли °C. */
export const PREDEC_VALUES = [0, 7, 13] as const;

/** Режимы `formula`: 0 — выкл, 1 — обычная, 2 — «авто 92+». */
export const FORMULA_MODES = [0, 1, 2] as const;

export const FORMULA_START_TEMP_DEFAULT = 92;
export const FORMULA_START_TEMP_MIN = 84;
export const FORMULA_START_TEMP_MAX = 100;
/** Конец линии формулы: на 99 °C скорость стремится к нулю. */
export const FORMULA_END_TEMP = 99;

/** Рекомендуемый диапазон ТД2 для первого предекремента. */
export const PREDEC_OPTIMAL_MIN_TD2 = 92;
export const PREDEC_OPTIMAL_MAX_TD2 = 94;

const PREDEC_MIN_HYSTERESIS: Readonly<Record<number, number>> = {
	7: 0.12,
	13: 0.18
};

/** Погрешность сравнения температур: 0,12 должно считаться достаточным. */
const EPSILON = 1e-9;

const round2 = (value: number): number => Math.round((value + Number.EPSILON) * 100) / 100;

/** Минимальный гистерезис для выбранного предекремента; `null` — предекремент выключен. */
export function minHysteresisFor(predec: number | null | undefined): number | null {
	if (predec === 7 || predec === 13) return PREDEC_MIN_HYSTERESIS[predec];
	return null;
}

/**
 * Состояние пары «предекремент + гистерезис».
 *
 * `unknown` — гистерезис ещё не прочитан; предупреждать не о чем.
 * `hysteresis-too-small` — контроллер автоматически отключит предекремент.
 */
export type PredecState = 'off' | 'ok' | 'unknown' | 'hysteresis-too-small';

export function predecState(
	predec: number | null | undefined,
	hyst: number | null | undefined
): PredecState {
	if (predec !== 7 && predec !== 13) return 'off';
	if (typeof hyst !== 'number' || !Number.isFinite(hyst)) return 'unknown';
	const min = PREDEC_MIN_HYSTERESIS[predec];
	return hyst + EPSILON >= min ? 'ok' : 'hysteresis-too-small';
}

/** `true`, только если предекремент включён и гистерезиса достаточно. */
export function predecWillWork(
	predec: number | null | undefined,
	hyst: number | null | undefined
): boolean {
	return predecState(predec, hyst) === 'ok';
}

/** Подпись значения `predec` для селекта. */
export function predecLabel(predec: number): string {
	if (predec === 7) return '0,07 °C';
	if (predec === 13) return '0,13 °C';
	return 'Выкл';
}

export type ProtectionThresholds = {
	/** Температура остановки отбора: Т отбора + гистерезис. */
	stopAt: number;
	/** Температура предекремента: стоп − predec; `null`, если он не сработает. */
	predecAt: number | null;
};

/**
 * Пороги защиты по текущей Т отбора. `null` — нет данных, чтобы их посчитать.
 * Если гистерезиса недостаточно, `predecAt` равен `null`: показывать порог,
 * который контроллер всё равно отключит, было бы обманом.
 */
export function protectionThresholds(
	tp1Target: number | null | undefined,
	hyst: number | null | undefined,
	predec: number | null | undefined
): ProtectionThresholds | null {
	if (typeof tp1Target !== 'number' || !Number.isFinite(tp1Target)) return null;
	if (typeof hyst !== 'number' || !Number.isFinite(hyst)) return null;

	const stopAt = round2(tp1Target + hyst);
	const state = predecState(predec, hyst);
	const predecAt =
		state === 'ok' && (predec === 7 || predec === 13)
			? round2(stopAt - predec / 100)
			: null;

	return { stopAt, predecAt };
}

/** Нормализует температуру начала формулы: 0/пусто → 92, кламп 84…100. */
export function normalizeFormulaStartTemp(value: number | null | undefined): number {
	if (typeof value !== 'number' || !Number.isFinite(value) || value <= 0) {
		return FORMULA_START_TEMP_DEFAULT;
	}
	return Math.min(Math.max(value, FORMULA_START_TEMP_MIN), FORMULA_START_TEMP_MAX);
}

/**
 * Начало формулы для конкретного режима.
 *
 * Для «авто 92+» руководство называет фиксированный порог 92 °C, а UART-документ
 * говорит, что `formula_start_temp` актуальна при `formula=1`. Поэтому в режиме 2
 * используем 92 °C, пока поведение не проверено на живом контроллере.
 */
export function formulaStartTempForMode(
	mode: number,
	value: number | null | undefined
): number {
	if (mode === 2) return FORMULA_START_TEMP_DEFAULT;
	return normalizeFormulaStartTemp(value);
}

export function formulaModeLabel(mode: number): string {
	if (mode === 2) return 'Авто 92+';
	if (mode === 1) return 'Формула';
	return 'Выкл';
}

export function formulaModeHint(mode: number): string {
	if (mode === 2) {
		return 'Полная скорость до первого предекремента, затем 92–99 °C. Нужен предекремент.';
	}
	if (mode === 1) {
		return 'Снижение с начала формулы до 99 °C.';
	}
	return 'Снижение по ТД2 выключено.';
}

/**
 * Прирост счётчика `preempt_cnt` между опросами. Сброс счётчика (новый этап)
 * и первый опрос события не дают.
 */
export function predecEventDelta(
	prev: number | null | undefined,
	next: number | null | undefined
): number {
	if (typeof prev !== 'number' || typeof next !== 'number') return 0;
	if (!Number.isFinite(prev) || !Number.isFinite(next)) return 0;
	const delta = next - prev;
	return delta > 0 ? delta : 0;
}

export type PredecVerdict = 'early' | 'normal' | 'late';

/** Оценка первого предекремента по ТД2 в момент события. */
export function predecVerdict(td2: number | null | undefined): PredecVerdict | null {
	if (typeof td2 !== 'number' || !Number.isFinite(td2)) return null;
	if (td2 < PREDEC_OPTIMAL_MIN_TD2) return 'early';
	if (td2 > PREDEC_OPTIMAL_MAX_TD2) return 'late';
	return 'normal';
}

export function predecVerdictHint(verdict: PredecVerdict): string {
	if (verdict === 'early') {
		return 'Первый предекремент рано: вероятно, завышена скорость отбора или ТД1 установлен низко.';
	}
	if (verdict === 'late') {
		return 'Первый предекремент поздно: вероятно, занижена скорость отбора или ТД1 установлен высоко.';
	}
	return 'Первый предекремент в рекомендуемом диапазоне 92–94 °C.';
}
