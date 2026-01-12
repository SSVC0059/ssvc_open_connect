import type { Profile } from '$lib/types/ssvc';

/**
 * Создает конфигурацию UI со значениями по умолчанию.
 */
export function createDefaultProfile(): any {
	return {
		powerKw: 2.5,
		volumeL: 18,
		strengthVol: 40,
		stabilizationMin: 20,
		heads: {
			percent: 3,
			period: 120,
			enabled: true,
			targetCycles: 2,
			targetFlowMlh: 0
		},
		late_heads: {
			percent: 7,
			period: 60,
			enabled: true,
			targetFlowMlh: 150
		},
		hearts: {
			period: 10,
			percent: 75,
			enabled: true,
			targetFlowMlh: 2500
		},
		tails: {
			percent: 5,
			period: 360,
			enabled: false,
			targetFlowMlh: 2500
		},
		ssvcSettings: {
			valve_bw: [7000, 12000, 7000], // Дефолтные значения пропускной способности
			heads: [0, 120],
			late_heads: [0, 60],
			hearts: [0, 10],
			tails: [0, 2],
			valve_bw_tails: 15000
		}
	};
}

interface VaporPhysics {
	strengthMass: number; // Массовая доля спирта в паре
	lMix: number; // Теплота испарения смеси (кДж/кг)
	volVaporPerHour: number; // Потенциальный объем сконденсированного пара (л/ч)
}

export class DistillationCycleModel {
	// private readonly L_ETHANOL = 855;
	// private readonly L_WATER = 2256;
	private readonly RHO_ETHANOL = 0.7893;

	private calculateVaporStrength(b: number): number {
		if (b <= 0) return 0;
		if (b >= 97.17) return 97.17; // Предел азеотропа

		const part1 = 1.04749494522173 * b;
		const part2 = 0.018725730342732 * Math.pow(b, 2);
		const part3 = 0.00011082005414225 * Math.pow(b, 3);

		const expPart1 =
			43.670453012901 * (1 - Math.exp(-0.246196276366746 * Math.pow(b, 0.966659341971092)));
		const expPart2 = Math.exp(0.0638669100921283 * Math.pow(b, 0.437695537197651));

		return part1 - part2 + part3 + expPart1 * expPart2;
	}

	private calculateBoilingTemp(b: number): number {
		if (b <= 0) return 0;
		if (b >= 97.17) return 97.17;

		const term1 = 99.974;
		const term2 = 0.93136 * b;
		const term3 = 0.02395 * Math.pow(b, 2);
		const term4 = 0.000365956 * Math.pow(b, 3);
		const term5 = 0.00000293273 * Math.pow(b, 4);
		const term6 = 0.00000000961 * Math.pow(b, 5);

		return term1 - term2 + term3 - term4 + term5 - term6;
	}

	// Заменяем константы на методы
	private getLWater(temp: number): number {
		// Аппроксимация удельной теплоты испарения воды от температуры
		return 2501 - 2.384 * temp;
	}

	private getLEthanol(temp: number): number {
		// Аппроксимация удельной теплоты испарения этанола от температуры
		return 940 - 1.12 * temp;
	}

	/**
	 * Перевод объемной доли (0.0-1.0) в массовую.
	 * Используется для расчета теплоты испарения смеси.
	 */
	private volToMassFraction(volFraction: number): number {
		if (volFraction <= 0) return 0;
		const rhoEth = 0.7893; // Плотность из вашего Excel
		const rhoWater = 1.0;
		const mEth = volFraction * rhoEth;
		const mWater = (1 - volFraction) * rhoWater;
		return mEth / (mEth + mWater);
	}

	/**
	 * Расчет Флегмового Числа (ФЧ)
	 */
	public calculateRefluxRatio(volVaporPerHour: number, drawRateMlH: number): number {
		if (drawRateMlH <= 0) return 999; // Режим работы "на себя"

		const drawRateLH = drawRateMlH / 1000;
		// ФЧ = (Весь пар / Отбор) - 1. Например: пара 10л, отбор 2л -> ФЧ = 4 (1 часть в отбор, 4 в колонну)
		const refluxRatio = volVaporPerHour / drawRateLH - 1;

		return Math.max(0, refluxRatio);
	}

