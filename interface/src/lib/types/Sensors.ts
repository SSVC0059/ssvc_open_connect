// Sensors.ts (Корректное определение)

export type ZoneName = 'inlet_water' | 'outlet_water' | 'ast' | 'unknown' | string;

export type Zones = {
	[K in ZoneName]?: string[];
};

export type SensorMap = {
	[address: string]: number;
}

export type SensorReading = {
	address: string;
	temp: number;
}

export type TemperatureResponse = {
	[zoneName: string]: SensorMap // Имя зоны (string) -> SensorMap
}


export type AlarmThresholdsState =  {
	thresholds: Record<string, ThresholdSettings>;
}

export type ThresholdSettings = {
	enabled: boolean;
	min: number;
	dangerous: number;
	critical: number;
}

export type CriticalStateResult = {
	isCritical: boolean;
	reason: 'Individual Sensor' | 'Fallback Zone' | 'None';
	triggerValue?: number; // Температура, вызвавшая срабатывание
	triggerThreshold?: number; // Превышенный порог
	triggerAddress?: string; // Адрес датчика (если сработал индивидуальный порог)
}