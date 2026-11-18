/**
 * События предекремента для экрана телеметрии.
 *
 * `preempt_cnt` контроллер отдаёт счётчиком, опрос идёт раз в 2 с, поэтому
 * событие — это прирост между двумя опросами (см. `predecEventDelta`).
 * Модуль не импортирует Svelte: список событий — обычный массив, чтобы его
 * легко было проверить юнит-тестом.
 */

export type PredecEvent = {
	/** Номер события с начала этапа: 1 — первый зафиксированный прирост счётчика. */
	index: number;
	/** Сколько срабатываний прибавилось между опросами. */
	count: number;
	/** ТД1 в момент события, °C. */
	td1?: number;
	/** ТД2 в момент события, °C. */
	td2?: number;
	/** Декремент, присланный контроллером вместе с телеметрией. */
	decrement?: number;
	/** Время телеметрии на момент события. */
	at?: string;
};

/** Сколько последних событий держим в памяти. */
export const PREDEC_EVENTS_LIMIT = 5;

/**
 * Возвращает новый список с добавленным событием и обрезает его до `limit`
 * последних элементов. Исходный массив не меняется.
 */
export function appendPredecEvents(
	list: readonly PredecEvent[],
	event: PredecEvent,
	limit = PREDEC_EVENTS_LIMIT
): PredecEvent[] {
	const safeLimit = Number.isFinite(limit) ? Math.max(0, Math.floor(limit)) : PREDEC_EVENTS_LIMIT;
	if (safeLimit === 0) return [];
	return [...list, event].slice(-safeLimit);
}
