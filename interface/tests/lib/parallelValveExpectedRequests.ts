import type { SsvcSettings } from '$lib/types/ssvc';

/** Одна точка параллельного отбора: [температура °С, время открытия клапана, период] */
export type ParallelV3Point = [number, number, number];

/**
 * Спека редактируемого поля вкладки «Параллельный отбор» (ParallelValve).
 */
export type ParallelValveFieldSpec = {
	/** Заголовок секции в интерфейсе */
	sectionTitle: string;
	/** Если несколько секций с одним заголовком — индекс (0-based) */
	sectionIndex?: number;
	/** Имя поля в теле запроса: PUT /rest/settings с телом { [field]: value } */
	field: string;
	/** Краткое описание значения */
	valueDescription: string;
	/**
	 * Ожидаемое тело запроса. Настраивается здесь — тесты проверяют именно его.
	 */
	expectedRequestBody: [number, number] | ParallelV3Point[];
	getValue: (settings: SsvcSettings) => [number, number] | ParallelV3Point[];
	/** 'modal' — EditNumbersModal (2 числа); 'parallel_v3' — EditParallelV3 (массив точек) */
	controlType: 'modal' | 'parallel_v3';
	/** Секция отображается только если условие выполняется */
	visibleWhen?: (settings: SsvcSettings) => boolean;
};

/**
 * Реестр ожидаемых запросов для вкладки «Параллельный отбор» (ParallelValve).
 * Чтобы проверить тело запроса — задайте expectedRequestBody в этом реестре.
 */
export const PARALLEL_VALVE_EXPECTED_REQUESTS = {
	parallel_v1: {
		sectionTitle: 'Скорость параллельного отбора клапаном №1',
		field: 'parallel_v1',
		valueDescription: '[время открытия клапана, период] — два числа, сек',
		expectedRequestBody: [1.5, 20] as [number, number],
		getValue: (s: SsvcSettings) => s.parallel_v1,
		controlType: 'modal',
		visibleWhen: (s: SsvcSettings) => !!s.parallel_v1
	} satisfies ParallelValveFieldSpec,
	parallel_v3: {
		sectionTitle: 'Скорость параллельного отбора клапаном №3',
		sectionIndex: 0,
		field: 'parallel_v3',
		valueDescription: 'массив точек [температура °С, время клапана, период]',
		expectedRequestBody: [[84, 1.2, 10]] as ParallelV3Point[],
		getValue: (s: SsvcSettings) => s.parallel_v3,
		controlType: 'parallel_v3',
		visibleWhen: (s: SsvcSettings) => !!s.parallel_v3
	} satisfies ParallelValveFieldSpec,
	parallel: {
		sectionTitle: 'Скорость параллельного отбора клапаном №3',
		sectionIndex: 1,
		field: 'parallel',
		valueDescription: '[время открытия клапана, период] — два числа, сек',
		expectedRequestBody: [0.8, 15] as [number, number],
		getValue: (s: SsvcSettings) => s.parallel,
		controlType: 'modal',
		visibleWhen: (s: SsvcSettings) => !!s.parallel
	} satisfies ParallelValveFieldSpec
} as const;

export type ParallelValveFieldKey = keyof typeof PARALLEL_VALVE_EXPECTED_REQUESTS;
