export type WifiStatus = {
	status: number;
	local_ip: string;
	mac_address: string;
	rssi: number;
	ssid: string;
	bssid: string;
	channel: number;
	subnet_mask: string;
	gateway_ip: string;
	dns_ip_1: string;
	dns_ip_2?: string;
};

export type WifiSettings = {
	hostname: string;
	priority_RSSI: boolean;
	wifi_networks: KnownNetworkItem[];
};

export type KnownNetworkItem = {
	ssid: string;
	password: string;
	static_ip_config: boolean;
	local_ip?: string;
	subnet_mask?: string;
	gateway_ip?: string;
	dns_ip_1?: string;
	dns_ip_2?: string;
};

export type NetworkItem = {
	rssi: number;
	ssid: string;
	bssid: string;
	channel: number;
	encryption_type: number;
};

export type ApStatus = {
	status: number;
	ip_address: string;
	mac_address: string;
	station_num: number;
};

export type ApSettings = {
	provision_mode: number;
	ssid: string;
	password: string;
	channel: number;
	ssid_hidden: boolean;
	max_clients: number;
	local_ip: string;
	gateway_ip: string;
	subnet_mask: string;
};

export type LightState = {
	led_on: boolean;
};

export type BrokerSettings = {
	mqtt_path: string;
	name: string;
	unique_id: string;
};

export type NTPStatus = {
	status: number;
	utc_time: string;
	local_time: string;
	server: string;
	uptime: number;
};

export type NTPSettings = {
	enabled: boolean;
	server: string;
	tz_label: string;
	tz_format: string;
};

export type Analytics = {
	max_alloc_heap: number;
	psram_size: number;
	free_psram: number;
	free_heap: number;
	total_heap: number;
	min_free_heap: number;
	core_temp: number;
	fs_total: number;
	fs_used: number;
	uptime: number;
};

export type RSSI = {
	rssi: number;
	ssid: string;
};

export type Battery = {
	soc: number;
	charging: boolean;
};

export type DownloadOTA = {
	status: string;
	progress: number;
	error: string;
};

export type StaticSystemInformation = {
	esp_platform: string;
	firmware_version: string;
	cpu_freq_mhz: number;
	cpu_type: string;
	cpu_rev: number;
	cpu_cores: number;
	sketch_size: number;
	free_sketch_space: number;
	sdk_version: string;
	arduino_version: string;
	flash_chip_size: number;
	flash_chip_speed: number;
	cpu_reset_reason: string;
};

export type SystemInformation = Analytics & StaticSystemInformation;

export type MQTTStatus = {
	enabled: boolean;
	connected: boolean;
	client_id: string;
	last_error: string;
};

export type MQTTSettings = {
	enabled: boolean;
	uri: string;
	username: string;
	password: string;
	client_id: string;
	keep_alive: number;
	clean_session: boolean;
};

export type SsvcOpenConnectMessage = {
	type: string;
	tp1_target?: number,
	manufacturer?: string,
	model?: string,
	version?: string,
	request?: string,
	result?: string,
	countdown?: string;
	time?: string;
	open?: number;
	period?: number;
	hysteresis?: number;
	tank_mmhg?: number;
	tp1_sap?: number;
	tp2_sap?: number;
	v1?: number;
	v2?: number;
	v3?: number;
	alc?: number;
	stop?: number;
	stops?: number;
	event?: string;
	common?: commonType;
	settings?: SsvcSettings,
};

export type commonType = {
	mmhg: string,
	tp1: number,
	tp2: number,
	relay:boolean,
	signal: boolean
};

export type SsvcSettings =   {
	heads: [number, number];                // Параметры для Heads: давление и период
	hearts: [number, number];               // Параметры для Hearts: давление и период
	hyst: number;                           // Гистерезис
	decrement: number;                      // Декремент
	tails: [number, number];                // Параметры для Tails: давление и период
	sound: number;                          // Звук (0 или 1)
	pressure: number;                       // Давление
	relay_inverted: number;                 // Инвертировано ли реле (0 или 1)
	relay_autostart: number;                // Автозапуск реле (0 или 1)
	autoresume: number;                     // Автоматическое возобновление (0 или 1)
	auto_mode: number;                      // Автоматический режим (0 или 1)
	head_timer: number;                     // Таймер для Heads (в секундах)
	body_timer: number;                     // Таймер для Hearts (в секундах)
	tail_temp: number;                      // Температура хвостов (Tail)
	start_delay: number;                    // Задержка старта (в секундах)
	hearts_finish_temp: number;             // Температура окончания Hearts
	parallel_v3: [number, number, number][]; // Параметры Parallel V3, массив массивов из трех значений
	parallel_v1: [number, number];          // Параметры Parallel V1
	hearts_temp_shift: number;              // Смещение температуры для Hearts
	hearts_pause: number;                   // Пауза для Hearts
	formula: number;                        // Формула
	formula_start_temp: number;             // Начальная температура для формулы
	tank_mmhg: number;                      // Давление в баке (mmHg)
	tp2_shift: number;                      // Смещение TP2
	tp_filter: number;                      // Температурный фильтр
	signal_tp1_control: number;             // Контроль сигнала TP1 (0 или 1)
	signal_inverted: number;                // Инвертирован сигнал (0 или 1)
	tp1_control_temp: number;               // Контроль температуры TP1
	tp1_control_start: number;              // Старт контроля TP1 (0 или 1)
	stab_limit_time: number;                // Ограничение времени стабилизации (в секундах)
	stab_limit_finish: number;              // Завершение стабилизации (0 или 1)
	backlight: string;                      // Подсветка ("off", "on" или другие возможные значения)
	valve_bandwidth: [number, number, number];                      // Подсветка ("off", "on" или другие возможные значения)
}

// export const commandState=  writable<CommandState>
export type CommandState = {
	isWaiting: boolean,
	message: string,
	command: string
}

