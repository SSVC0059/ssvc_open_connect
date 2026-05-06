// ======================== Сообщения SSVC ======================== //

export type SsvcOpenConnectMessage = {
	telemetry: SsvcTelemetryMessage;
	waterInputTemp?: number;
	waterOutputTemp?: number;
	manufacturer?: string;
	model?: string;
	version?: string;
	request?: string;
	result?: string;
	hysteresis?: number;
	tp1_sap?: number;
	tp2_sap?: number;
	rectificationStart?: string;
	rectificationEnd?: string;
	settings?: SsvcSettings;
	status?: RectStatus;
};

export type SsvcOpenConnectInfo = {
	ssvc: ssvc_info;
	os: os_info;
};

export type ssvc_info = {
	version: string;
	api: number;
	mode: 'late_heads';
};

export type os_info = {
	version: string;
	is_support_api: boolean;
};

// ======================== Статус ректификации ======================== //

export type RectStatus = {
	stage: string;
	status: string;
	startTime: string;
	endTime: string;
	stages: Stages;
	/** Ошибка связи по UART (SSVC выключен или отключён) */
	uartConnectionError?: boolean;
};

// Этапы ректификации
type Stages = {
	[key: string]: string; // или другой тип данных
};

// ======================== Телеметрия SSVC ======================== //

export type SsvcTelemetryMessage = {
	type: string;
	pid?: number;
	start_time?: string;
	end_time?: string;
	tank_mmhg: number;
	tp1_target?: number;
	tp2_target?: number;
	countdown?: string;
	release?: string;
	time?: string;
	open?: number;
	period?: number;
	hysteresis?: number;
	valveOpen?: number;
	volumeSpeed?: number;
	v1?: number;
	v2?: number;
	v3?: number;
	stop: boolean;
	stops: number;
	common: commonType;
	volume?: valveFlowVolumeType;
	alc?: number;
	event?: string;
	info?: string;
	decrement?: number; // Декремент
};

export type commonType = {
	mmhg: string;
	tp1: number;
	tp2: number;
	relay: boolean;
	signal: boolean;
	overclockingOn: boolean;
	heatingOn: boolean;
	hysteresis?: number;
};

export type valveFlowVolumeType = {
	[key: string]: number; // или другой тип данных
};

// ======================== Настройки SSVC  ======================== //

/**
 * Настройки фракций для SSVC в формате [openTime, period].
 * openTime: время открытия клапана в мс.
 * period: период ШИМ в мс.
 */
export type SsvcTuple = [number, number];

export type SsvcSettings = {
	heads: SsvcTuple; // Параметры для Голов: [openTime, period]
	late_heads: SsvcTuple; // Параметры для подголовников: [openTime, period]
	hearts: SsvcTuple; // Параметры для Тела: [openTime, period]
	tails: SsvcTuple; // Параметры для Хвостов: [openTime, period]
	hyst: number; // Гистерезис
	decrement: number; // Декремент
	sound: number; // Звук (0 или 1)
	pressure: number; // Давление
	relay_inverted: number; // Инвертировано ли реле (0 или 1)
	relay_autostart: number; // Автозапуск реле (0 или 1)
	autoresume: number; // Автоматическое возобновление (0 или 1)
	auto_mode: number; // Автоматический режим (0 или 1)
	heads_timer: number; // Таймер для Heads (в секундах)
	late_heads_timer: number; // Время отбора подголовников, с. Актуально в firmware 2.3.*
	hearts_timer: number; // Таймер для Hearts (в секундах)
	tails_temp: number; // Температура хвостов (Tail)
	start_delay: number; // Задержка старта (в секундах)
	hearts_finish_temp: number; // Температура окончания Hearts
	parallel_v3: [number, number, number][]; // Параметры Parallel V3, массив массивов из трех значений
	parallel_v1: SsvcTuple; // Параметры Parallel V1
	parallel: SsvcTuple; // Параметры Parallel V3 - подголовники
	hearts_temp_shift: boolean; // Смещение температуры для Hearts
	hearts_pause: boolean; // Пауза для Hearts
	formula: boolean; // Формула
	formula_start_temp: number; // Начальная температура для формулы
	tank_mmhg: number;
	tp2_shift: number; // Смещение TP2
	tp_filter: boolean; // Температурный фильтр
	signal_tp1_control: number; // Контроль сигнала TP1 (0 или 1)
	signal_inverted: number; // Инвертирован сигнал (0 или 1)
	tp1_control_temp: number; // Контроль температуры TP1
	tp1_control_start: number; // Старт контроля TP1 (0 или 1)
	stab_limit_time: number; // Ограничение времени стабилизации (в секундах)
	stab_limit_finish: number; // Завершение стабилизации (0 или 1)
	backlight: string; // Подсветка ("off", "on" или другие возможные значения)
	valve_bw: [number, number, number]; // Подсветка ("off", "on" или другие возможные значения)
	release_speed: number;
	release_timer: number;
	heads_final: number;
};

