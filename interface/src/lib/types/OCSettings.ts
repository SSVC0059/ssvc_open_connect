export type ZoneName = 'inlet_water' | 'outlet_water' | 'ast' | 'unknown' | string; // Добавь все возможные зоны

export type Zones = {
	[K in ZoneName]?: string[]; // Массив адресов датчиков (может быть пустым или отсутствовать)
};

export type SensorData = {
	address: string; // Формат адреса может быть уточнен, если есть специфические требования
	zone: ZoneName;
	temperature: number;
	warningThreshold: number;
	criticalThreshold: number;
};

export type SensorsData = {
	sensors: SensorData[];
	count: number;
	timestamp: number;
};
