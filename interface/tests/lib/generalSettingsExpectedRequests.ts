import type { SsvcSettings } from '$lib/types/ssvc';

/**
 * Тип значения тела запроса для GeneralSettings: число или boolean.
 */
export type GeneralSettingsRequestBody = number | boolean;

/**
 * Спека одного редактируемого поля вкладки «Общие» (GeneralSettings):
 * подпись в UI и соответствующий REST-запрос PUT /rest/settings.
 */
export type GeneralSettingsFieldSpec = {
	/** Текст подписи поля в интерфейсе (input-label или рядом) */
	label: string;
	/** Имя поля в теле запроса: PUT /rest/settings с телом { [field]: value } */
	field: string;
	/** Краткое описание значения */
	valueDescription: string;
	/** Ожидаемое тело запроса для этого поля. Настраивается здесь — тесты проверяют именно его. */
	expectedRequestBody: GeneralSettingsRequestBody;
	getValue: (settings: SsvcSettings) => GeneralSettingsRequestBody;
	/** 'modal' — открыть модалку и нажать «Сохранить»; 'checkbox' — клик по чекбоксу */
	controlType: 'modal' | 'time_modal' | 'checkbox';
	/** Секция отображается только если условие выполняется (например, release_timer) */
	visibleWhen?: (settings: SsvcSettings) => boolean;
};

/**
 * Реестр ожидаемых запросов для вкладки «Общие» (GeneralSettings).
 * Чтобы проверить тело запроса для поля — задайте expectedRequestBody в этом реестре.
 */
export const GENERAL_SETTINGS_EXPECTED_REQUESTS = {
	hyst: {
		label: 'Гистерезис при отборе тела',
		field: 'hyst',
		valueDescription: 'число, °С',
		expectedRequestBody: 0.5,
		getValue: (s: SsvcSettings) => s.hyst,
		controlType: 'modal'
	} satisfies GeneralSettingsFieldSpec,
	decrement: {
		label: 'Декремент при отборе тела',
		field: 'decrement',
		valueDescription: 'число, %',
		expectedRequestBody: 2,
		getValue: (s: SsvcSettings) => s.decrement,
		controlType: 'modal'
	} satisfies GeneralSettingsFieldSpec,
	heads_timer: {
		label: 'Время отбора голов',
		field: 'heads_timer',
		valueDescription: 'число, секунды',
		expectedRequestBody: 300,
		getValue: (s: SsvcSettings) => s.heads_timer,
		controlType: 'time_modal'
	} satisfies GeneralSettingsFieldSpec,
	hearts_timer: {
		label: 'Таймер фиксации температуры отбора тела',
		field: 'hearts_timer',
		valueDescription: 'число, сек',
		expectedRequestBody: 15,
		getValue: (s: SsvcSettings) => s.hearts_timer,
		controlType: 'modal'
	} satisfies GeneralSettingsFieldSpec,
	tank_mmhg: {
		label: 'Поправка давления в кубе',
		field: 'tank_mmhg',
		valueDescription: 'число, мм рт. ст.',
		expectedRequestBody: 10,
		getValue: (s: SsvcSettings) => s.tank_mmhg,
		controlType: 'modal'
	} satisfies GeneralSettingsFieldSpec,
	tails_temp: {
		label: 'Температура ТД2 завершения отбора тела',
		field: 'tails_temp',
		valueDescription: 'число, °С',
		expectedRequestBody: 85,
		getValue: (s: SsvcSettings) => s.tails_temp,
		controlType: 'modal'
	} satisfies GeneralSettingsFieldSpec,
	formula: {
		label: 'Формула',
		field: 'formula',
		valueDescription: 'boolean',
		expectedRequestBody: true,
		getValue: (s: SsvcSettings) => s.formula,
		controlType: 'checkbox'
	} satisfies GeneralSettingsFieldSpec,
	formula_start_temp: {
		label: 'Температура начала формулы',
		field: 'formula_start_temp',
		valueDescription: 'число, °С',
		expectedRequestBody: 90,
		getValue: (s: SsvcSettings) => s.formula_start_temp,
		controlType: 'modal'
	} satisfies GeneralSettingsFieldSpec,
	release_timer: {
		label: 'Время сброса',
		field: 'release_timer',
		valueDescription: 'число, сек',
		expectedRequestBody: 60,
		getValue: (s: SsvcSettings) => s.release_timer,
		controlType: 'modal',
		visibleWhen: (s: SsvcSettings) => (s.release_timer ?? 0) >= 0
	} satisfies GeneralSettingsFieldSpec,
	release_speed: {
		label: 'Скорость сброса',
		field: 'release_speed',
		valueDescription: 'число, сек',
		expectedRequestBody: 5,
		getValue: (s: SsvcSettings) => s.release_speed,
		controlType: 'modal',
		visibleWhen: (s: SsvcSettings) => (s.release_timer ?? 0) >= 0
	} satisfies GeneralSettingsFieldSpec,
	heads_final: {
		label: 'Скорость отбора к окончанию голов',
		field: 'heads_final',
		valueDescription: 'число, сек',
		expectedRequestBody: 2.5,
		getValue: (s: SsvcSettings) => s.heads_final,
		controlType: 'modal',
		visibleWhen: (s: SsvcSettings) => (s.release_timer ?? 0) >= 0
	} satisfies GeneralSettingsFieldSpec
} as const;

export type GeneralSettingsFieldKey = keyof typeof GENERAL_SETTINGS_EXPECTED_REQUESTS;