// ======================== Настройки Telegram ======================== //

export type TelegramConfig = {
	token: string;
	chat_id: string;
};

// ==================== Настройки подсистем OpenConnect  ===================== //

export type SubsystemsState = {
	telegram_bot: boolean;
	thermal: boolean;
	i2c_bus: boolean;
	atm_sensor: boolean;
	settings:	boolean;
};

/** Ответ GET/PUT /rest/oc/hardware-config */
export type OcHardwareCapabilities = {
	pinoutUseGpio: boolean;
	relayOptionsAvailable: boolean;
};

export type OcHardwareConfigResponse = {
	schemaVersion: number;
	pressureSensorEnabled: boolean;
	bmp581I2cAddress: number;
	/** I²C probe at `bmp581I2cAddress` when `pressureSensorEnabled` (from GET/PUT). */
	bmp581ProbeOk?: boolean;
	userRelayPcfEnabled: boolean;
	/** Ordered I²C addresses (7-bit); index = chip order (first = alarm outputs). */
	relayPcf8574Addresses: number[];
	/** Present when firmware probes expanders on GET (PCF8574 builds). */
	relayPcf8574ProbeOk?: boolean[];
	relayHardwareOk?: boolean;
	pendingReboot: boolean;
	/** DS3231 real-time clock (I²C, optional). */
	rtcEnabled: boolean;
	/** 7-bit I²C address, default 0x68 (104). */
	ds3231I2cAddress: number;
	/** I²C probe at `ds3231I2cAddress` when `rtcEnabled` (from GET/PUT). */
	ds3231ProbeOk?: boolean;
	/** LCD1602 (HD44780 with PCF8574 backpack) over I²C, optional. */
	lcd1602Enabled?: boolean;
	/** 7-bit I²C address, typical 0x27 (39). */
	lcd1602I2cAddress?: number;
	/** I²C probe at `lcd1602I2cAddress` when `lcd1602Enabled` (from GET/PUT). */
	lcd1602ProbeOk?: boolean;
	/** Резерв (OLED и т.д.). */
	oledEnabled?: boolean;
	capabilities: OcHardwareCapabilities;
	needsReboot: boolean;
};

export type SendCommandResponse = {
	success: boolean;
	message: string;
};

// ==================== Профили ректификации  ===================== //

/** Правила для пользовательских линий PCF8574 (см. прошивку RelayRuleEngine). */
export type RelayCondition =
	| { type: 'sensor_alarm'; levels?: number[]; sensorNameContains?: string }
	| { type: 'hardware_fault'; code?: number; roleContains?: string }
	| { type: 'rectification'; stageEquals: string }
	| {
			type: 'ssvc_setting';
			key: string;
			boolEquals?: boolean;
			intEquals?: number;
			floatEquals?: number;
	  };

export type OpenConnectRelayRuleRow = {
	enabled?: boolean;
	targetBit: number;
	actionEnergize?: boolean;
	priority?: number;
	condition: RelayCondition;
};

