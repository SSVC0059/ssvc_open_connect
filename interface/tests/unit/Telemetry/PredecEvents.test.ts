/**
 * Тесты карточки событий предекремента и чистого помощника appendPredecEvents.
 * Вердикты оцениваются модулем rectificationRules, тексты берём из него же,
 * чтобы тесты не дублировали формулировки.
 */
import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/svelte';
import PredecEvents from '$lib/components/Telemetry/PredecEvents.svelte';
import {
	appendPredecEvents,
	PREDEC_EVENTS_LIMIT,
	type PredecEvent
} from '$lib/components/Telemetry/predecEvents';
import { predecVerdictHint } from '$lib/actions/rectificationRules';

function makeEvent(patch: Partial<PredecEvent> = {}): PredecEvent {
	return {
		index: 1,
		count: 1,
		td1: 78.52,
		td2: 93.1,
		decrement: 13,
		at: '01:12:33',
		...patch
	};
}

describe('PredecEvents: оценка события', () => {
	it('показывает событие в норме с ТД1/ТД2 и подсказкой', () => {
		render(PredecEvents, { props: { events: [makeEvent()] } });

		expect(
			screen.getByText('Предекремент №1: ТД1 78,52 °C, ТД2 93,10 °C')
		).toBeInTheDocument();
		expect(screen.getByText('в норме 92–94 °C')).toBeInTheDocument();
		expect(screen.getByText(predecVerdictHint('normal'))).toBeInTheDocument();
	});

	it('помечает ранний предекремент и объясняет причину', () => {
		render(PredecEvents, { props: { events: [makeEvent({ td2: 91.4 })] } });

		expect(screen.getByText('рано')).toBeInTheDocument();
		expect(screen.getByText(predecVerdictHint('early'))).toBeInTheDocument();
	});

	it('помечает поздний предекремент и объясняет причину', () => {
		render(PredecEvents, { props: { events: [makeEvent({ index: 2, td2: 94.6 })] } });

		expect(screen.getByText('Предекремент №2: ТД1 78,52 °C, ТД2 94,60 °C')).toBeInTheDocument();
		expect(screen.getByText('поздно')).toBeInTheDocument();
		expect(screen.getByText(predecVerdictHint('late'))).toBeInTheDocument();
	});

	it('без ТД2 не выдумывает оценку', () => {
		render(PredecEvents, { props: { events: [makeEvent({ td2: undefined })] } });

		expect(
			screen.getByText('Предекремент №1: ТД1 78,52 °C, ТД2 — °C')
		).toBeInTheDocument();
		expect(screen.getByText('нет данных')).toBeInTheDocument();
		expect(screen.queryByText(predecVerdictHint('normal'))).not.toBeInTheDocument();
	});
});

describe('PredecEvents: несколько срабатываний за интервал', () => {
	it('показывает, сколько срабатываний прибавилось между опросами', () => {
		render(PredecEvents, { props: { events: [makeEvent({ count: 3 })] } });

		expect(screen.getByText('3 срабатываний за интервал')).toBeInTheDocument();
	});

	it('для одного срабатывания не показывает счётчик интервала', () => {
		render(PredecEvents, { props: { events: [makeEvent({ count: 1 })] } });

		expect(screen.queryByText(/срабатываний за интервал/)).not.toBeInTheDocument();
	});
});

describe('PredecEvents: пустое состояние', () => {
	it('без событий пишет «Пока не было»', () => {
		render(PredecEvents, { props: { events: [] } });

		expect(screen.getByText('Пока не было')).toBeInTheDocument();
		expect(screen.queryByText(/Предекремент №/)).not.toBeInTheDocument();
	});
});

describe('appendPredecEvents', () => {
	it('добавляет событие в конец, не меняя исходный массив', () => {
		const first = makeEvent({ index: 1 });
		const second = makeEvent({ index: 2, count: 2 });
		const source: PredecEvent[] = [first];

		const result = appendPredecEvents(source, second);

		expect(result).toEqual([first, second]);
		expect(source).toEqual([first]);
		expect(result).not.toBe(source);
	});

	it('по умолчанию хранит последние пять событий', () => {
		let list: PredecEvent[] = [];
		for (let index = 1; index <= 7; index++) {
			list = appendPredecEvents(list, makeEvent({ index }));
		}

		expect(list).toHaveLength(PREDEC_EVENTS_LIMIT);
		expect(list.map((event) => event.index)).toEqual([3, 4, 5, 6, 7]);
	});

	it('уважает явный лимит и порядок', () => {
		const list = [makeEvent({ index: 1 }), makeEvent({ index: 2 })];
		const result = appendPredecEvents(list, makeEvent({ index: 3 }), 2);

		expect(result.map((event) => event.index)).toEqual([2, 3]);
	});

	it('с нулевым лимитом отдаёт пустой список', () => {
		expect(appendPredecEvents([makeEvent()], makeEvent({ index: 2 }), 0)).toEqual([]);
	});
});
