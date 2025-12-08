// WizardSettings/logic.ts
import { fetchSettings } from '$lib/api/ssvcApi';
import type { SsvcSettings } from '$lib/types/ssvc';

export async function loadSettings(): Promise<SsvcSettings | null> {
	return await fetchSettings();
}
export function secondsToTimeString(sec: number): string {
	const hours = Math.floor(sec / 3600)
		.toString()
		.padStart(2, '0');
	const minutes = Math.floor((sec % 3600) / 60)
		.toString()
		.padStart(2, '0');
	return `${hours}:${minutes}`;
}

export function handleInputChange(settings: SsvcSettings, event: Event) {
	const target = event.target as HTMLInputElement;
	const correctedTime = handleTimeChange(target.value);
	settings.heads_timer = timeStringToSeconds(correctedTime);
	target.value = correctedTime;
}

function timeStringToSeconds(timeString: string): number {
	const [hoursStr, minutesStr] = timeString.split(':');
	const hours = parseInt(hoursStr, 10);
	const minutes = parseInt(minutesStr, 10);

	if (isNaN(hours) || isNaN(minutes)) {
		return 0;
	}

	return Math.min(hours * 3600 + minutes * 60, 86400);
}

function handleTimeChange(value: string): string {
	const seconds = timeStringToSeconds(value);
	const roundedSeconds = Math.round(seconds / 300) * 300;
	return secondsToTimeString(roundedSeconds);
}

/**
 * Рассчитывает скорость отбора в мл/час
 * @param openTime Время открытого клапана в секундах
 * @param period Период открытия в секундах (общий цикл: открыто + закрыто)
 * @param flowRate Пропускная способность клапана в мл/час (при постоянном открытии)
 * @returns Скорость отбора в мл/час
 */
export function calculateSamplingRate(
	openTime: number,
	period: number,
	flowRate: number
): number {
	console.log(openTime);

	// Рассчитываем коэффициент открытия (долю времени, когда клапан открыт)
	const dutyCycle = openTime / period;

	// Скорость отбора = пропускная способность * коэффициент открытия
	return flowRate * dutyCycle;
}

/**
 * Рассчитывает скорость отбора.
 * @param openTime Время открытия клапана (сек)
 * @param period Период между открытиями (сек)
 * @param settings Настройки SsvcSettings
 * @param valveIndex Индекс клапана (по умолчанию 2)
 * @returns Скорость отбора (мл/час)
 */
export function getSamplingRate(
	openTime: number,
	period: number,
	settings: SsvcSettings,
	valveIndex: number = 2
): number {
	const flowRate = settings.valve_bw?.[valveIndex] || 7000;
	return calculateSamplingRate(openTime, period, flowRate);
}