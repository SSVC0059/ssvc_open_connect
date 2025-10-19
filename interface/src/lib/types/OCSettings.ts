export type ZoneName = 'inlet_water' | 'outlet_water' | 'ast' | 'unknown' | string; // Добавь все возможные зоны

export type Zones = {
	[K in ZoneName]?: string[]; // Массив адресов датчиков (может быть пустым или отсутствовать)
};

export type SensorData = {
	address: string; // Формат адреса может быть уточнен, если есть специфические требования
	temp: number;
};

export type ZonesMap = {
	[zoneName: string]: SensorData[]
}
export type TemperatureResponse = {
	zones: ZonesMap;
};
