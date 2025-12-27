import type { FractionCalcResult, Profile, SsvcSettings } from '$lib/types/ssvc';

/**
 * Создает конфигурацию UI со значениями по умолчанию.
 * @returns {Profile} Конфигурация по умолчанию.
 */
export function createDefaultUIConfig(): Profile {
	return {
		id: '',
		name: '',
		createdAt: '',
		isApplied: false,
		rawVolumeLiters: 10,
		rawStrength: 40,
		rawTemperature: 20,
		targetStrength: 96.6,
		collectionNodeVolumeMl: 50, // Объем узла отбора по умолчанию
		headsTotalPercent: 3,
		lateHeadsPercent: 7,
		heartsPercent: 75,
		tailsPercent: 15,
		ssvcSettings: {} as SsvcSettings
	};
}


export function tempCorrectionAA(temp: number, strength: number) {
	const correction = 0.37 * (temp - 20);
	return round(Math.max(0, Math.min(100, strength - correction)), 1);
}



export function getTotalAAMl(rawVolumeLiters: number, realRawStrength:number, temp:number = 20): number {
	const roundedVol = round(rawVolumeLiters, 1);
	return (roundedVol * 1000 * tempCorrectionAA(temp, realRawStrength)) / 100;
}

/**
 * Рассчитывает параметры отбора голов.
 * @param totalAAml Общий объем абсолютного спирта в мл.
 * @param profileData Данные профиля.
 * @param releaseEnabled Включен ли режим "Сброс и снижение".
 * @returns {FractionCalcResult} Результат расчета.
 */
export function calculateHeadsRampStage(
	totalAAml: number,
	profileData: Profile,
	releaseEnabled: boolean
): FractionCalcResult {
	const { ssvcSettings, headsTotalPercent } = profileData;
	const headsTotalMl = (totalAAml * headsTotalPercent) / 100;
	let remainingMl = headsTotalMl;
	let totalDurationSec = 0;

	if (releaseEnabled && ssvcSettings) {
		const { release_timer = 0, release_speed = 0, valve_bw, heads_final = 0 } = ssvcSettings;
		const valveBwMlPerSec = (valve_bw?.[0] ?? 0) / 3600;

		// Расчет сброса
		if (release_timer > 0 && release_speed > 0) {
			const releaseVolume = release_speed * valveBwMlPerSec * release_timer;
			remainingMl -= releaseVolume;
			totalDurationSec += release_timer;
		}

		// Расчет плавного снижения
		const { heads } = ssvcSettings;
		if (heads && heads.length === 2 && remainingMl > 0) {
			const [initialOpenTime, period] = heads;
			const finalOpenTime = heads_final;

			// Это упрощенная модель. Для точного расчета нужен интеграл.
			// Предположим линейное снижение скорости отбора.
			const initialRate = (initialOpenTime / period) * (valve_bw?.[0] ?? 0); // мл/ч
			const finalRate = (finalOpenTime / period) * (valve_bw?.[0] ?? 0); // мл/ч
			const averageRate = (initialRate + finalRate) / 2; // мл/ч

			if (averageRate > 0) {
				const rampDurationHours = remainingMl / averageRate;
				totalDurationSec += rampDurationHours * 3600;
			}
		}
	} else if (ssvcSettings) {
		const { heads, valve_bw } = ssvcSettings;
		if (heads && heads.length === 2 && valve_bw && valve_bw.length > 0) {
			const [openTime, period] = heads;
			const valveBwMlPerHour = valve_bw[0];
			const selectionRateMlPerHour = (openTime / period) * valveBwMlPerHour;

			if (selectionRateMlPerHour > 0) {
				const durationHours = headsTotalMl / selectionRateMlPerHour;
				totalDurationSec = durationHours * 3600;
			}
		}
	}

	return {
		ml: round(headsTotalMl, 1),
		durationMin: round(totalDurationSec / 60, 0)
	};
}

/**
 * Предлагает настройки для отбора голов на основе желаемых параметров.
 * @param desiredInitialRateMlph Желаемая начальная скорость отбора (мл/ч).
 * @param desiredFinalRateMlph Желаемая конечная скорость отбора (мл/ч).
 * @param desiredReleaseVolumeMl Желаемый объем сброса (мл).
 * @param profileData Данные профиля.
 * @returns {Partial<SsvcSettings>} Рекомендуемые настройки.
 */
export function suggestHeadsSettings(
	desiredInitialRateMlph: number,
	desiredFinalRateMlph: number,
	desiredReleaseVolumeMl: number,
	profileData: Profile
): Partial<SsvcSettings> {
	const { ssvcSettings } = profileData;
	const valveBwMlPerHour = ssvcSettings?.valve_bw?.[0] ?? 0;
	if (valveBwMlPerHour === 0) return {}; // Невозможно рассчитать без пропускной способности

	const suggestedSettings: Partial<SsvcSettings> = {};
	const period = 360; // Предположим стандартный период 6 минут (360 секунд)

	// --- Расчет для режима "Сброс и снижение" ---

	// 1. Расчет параметров сброса (release_timer, release_speed)
	// Для сброса будем использовать максимальную разумную скорость, например, 90% от пропускной способности клапана.
	const releaseRateMlph = valveBwMlPerHour * 0.9;
	const releaseSpeed = (releaseRateMlph / valveBwMlPerHour) * period; // Это будет наше "время открытия клапана" для сброса
	const releaseDurationHours = desiredReleaseVolumeMl / releaseRateMlph;
	const releaseTimerSec = releaseDurationHours * 3600;

	suggestedSettings.release_speed = round(releaseSpeed, 1);
	suggestedSettings.release_timer = round(releaseTimerSec, 0);

	// 2. Расчет начальной скорости отбора (heads)
	let initialOpenTime = (desiredInitialRateMlph / valveBwMlPerHour) * period;
	initialOpenTime = Math.max(0.1, Math.min(period, initialOpenTime)); // Ограничения
	suggestedSettings.heads = [round(initialOpenTime, 1), period];

	// 3. Расчет конечной скорости отбора (heads_final)
	let finalOpenTime = (desiredFinalRateMlph / valveBwMlPerHour) * period;
	finalOpenTime = Math.max(0.1, Math.min(period, finalOpenTime)); // Ограничения
	suggestedSettings.heads_final = round(finalOpenTime, 1);

	return suggestedSettings;
}


/**
 * Вспомогательный метод округления до заданного знака
 */
function round(v: number, decimals: number): number {
	const p = Math.pow(10, decimals);
	return Math.round(v * p) / p;
}
