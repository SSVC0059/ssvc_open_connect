import type { SsvcSettings } from '$lib/types/ssvc';

/**
 * Спека редактируемого поля вкладки «Клапаны» (ValveBandwidth):
 * заголовок секции в UI и соответствующий REST-запрос PUT /rest/settings.
 */
export type ValveBandwidthFieldSpec = {
	/** Заголовок секции в интерфейсе */
	sectionTitle: string;
	/** Имя поля в теле запроса: PUT /rest/settings с телом { [field]: value } */
	field: string;
	/** Краткое описание значения */
	valueDescription: string;
	/**
	 * Ожидаемое тело запроса: [головы, тело, подголовники|хвосты] мл/ч.
	 * Настраивается здесь — тесты проверяют именно его.
	 */
	expectedRequestBody: [number, number, number];
	getValue: (settings: SsvcSettings) => [number, number, number];
};

/**
 * Реестр ожидаемых запросов для вкладки «Клапаны» (ValveBandwidth).
 * Чтобы проверить тело запроса для valve_bw — задайте expectedRequestBody в этом реестре.
 */
export const VALVE_BANDWIDTH_EXPECTED_REQUESTS = {
	valve_bw: {
		sectionTitle: 'Настройки пропускной способности клапанов',
		field: 'valve_bw',
		valueDescription: '[головы, тело, подголовники|хвосты] мл/ч',
		expectedRequestBody: [100, 200, 150] as [number, number, number],
		getValue: (s: SsvcSettings) => s.valve_bw
	} satisfies ValveBandwidthFieldSpec
} as const;

export type ValveBandwidthFieldKey = keyof typeof VALVE_BANDWIDTH_EXPECTED_REQUESTS;
