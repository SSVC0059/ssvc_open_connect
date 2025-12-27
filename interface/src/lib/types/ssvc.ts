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

export type SsvcSettings = {
	heads: [number, number]; // Параметры для Голов: давление и период
	late_heads: [number, number]; // Параметры для подголовников: давление и период
	hearts: [number, number]; // Параметры для Тела: давление и период
	tails: [number, number]; // Параметры для Хвостов: давление и период
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
	parallel_v1: [number, number]; // Параметры Parallel V1
	parallel: [number, number]; // Параметры Parallel V3 - подголовники
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
};

export type SendCommandResponse = {
	success: boolean;
	message: string;
};

// ==================== Профили ректификации  ===================== //

export type Profile = {
	id: string;
	name: string;
	createdAt: string;
	isApplied?: boolean;

	// --- Параметры сырца ---
	/** Объем спирта-сырца в кубе (литры, точность 0.1) */
	rawVolumeLiters: number;
	/** Спиртуозность сырца по показаниям ареометра (%) */
	rawStrength: number;
	/** Температура сырца при замере спиртуозности (°C) */
	rawTemperature: number;
	/** Желаемая крепость продукта на выходе (например, 96.6) */
	targetStrength: number;

	/** Объем узла отбора, мл */
	collectionNodeVolumeMl: number;

	// --- Целевые объемы фракций (в % от абсолютного спирта) ---
	/** Суммарный объем головных фракций (%) */
	headsTotalPercent: number;
	/** Объем подголовников (оборотного спирта) (%) */
	lateHeadsPercent: number;
	/** Объем основного тела (%) */
	heartsPercent: number;
	/** Объем хвостовых фракций (%). Тело рассчитывается как остаток. */
	tailsPercent: number;

	/** Настройки SSVC **/
	ssvcSettings: SsvcSettings;
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