	/**
	 * Расчет флегмового числа (ФЧ)
	 * @param powerWatts Мощность в Ваттах (например, 2500)
	 * @param drawRateMlh Скорость отбора в мл/ч (например, 2500)
	 */
	private calculatePhlegmatic(powerWatts: number, drawRateMlh: number): number {
		if (drawRateMlh <= 0) return 99.9;

		// 1. Расчет объема испаряемого спирта (мл/ч)
		// powerWatts * 3.6 — это (Вт * 3600 сек) / 1000 Дж
		// 857 — теплота испарения (кДж/кг)
		// 0.789 — плотность (кг/л)
		const volAlc = ((powerWatts * 3.6) / 857 / 0.789) * 1000;

		// 2. Объем возвращаемой флегмы (мл/ч)
		const volReturn = volAlc - drawRateMlh;

		// 3. Флегмовое число = Возврат / Отбор
		// Именно это отношение показывает, сколько частей вернулось на 1 часть отобранную
		const f = volReturn / drawRateMlh;

		return Math.max(0, f);
	}

	/**
	 * Вычисляет физические свойства пара для заданной спиртуозности в кубе и мощности.
	 * Централизованное место для физических расчетов.
	 */
	private getVaporPhysics(b: number, powerWatts: number): VaporPhysics {
		const currentTemp = this.calculateBoilingTemp(b);
		const vVapVol = this.calculateVaporStrength(b) / 100;
		const vVapMass = this.volToMassFraction(vVapVol);

		// Используем динамические коэффициенты на основе температуры
		const lMix =
			vVapMass * this.getLEthanol(currentTemp) + (1 - vVapMass) * this.getLWater(currentTemp);

		// Масса пара в час (кг/ч)
		const massVaporPerHour = (powerWatts * 3600) / (lMix * 1000);

		// Объем пара в час (л/ч). Используем плотность воды при температуре кипения (~0.96)
		const volVaporPerHour =
			(massVaporPerHour * vVapMass) / 0.7893 + (massVaporPerHour * (1 - vVapMass)) / 0.96;

		return { strengthMass: vVapMass, lMix, volVaporPerHour };
	}

	/**
	 * Находит спиртуозность в кубе (vol %) для заданной температуры кипения.
	 * Использует итерационный поиск (метод бисекции).
	 */
	private findStrengthByTemp(targetTemp: number): number {
		if (targetTemp >= 100) return 0;
		if (targetTemp <= 78.15) return 97.17;

		let min = 0;
		let max = 97.17;
		let iterations = 0;

		while (iterations < 20) {
			let mid = (min + max) / 2;
			let temp = this.calculateBoilingTemp(mid);

			if (Math.abs(temp - targetTemp) < 0.01) return mid;

			// Чем выше спиртуозность, тем ниже температура
			if (temp > targetTemp) {
				min = mid;
			} else {
				max = mid;
			}
			iterations++;
		}
		return min;
	}

