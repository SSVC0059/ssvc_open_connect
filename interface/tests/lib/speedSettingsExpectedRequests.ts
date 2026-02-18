import type { SsvcSettings } from '$lib/types/ssvc';

/**
 * Спека одного редактируемого поля вкладки «Параметры отбора»:
 * заголовок секции в UI и соответствующий REST-запрос PUT /rest/settings.
 */
export type SpeedSettingsFieldSpec = {
	/** Заголовок секции в интерфейсе (например "Параметры отбора голов") */
	sectionTitle: string;
	/** Имя поля в теле запроса: PUT /rest/settings с телом { [field]: value } */
	field: string;
	/** Краткое описание значения (для понимания, что отправляет поле) */
	valueDescription: string;
	/**
	 * Ожидаемое тело запроса для этого поля: значение, которое должно уходить в PUT /rest/settings.
	 * Настраивается здесь — тесты проверяют именно его. Пример: для heads — [12.0, 10].
	 */
	expectedRequestBody: [number, number];
	getValue: (settings: SsvcSettings) => [number, number];
};

/**
 * Реестр ожидаемых запросов для вкладки «Параметры отбора» (SpeedSettings).
 * Каждое редактируемое поле отправляет PUT /rest/settings с телом { [field]: value }.
 *
 * Чтобы проверить, что тело запроса включает, например, heads: [12.0, 10] —
 * задайте expectedRequestBody для heads в этом реестре (ниже). Тесты сверяются с ним.
 */
export const SPEED_SETTINGS_EXPECTED_REQUESTS = {
	heads: {
		sectionTitle: 'Параметры отбора голов',
		field: 'heads',
		valueDescription: '[время открытия клапана, период] — два числа, сек',
		expectedRequestBody: [12.0, 10] as [number, number],
		getValue: (s: SsvcSettings) => s.heads
	} satisfies SpeedSettingsFieldSpec,
	late_heads: {
		sectionTitle: 'Параметры отбора подголовников',
		field: 'late_heads',
		valueDescription: '[время открытия клапана, период] — два числа, сек',
		expectedRequestBody: [0.8, 14] as [number, number],
		getValue: (s: SsvcSettings) => s.late_heads
	} satisfies SpeedSettingsFieldSpec,
	hearts: {
		sectionTitle: 'Параметры отбора тела',
		field: 'hearts',
		valueDescription: '[время открытия клапана, период] — два числа, сек',
		expectedRequestBody: [1.2, 22] as [number, number],
		getValue: (s: SsvcSettings) => s.hearts
	} satisfies SpeedSettingsFieldSpec,
	tails: {
		sectionTitle: 'Параметры отбора хвостов',
		field: 'tails',
		valueDescription: '[время открытия клапана, период] — два числа, сек',
		expectedRequestBody: [0.3, 8] as [number, number],
		getValue: (s: SsvcSettings) => s.tails
	} satisfies SpeedSettingsFieldSpec
} as const;

export type SpeedSettingsFieldKey = keyof typeof SPEED_SETTINGS_EXPECTED_REQUESTS;