export type OpenConnectRelayRules = {
	schemaVersion?: number;
	rules: OpenConnectRelayRuleRow[];
};

export type RelayDescriptor = {
	index: number;
	label: string;
	editable: boolean;
	reason?: string;
	targetBit: number;
};

export type FaultCodeDescriptor = {
	value: number;
	key: string;
	label: string;
};

export type RectificationStageDescriptor = {
	value: string;
	label: string;
};

export type SsvcFieldDescriptor = {
	key: string;
	label: string;
	valueType: 'bool' | 'int' | 'float';
};

export type RelayRuleMetadata = {
	supported: boolean;
	relays: RelayDescriptor[];
	/** Mirror of hardware: number of PCF8574 relay expanders. */
	relayChipCount?: number;
	/** Total user + system lines (chips × 8). */
	relayLineCount?: number;
	sensorNames: string[];
	hardwareFaultCodes: FaultCodeDescriptor[];
	hardwareRoles: string[];
	rectificationStages: RectificationStageDescriptor[];
	ssvcFields: SsvcFieldDescriptor[];
};

interface FractionSettings {
	percent: number; // % от АС
	enabled: boolean; // Включен ли отбор
	targetFlowMlh: number; // Желаемая скорость отбора (мл/ч)
	targetCycles?: number; // Целевое количество циклов переиспарения (только для голов)
	decrementPercent?: number;
}

export type Profile = {
	id: string;
	name: string;
	createdAt: string;
	isApplied?: boolean;

	// --- Параметры сырца ---
	/** Реальная мощность колонны */
	powerKw: number;
	/** Объем спирта-сырца в кубе (литры, точность 0.1) */
	volumeL: number;
	/** Спиртуозность сырца по показаниям ареометра (%) */
	strengthVol: number;

	stabilizationMin: number;

	heads: FractionSettings;
	late_heads: FractionSettings;
	hearts: FractionSettings;
	tails: FractionSettings;

	/** Настройки SSVC **/
	ssvcSettings: SsvcSettings;

	/** Пользовательская логика реле расширителя (только PCF8574 / PINOUT_USE_GPIO=0). */
	openConnectRelayRules?: OpenConnectRelayRules;

	analytics: Analytics;
};

export type Analytics = {
	totalAS: number;
	boilingTemp: number;
	oneCycleTime: number;
	flows: {
		heads: number;
		heads_release: number;
		heads_final: number;
		late_heads: number;
		hearts: number;
		hearts_avg?: number;
		hearts_final?: number;
		tails: number;
	};
	fractions: {
		releaseMl: number;
		headsMl: number;
		lateHeadsMl: number;
		heartsMl: number;
		tailsMl: number;
	};
	timers: {
		heads: number;
		late_heads: number;
		hearts: number;
		tails: number;
		total_process: number;
	};
	refluxRatio: {
		heads: number;
		late_heads: number;
		hearts: number;
		tails: number;
	};
	phlegmatic: {
		heads: number;
		late_heads: number;
		hearts: number;
		tails: number;
	};
	residueMl: number;
	residualFortress: number;
};
export type Profiles = Profile[];

// === Типы для работы с калькулятором ректификации (редактор профилей)  === //

export type FractionCalcResult = {
	/** Рассчитанный объем фракции в миллилитрах */
	ml: number;
	/** Рассчитанная длительность этапа в минутах */
	durationMin: number;
};

// ==================== Файлы  ===================== //
export type FSItemType = 'directory' | 'file';

export interface BaseFSItem {
	name: string;
	type: FSItemType;
}

export interface FileItem extends BaseFSItem {
	type: 'file';
	size: number;
}

export interface DirectoryItem extends BaseFSItem {
	type: 'directory';
	children: FSItem[]; // Рекурсия: массив может содержать файлы или папки
}

// Итоговый тип элемента
export type FSItem = FileItem | DirectoryItem;

// Тип для корня (массив элементов)
export type FileSystemTree = FSItem[];