	public calculateProcess(profile: Profile): Profile {
		const lateHeadsEnabled = profile.late_heads.enabled;
		const tailsEnabled = profile.tails.enabled;
		const netPowerWatts = profile.powerKw * 1000;
		const stabSec = profile.stabilizationMin * 60;

		// Начальные параметры куба
		let currentStrengthVol = profile.strengthVol;
		let currentVolumeL = profile.volumeL;
		let totalAS_ml = currentVolumeL * 1000 * (currentStrengthVol / 100);
		const initialTotalAS = totalAS_ml;

		// Извлекаем пропускную способность и базовые настройки
		const bw_late = profile.ssvcSettings?.valve_bw?.[1] || 0;
		const bw_valve3 = profile.ssvcSettings?.valve_bw?.[2] || 0;

		// --- 1. РАСЧЕТ ГОЛОВ (С учетом опции "Сброс и снижение") ------
		const physicsHeads = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
		const totalHeadsVolMl = (initialTotalAS * profile.heads.percent) / 100;

		const bw_heads = profile.ssvcSettings.valve_bw?.[0] || 0;
		// Приводим значения из настроек SSVC к рабочим числам (исключаем -1 и 0)

		const rTimer = Number(
			profile.ssvcSettings.release_timer < 0 ? 0 : profile.ssvcSettings.release_timer
		);
		const rOpen = Number(
			profile.ssvcSettings.release_speed < 0 ? 0 : profile.ssvcSettings.release_speed
		);
		const hFinal = profile.ssvcSettings.heads_final > 0 ? profile.ssvcSettings.heads_final : 0;
		const headsCyclePeriod = profile.ssvcSettings.heads[1] || 10;

		// Расчет времени одного цикла переиспарения (oneCycleTime)
		const alcoholMassG = initialTotalAS * this.RHO_ETHANOL;
		const totalVaporMassPerSec = netPowerWatts / 1000 / physicsHeads.lMix;
		const alcoholVaporMassPerSecG = totalVaporMassPerSec * physicsHeads.strengthMass * 1000;
		const oneCycleSec_heads =
			alcoholVaporMassPerSecG > 0 ? alcoholMassG / alcoholVaporMassPerSecG : 0;

		// Скорости в мл/ч
		const releaseFlowMlh = (rOpen / headsCyclePeriod) * bw_heads;
		const releaseMl = (releaseFlowMlh * rTimer) / 3600;

		// Определение стартовой скорости основного отбора
		let headsStartFlowMlh: number;
		if (profile.heads.targetFlowMlh && profile.heads.targetFlowMlh > 0) {
			headsStartFlowMlh = Math.min(profile.heads.targetFlowMlh, bw_heads);
		} else {
			// Если авто-расчет: используем желаемое количество циклов переиспарения
			const alcoholMassG = initialTotalAS * this.RHO_ETHANOL;
			const totalVaporMassPerSec = netPowerWatts / 1000 / physicsHeads.lMix;
			const alcoholVaporMassPerSecG = totalVaporMassPerSec * physicsHeads.strengthMass * 1000;
			const oneCycleSec_heads =
				alcoholVaporMassPerSecG > 0 ? alcoholMassG / alcoholVaporMassPerSecG : 0;

			const cycles = profile.heads.targetCycles || 2;
			const targetTotalTimeSec = Math.max(1, oneCycleSec_heads * cycles);

			// Вычисляем необходимую стартовую скорость, чтобы успеть добрать остаток
			const remainingToCollect = Math.max(0, totalHeadsVolMl - releaseMl);
			headsStartFlowMlh = (remainingToCollect * 3600) / Math.max(1, targetTotalTimeSec - rTimer);
		}

		// 1.3 Финальная скорость при снижении
		const headsFinalFlowMlh =
			hFinal > 0 ? (hFinal / headsCyclePeriod) * bw_heads : headsStartFlowMlh;

		// РАСЧЕТ ВРЕМЕНИ ОСНОВНОГО ЭТАПА
		let mainHeadsTimerSec = 0;
		const remainingVolMl = Math.max(0, totalHeadsVolMl - releaseMl);

		if (remainingVolMl > 0) {
			if (hFinal > 0 && headsFinalFlowMlh < headsStartFlowMlh) {
				// Учитываем среднюю скорость при снижении (декременте)
				const avgFlowMlh = (headsStartFlowMlh + headsFinalFlowMlh) / 2;
				mainHeadsTimerSec = (remainingVolMl / avgFlowMlh) * 3600;
			} else {
				mainHeadsTimerSec = (remainingVolMl / headsStartFlowMlh) * 3600;
			}
		}

		const headsTimerSec = rTimer + mainHeadsTimerSec + stabSec;
		// РАСЧЕТ ВРЕМЕНИ ОТКРЫТИЯ КЛАПАНА ДЛЯ ИНТЕРФЕЙСА (в секундах)
		// Это значение, которое "пойдет" в первый элемент массива ssvcSettings.heads
		const headsOpenTime = (headsStartFlowMlh / bw_heads) * headsCyclePeriod;

		const headRefluxRatio = this.calculateRefluxRatio(
			physicsHeads.volVaporPerHour,
			headsStartFlowMlh
		);

		// Обновляем куб после голов
		totalAS_ml -= totalHeadsVolMl * 0.96;
		currentVolumeL -= totalHeadsVolMl / 1000;
		currentStrengthVol = Math.max(0.1, (totalAS_ml / (currentVolumeL * 1000)) * 100);

		// --- 2. РАСЧЕТ ПОДГОЛОВНИКОВ ---
		const physicsLate = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
		let lateHeadsTimerSec = 0,
			lateHeadsOpenTime = 0,
			lateHeadsFlow = 0,
			lateHeadsVol = 0,
			lateHeadsRefluxRatio = 0;

		if (lateHeadsEnabled) {
			lateHeadsVol = initialTotalAS * (profile.late_heads.percent / 100);
			lateHeadsFlow = profile.late_heads.targetFlowMlh || bw_late * 0.1;
			lateHeadsTimerSec = lateHeadsFlow > 0 ? (lateHeadsVol / lateHeadsFlow) * 3600 : 0;
			lateHeadsOpenTime = (lateHeadsFlow / bw_late) * profile.ssvcSettings.late_heads[1];
			lateHeadsRefluxRatio = this.calculateRefluxRatio(physicsLate.volVaporPerHour, lateHeadsFlow);

			totalAS_ml -= lateHeadsVol * 0.96;
			currentVolumeL -= lateHeadsVol / 1000;
			currentStrengthVol = Math.max(0.1, (totalAS_ml / (currentVolumeL * 1000)) * 100);
		}

		// --- 3. РАСЧЕТ ТЕЛА ---
		const physicsHearts = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
		let heartsProductMl: number = 0;
		let calculatedHeartsPercent: number = 0;

		// Приоритет hearts_finish_temp при наличии значения > 0
		if (profile.ssvcSettings.hearts_finish_temp && profile.ssvcSettings.hearts_finish_temp > 0) {
			// 1. Узнаем целевую спиртуозность в кубе при Т завершения
			const targetCubeStrength = this.findStrengthByTemp(profile.ssvcSettings.hearts_finish_temp);

			// 2. Рассчитываем, сколько АС должно остаться в кубе.
			// Важно: объем жидкости в кубе тоже уменьшается, поэтому используем упрощенную пропорцию
			// или более точный расчет остатка:
			const remainingAS = currentVolumeL * 1000 * (targetCubeStrength / 100);

			// 3. Объем тела в мл (с учетом крепости отбора 96%)
			const availableAS = Math.max(0, totalAS_ml - remainingAS);
			heartsProductMl = availableAS / 0.96;

			// Для обратной связи обновляем процент в аналитике
			// Вместо этого вычислите локальную переменную:
			calculatedHeartsPercent = Number(
				(((heartsProductMl * 0.96) / initialTotalAS) * 100).toFixed(1)
			);
		} else {
			// Обратная совместимость: расчет по проценту
			heartsProductMl = (initialTotalAS * (profile.hearts.percent / 100)) / 0.96;
		}

		// Расчет скоростей и таймеров остается прежним
		// --- Внутри calculateProcess ---

		const startTemp = this.calculateBoilingTemp(currentStrengthVol);
		const endTemp = profile.ssvcSettings.hearts_finish_temp || 90;
		const isFormulaEnabled = profile.ssvcSettings.formula;
		const hyst = profile.ssvcSettings.hyst || 0.1;
		const decFactor = (profile.ssvcSettings.decrement || 0) / 100;

		let heartsInitialSpeed = profile.hearts.targetFlowMlh || physicsHearts.volVaporPerHour * 1000;
		let heartsAverageSpeed = heartsInitialSpeed;
		let heartsFinalSpeed = heartsInitialSpeed;

		if (isFormulaEnabled && decFactor > 0 && endTemp > startTemp) {
			// Количество шагов декремента (залетов)
			// Коэффициент 2.0 учитывает инерцию колонны: залет случается чаще,
			// чем меняется средняя Т куба на величину гистерезиса.
			const n = Math.max(0.1, (endTemp - startTemp) / (hyst * 2.0));
			// Скорость в конце процесса
			heartsFinalSpeed = heartsInitialSpeed * Math.pow(1 - decFactor, n);
			const lnFactor = Math.log(1 - decFactor);
			heartsAverageSpeed = heartsInitialSpeed * ((Math.pow(1 - decFactor, n) - 1) / (n * lnFactor));
		}

		const heartsTimerSec =
			heartsAverageSpeed > 0 ? (heartsProductMl / heartsAverageSpeed) * 3600 : 0;
		const heartsOpenTime = (heartsInitialSpeed / bw_valve3) * profile.ssvcSettings.hearts[1];
		const heartsRefluxRatio = this.calculateRefluxRatio(
			physicsHearts.volVaporPerHour,
			heartsInitialSpeed
		);

		// Обновление состояния куба после тела
		totalAS_ml -= heartsProductMl * 0.96;
		currentVolumeL -= heartsProductMl / 1000;
		currentStrengthVol = Math.max(0.1, (totalAS_ml / (currentVolumeL * 1000)) * 100);

		// --- 4. РАСЧЕТ ХВОСТОВ ---
		const physicsTails = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
		const tailsVol = tailsEnabled ? initialTotalAS * (profile.tails.percent / 100) : 0;
		let tailsFlow = profile.tails.targetFlowMlh || bw_valve3;
		const tailsTimerSec = tailsFlow > 0 ? (tailsVol / tailsFlow) * 3600 : 0;
		const tailsOpenTime = (tailsFlow / bw_valve3) * profile.ssvcSettings.tails[1];
		const tailsRefluxRatio = this.calculateRefluxRatio(physicsTails.volVaporPerHour, tailsFlow);

		const totalProcessSec =
			stabSec + headsTimerSec + lateHeadsTimerSec + heartsTimerSec + tailsTimerSec;

		const phlegmaticHeads = this.calculatePhlegmatic(netPowerWatts, headsStartFlowMlh);
		const phlegmaticLateHeads = this.calculatePhlegmatic(netPowerWatts, lateHeadsFlow);
		const phlegmaticHearts = this.calculatePhlegmatic(netPowerWatts, heartsInitialSpeed);
		const phlegmaticTails = this.calculatePhlegmatic(netPowerWatts, tailsFlow);

		return {
			...profile,
			hearts: {
				...profile.hearts,
				percent:
					profile.ssvcSettings.hearts_finish_temp > 0
						? calculatedHeartsPercent
						: profile.hearts.percent
			},
			ssvcSettings: {
				...profile.ssvcSettings,
				heads: [Number(headsOpenTime.toFixed(1)), headsCyclePeriod],
				late_heads: [Number(lateHeadsOpenTime.toFixed(1)), profile.ssvcSettings.late_heads[1]],
				hearts: [Number(heartsOpenTime.toFixed(1)), profile.ssvcSettings.hearts[1]],
				tails: [Number(tailsOpenTime.toFixed(1)), profile.ssvcSettings.tails[1]]
			},
			analytics: {
				totalAS: Math.round(initialTotalAS),
				boilingTemp: Number(this.calculateBoilingTemp(profile.strengthVol).toFixed(2)),
				residueMl: Math.round(currentVolumeL * 1000),
				residualFortress: Number(currentStrengthVol.toFixed(1)),
				oneCycleTime: Math.round(oneCycleSec_heads), // Добавлено для отображения переиспарения
				flows: {
					heads: Math.round(headsStartFlowMlh),
					heads_release: Math.round(releaseFlowMlh),
					heads_final: Math.round(headsFinalFlowMlh),
					late_heads: Math.round(lateHeadsFlow),
					hearts: Math.round(heartsInitialSpeed),
					hearts_avg: Math.round(heartsAverageSpeed),
					hearts_final: Math.round(heartsFinalSpeed),
					tails: Math.round(tailsFlow)
				},
				fractions: {
					releaseMl: Number(releaseMl.toFixed(2)),
					headsMl: Math.round(totalHeadsVolMl),
					lateHeadsMl: Math.round(lateHeadsVol),
					heartsMl: Math.round(heartsProductMl),
					tailsMl: Math.round(tailsVol)
				},
				timers: {
					heads: profile.heads.enabled ? Math.round(headsTimerSec) : 0,
					late_heads: Math.round(lateHeadsTimerSec),
					hearts: Math.round(heartsTimerSec),
					tails: profile.tails.enabled ? Math.round(tailsTimerSec) : 0,
					total_process: Math.round(totalProcessSec)
				},
				refluxRatio: {
					heads: Math.round(headRefluxRatio * 100) / 100,
					late_heads: Math.round(lateHeadsRefluxRatio * 100) / 100,
					hearts: Math.round(heartsRefluxRatio * 100) / 100,
					tails: Math.round(tailsRefluxRatio * 100) / 100
				},
				phlegmatic: {
					heads: Number(phlegmaticHeads.toFixed(2)),
					late_heads: Number(phlegmaticLateHeads.toFixed(2)),
					hearts: Number(phlegmaticHearts.toFixed(2)),
					tails: Number(phlegmaticTails.toFixed(2))
				}
			}
		};
	}
}
