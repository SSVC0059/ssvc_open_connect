<script lang="ts">

    import type {SsvcTelemetryMessage} from '$lib/types/ssvc';
    import type {AlarmThresholdsState,
        SensorMap,
        TemperatureResponse, ThresholdSettings} from '$lib/types/Sensors';
    import {fetchAlarmThresholds} from "$lib/api/ssvcApi";

    const { telemetry, temperatureResponse, alarmThresholds } = $props<{
		telemetry: SsvcTelemetryMessage | null;
		temperatureResponse: TemperatureResponse | null;
        alarmThresholds?: AlarmThresholdsState | null;
	}>();

	let headsBeginSelected = $derived(telemetry.type === "heads")
	let lateHeadsBeginSelected = $derived(telemetry.type === "late_heads")
	let heartsBeginSelected = $derived(telemetry.type === "hearts")
	let tailsBeginSelected = $derived(telemetry.type === "tails")

	let tails_support = $derived(telemetry.volume.hasOwnProperty("tails"))

	// tca
	let tcaTempValue = $derived(getMaxTemperatureByZone("act"));
	let tcaCriticalTemp = $state(55);
	let isCriticalTcaTemp = $state(false);

	// Исходящая вода
  let waterOutTempValue = $derived(getMaxTemperatureByZone("outlet_water"));
	let WaterOutCriticalTemp = $state(55);
	let isCriticalWaterOutTemp = $state(false);

	// Входящая вода
	let waterInTempValue =  $derived(getMaxTemperatureByZone("inlet_water"));
	let WaterInCriticalTemp = $state(30);
	let isCriticalWaterInTemp = $state(false);

	let WaterMinTempValue = $state(10)
	let WaterMaxTempValue = $state(60)
	let WaterMidTempValue = $state(50)
	let WaterGradientOffsets  = $derived(getGradientStops(
		WaterMinTempValue,
		WaterMaxTempValue,
		WaterMidTempValue,
		(waterOutTempValue + waterInTempValue) / 2
	));

	// 	Колонна
	let columnThermalValue = $derived(telemetry.common.tp1);
	let columnMinimalValue = $state(20);
	let columnCriticalTemp = $state(85);
	let columnMidTemp = $state(78.8);
	let columnGradientOffsets  = $derived(
		getGradientStops(columnMinimalValue,
			columnCriticalTemp,
			columnMidTemp,
			columnThermalValue
		));
	let isCriticalColumnTemp = $state(false)

	// тэн
	let heatingOn = $derived(telemetry.common.heatingOn);
	let overclockingOn = $derived(telemetry.common.overclockingOn);

	// Куб
	let tankThermalValue = $derived(telemetry.common.tp2);
	let tankCriticalTemp = $state(100);
	let isCriticalTankTemp = $state(false)

	function getGradientStops(minimalTemperature: number, maximalTemperature: number, midTemperature: number, currentTemperature: number) {
		if (currentTemperature <= midTemperature) {
			// Температура ниже 78.8°C - меняем синий stop
			// Нормализуем от 0 до 1 в диапазоне от минимальной до середины
			const normalized = Math.max(0, (currentTemperature - minimalTemperature) / (midTemperature - minimalTemperature));
			return {
				redOffset: 0,
				blueOffset: normalized  // Синий двигается от 0 до 1
			};
		} else {
			// Температура выше 78.8°C - меняем красный stop
			// Нормализуем от 0 до 1 в диапазоне от середины до критической
			const normalized = Math.min(1, (currentTemperature - midTemperature) / (maximalTemperature - midTemperature));
			return {
				redOffset: normalized,  // Красный двигается от 0 до 1
				blueOffset: 1
			};
		}
	}

	let colorTemperatureWaterOut = $derived(getTemperatureColor(waterOutTempValue));
	let colorTemperatureWaterInput = $derived(getTemperatureColor(waterInTempValue));
	let colorTemperatureTCA = $derived(getTemperatureColor(tcaTempValue));
	let colorTemperatureColumn = $derived(getTemperatureColor(columnThermalValue));


	function getTemperatureColor(temperature: number) {
		const clampedTemp = Math.max(10, Math.min(80, temperature));
		const normalized = (clampedTemp - 10) / (80 - 10);

		const startColor = { r: 23, g: 193, b: 255 };
		const endColor = { r: 255, g: 0, b: 0 };

		const r = Math.round(startColor.r + (endColor.r - startColor.r) * normalized);
		const g = Math.round(startColor.g + (endColor.g - startColor.g) * normalized);
		const b = Math.round(startColor.b + (endColor.b - startColor.b) * normalized);

		return `#${((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1).toUpperCase()}`;
	}

    function checkCriticalTempByZone(zone: string, criticalTempByZone: number): boolean {
        // 1. Проверяем наличие данных
        if (!temperatureResponse || !temperatureResponse[zone]) {
            return false;
        }

        const sensorMap: SensorMap = temperatureResponse[zone];
        const sensorEntries = Object.entries(sensorMap);

        // 2. Если есть глобальные пороги, используем их
        if (alarmThresholds) {
            const thresholds = alarmThresholds.thresholds;
            for (const [address, temp] of sensorEntries) {
                const threshold: ThresholdSettings | undefined = thresholds[address];
                if (threshold && threshold.enabled && temp >= threshold.critical) {
                    return true;
                }
            }
        }

        // 3. Если нет порогов или ни один датчик не критичен по порогу,
        // используем запасной общий порог зоны
        const maxTemp = getMaxTemperatureByZone(zone);
        return maxTemp >= criticalTempByZone;
    }

	function getMaxTemperatureByZone(zone: string ): number {
        if (!temperatureResponse || !temperatureResponse[zone]) {
            return 0;
        }

        const sensorMap: SensorMap = temperatureResponse[zone];
        const sensorEntries = Object.entries(sensorMap); // [[address, temp], ...]

        if (sensorEntries.length === 0) {
            return 0;
        }

        // Находим максимальную температуру в SensorMap
        const maxTemp = Math.max(...sensorEntries.map(([, temp]) => temp));

        return Number(maxTemp.toFixed(2));
	}

    $effect(() => {
        isCriticalTcaTemp = checkCriticalTempByZone("act", tcaCriticalTemp);
        isCriticalWaterOutTemp = checkCriticalTempByZone("outlet_water", WaterOutCriticalTemp);
        isCriticalWaterInTemp = checkCriticalTempByZone("inlet_water", WaterInCriticalTemp);

        // Для колонны и куба (если они не используют DS18B20 с порогами по адресам)
        isCriticalColumnTemp = columnThermalValue > columnCriticalTemp;
        isCriticalTankTemp = tankThermalValue > tankCriticalTemp;
    });

</script>

<svg id="Layer_2" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 280 470" class="svg-column">
	<defs>
		<linearGradient id="linear-gradient" x1="121.01" y1="301.03" x2="121.01" y2="40.04" gradientUnits="userSpaceOnUse">
			<stop offset={columnGradientOffsets.redOffset} stop-color="#ff6a6e"/>
			<stop offset={columnGradientOffsets.blueOffset} stop-color="#00d4ef"/>
		</linearGradient>
		<linearGradient id="linear-gradient-water" x1="121.01" y1="301.03" x2="121.01" y2="40.04" gradientUnits="userSpaceOnUse">
			<stop offset={1-WaterGradientOffsets.blueOffset} stop-color="#00d4ef"/>
			<stop offset={1+WaterGradientOffsets.redOffset} stop-color="#ff6a6e"/>
		</linearGradient>
	</defs>
	<rect class="cls-2" x="241.56" y="228.75" width="37.53" height="18.26"/>
	<rect class="cls-2" x="102.24" y="0.07" width="37.53" height="18.26"/>
	<rect class="cls-2" x="49.35" y="25.59" width="37.53" height="18.26"/>
	<path class="cls-9" d="M250.84,107.51h-105.79v-67.47h-9.62v75.77h109.75c3.12,0,5.65,1.84,5.65,4.11v67.47c0,2.32-1.88,4.19-4.19,4.19h-39.08v75.77h19.23v-8.42h14.43v-8.42h-14.43v-50.51h25.24c4.65,0,8.42-3.77,8.42-8.42v-75.77c0-4.58-3.32-8.3-9.62-8.3Z"/>
	<rect class="cls-5" x="106.58" y="40.04" width="28.85" height="260.99"/>
	<polyline class="cls-3" points="68.12 309.44 106.58 301.03 135.44 301.03 173.9 309.44"/>
	<rect class="cls-3" x="111.39" y="31.62" width="19.23" height="8.42"/>
	<rect class="cls-3" x="106.58" y="23.2" width="28.85" height="8.42"/>
	<path class="cls-3" d="M135.44,132.65v8.42h73.05c2.15,0,3.88,1.74,3.88,3.88v4.53h9.62v-8.42h0c0-4.65-3.77-8.42-8.42-8.42h-78.13Z"/>
	<rect class="cls-3" x="207.56" y="149.49" width="19.23" height="19.23"/>
	<rect class="cls-3" x="15.22" y="141.07" width="19.23" height="19.23"/>
	<rect class="cls-3" x="0.8" y="199.6" width="48.09" height="58.93"/>
	<rect class="cls-10" x="5.77" y="225.47" width="38.14" height="28.85"/>
	<rect class="cls-3" x="193.14" y="309.44" width="48.09" height="58.93"/>
	<rect class="cls-10" x="198.11" y="335.32" width="38.14" height="28.85"/>
	<path class="cls-3" d="M29.65,124.23h0c-5.31,0-9.62,4.31-9.62,9.62v7.22h9.62v-4.32c0-2.26,1.83-4.1,4.1-4.1h72.84v-8.42h-76.94Z"/>
	<rect class="cls-3" x="20.03" y="160.3" width="9.62" height="6.02"/>
	<rect class="cls-3" x="58.5" y="309.44" width="125.02" height="8.42"/>
	<rect class="cls-3" x="68.12" y="317.86" width="105.79" height="143.12"/>
	<rect class="cls-10" x="72.93" y="393.63" width="96.17" height="63.14"/>
	{#if tails_support}
		<path class="cls-3" d="M28.45,283.94c-4.65,0-8.42,3.77-8.42,8.42h0v16.84h9.62v-12.54c0-2.37,1.92-4.3,4.3-4.3h72.64v-8.42h-78.13Z"/>
		<rect class="cls-3" x="15.22" y="309.2" width="19.23" height="19.23"/>
		<rect class="cls-3" x="20.03" y="328.43" width="9.62" height="6.02"/>
		<circle class="cls-8" cx="24.84" cy="318.81" r="4.81"/>
		<rect class="cls-3" x="0.8" y="368.23" width="48.09" height="58.93"/>
		<rect class="cls-10" x="5.77" y="394.1" width="38.14" height="28.85"/>
	{/if}

	<path d="M175.52,451.21h-66.31c-6.97,0-10.63-4.35-10.63-9.72h0c0-5.37,3.65-9.72,10.63-9.72h66.31"
				class={`
							cls-4
							${heatingOn ? 'heating-on' : 'heating-off'}
							${heatingOn && overclockingOn ? 'pulse-animation' : ''}
				`}
	/>
	<rect class="cls-3" x="175.52" y="429.9" width="19.23" height="23.18"/>
	<rect class="cls-3" x="194.76" y="438.6" width="4.81" height="5.79"/>
	<g>
<!--		<path class="cls-1" d="M29.65,430.97h-9.62c-2.66,0-4.81,1.88-4.81,4.21s2.15,4.21,4.81,4.21h9.62c2.66,0,4.81-1.88,4.81-4.21s-2.15-4.21-4.81-4.21Z"/>-->
<!--		<ellipse class="cls-1" cx="20.03" cy="439.39" rx="2.4" ry="2.1"/>-->
<!--		<ellipse class="cls-1" cx="27.24" cy="442.29" rx="2.4" ry="2.1"/>-->
	</g>
	<g>
<!--		<path class="cls-1" d="M29.65,262.74h-9.62c-2.66,0-4.81,1.88-4.81,4.21s2.15,4.21,4.81,4.21h9.62c2.66,0,4.81-1.88,4.81-4.21s-2.15-4.21-4.81-4.21Z"/>-->
<!--		<ellipse class="cls-1" cx="20.03" cy="271.16" rx="2.4" ry="2.1"/>-->
<!--		<ellipse class="cls-1" cx="27.24" cy="274.06" rx="2.4" ry="2.1"/>-->
	</g>
	<g>
<!--		<path class="cls-1" d="M221.99,374.29h-9.62c-2.66,0-4.81,1.88-4.81,4.21s2.15,4.21,4.81,4.21h9.62c2.66,0,4.81-1.88,4.81-4.21s-2.15-4.21-4.81-4.21Z"/>-->
<!--		<ellipse class="cls-1" cx="212.37" cy="382.71" rx="2.4" ry="2.1"/>-->
<!--		<ellipse class="cls-1" cx="219.58" cy="385.61" rx="2.4" ry="2.1"/>-->
	</g>
	<polygon class="cls-3" points="212.37 168.72 212.37 183.16 212.37 183.16 212.37 275.77 221.99 275.77 221.99 183.16 221.99 168.72 212.37 168.72"/>
	<polygon class="cls-9" points="96.97 40.04 96.97 48.46 87.35 48.46 87.35 56.88 96.97 56.88 96.97 115.81 106.58 115.81 106.58 40.04 96.97 40.04"/>
	<path class={`
				${isCriticalWaterInTemp ? 'pulse-animation' : ''}
				`
					} d="M279.7,247.48h-38.48v-19.19h38.48v19.19ZM242.19,246.51h36.53v-17.24h-36.53v17.24Z"/>
	<g>
		<text x="260" y="242"
					class="sensor-text">{waterInTempValue}</text>
	</g>
	<g>
		<path class={`
				${isCriticalTankTemp ? 'pulse-animation' : ''}
				`
					} d="M140.25,351.54h-38.48v-19.19h38.48v19.19ZM102.74,350.56h36.53v-17.24h-36.53v17.24Z"/>
		<g>
			<text x="121.5" y="347"
						class="sensor-text">{tankThermalValue}</text>
		</g>
	</g>
	{#if telemetry.common.mmhg}
		<g>
			<g>
				<text x="125.5" y="377"
							class="sensor-text">{telemetry.common.mmhg}ммРс</text>
			</g>
		</g>
	{/if}
	<g>
		<line class="cls-7" stroke={colorTemperatureWaterInput} x1="246.03" y1="254.72" x2="274.88" y2="254.72"/>
		<polyline class="cls-7" stroke={colorTemperatureWaterInput} points="252.03 250.51 246.03 254.72 252.03 258.93"/>
	</g>
	<g>
		<line class="cls-7" stroke={colorTemperatureWaterOut} x1="53.69" y1="52.67" x2="82.54" y2="52.67"/>
		<polyline class="cls-7" stroke={colorTemperatureWaterOut} points="59.69 48.46 53.69 52.67 59.69 56.88"/>
	</g>
	<g>
		<path class={`
				${isCriticalWaterOutTemp ? 'pulse-animation' : ''}
				`
					} d="M87.36,44.25h-38.48v-19.19h38.48v19.19ZM49.85,43.27h36.53v-17.24h-36.53v17.24Z"/>
		<g>
			<text x="69" y="40"
						class="sensor-text">{waterOutTempValue}</text>
		</g>
	</g>
	<g>
		<path class={`
				${isCriticalTcaTemp ? 'pulse-animation' : ''}
				`
					} d="M140.25,18.83h-38.48v-19.19h38.48v19.19ZM102.74,17.85h36.53v-17.24h-36.53v17.24Z"/>
		<g>
			<text x="121.5" y="14"
						class="sensor-text">{tcaTempValue}</text>
		</g>
	</g>
	<g>
		<g>
			<rect class="cls-2" x="96.01" y="172.64" width="50.01" height="18.22"/>
			<path class={`
				${isCriticalColumnTemp ? 'pulse-animation' : ''}
				`
					} d="M146.5,191.35h-50.98v-19.19h50.98v19.19ZM96.49,190.37h49.03v-17.24h-49.03v17.24Z"/>
		</g>
		<g>
			<text x="121.5" y="186.5"
						class="sensor-text">{columnThermalValue}</text>
		</g>
	</g>
<!--	Капли Хвосты-->
	<path class={`
								cls-6
								${tailsBeginSelected ? "water-drop" : "hidden-element"}
								drop-1
							`}
		d="M28,349.42h0c-1.33,1.64-3.48,1.64-4.81,0h0c-1.33-1.64-1.33-4.31,0-5.95l2.4-3.7,2.4,3.7c1.33,1.64,1.33,4.31,0,5.95Z" />

	<!--	Капли Головы-->
	<path class={`
								cls-6
								${heartsBeginSelected ? "water-drop" : "hidden-element"}
								drop-1
							`}
		d="M220,290.14h0c-1.33,1.64-3.48,1.64-4.81,0h0c-1.33-1.64-1.33-4.31,0-5.95l2.4-3.7,2.4,3.7c1.33,1.64,1.33,4.31,0,5.95Z"/>

	<!--	Капли Тело-->
	<path class={`
								cls-6
								${headsBeginSelected || lateHeadsBeginSelected  ? "water-drop" : "hidden-element"}
								drop-1
							`}
		d="M28,180.69h0c-1.33,1.64-3.48,1.64-4.81,0h0c-1.33-1.64-1.33-4.31,0-5.95l2.4-3.7,2.4,3.7c1.33,1.64,1.33,4.31,0,5.95Z"/>

	<circle class="cls-8" cx="24.84" cy="150.68" r="4.81"/>
	<circle class="cls-8" cx="217.18" cy="159.1" r="4.81"/>
</svg>

<style>

    .sensor-text {
        font-family: Arial, sans-serif;
        font-size: 12px;
        font-weight: bold;
        text-anchor: middle;
    }

    /*Капли*/
    .water-drop {
        animation-play-state: running;
        animation-fill-mode: both;
    }

		.hidden-element {
        display: none;
    }

    /* Анимация падения капель */
    .drop-1 {
        animation: dropFall 3s ease-in infinite;
        animation-delay: 0s;
    }

    /* Ключевые кадры анимации падения */
    @keyframes dropFall {
        0% {
            opacity: 0;
            transform: translateY(-10px) scale(1);
        }
        50% {
            opacity: 1;
            transform: translateY(10px) scale(1);
        }
        100% {
            opacity: 0;
            transform: translateY(60px) scale(1);
        }
    }

    .critical-temp {
        stroke: #ff0000;
        stroke-width: 4px;
        animation: criticalPulse 1s infinite;
    }

    .heating-on {
        stroke: var(--red-700);
        stroke-width: 3px;
    }

    .heating-off {
        stroke: #000000;
        stroke-width: 2px;
    }

    .pulse-animation {
        animation: heatingPulse 0.5s infinite alternate;
    }

    @keyframes criticalPulse {
        0% { stroke: var(--red-300); }
        50% { stroke: var(--red-500); }
        100% { stroke: var(--red-700); }
    }

    @keyframes heatingPulse {
        0% {
						stroke-width: 1px;
            stroke: var(--color-red-500)
				}
        100% {
						stroke-width: 2px;
            stroke: var(--red-700)
				}
    }

    .svg-column {
        /* Базовые настройки */
        width: 100%;
        height: auto;
        /*max-width: 600px; !* Максимальный размер для больших экранов *!*/
        max-height: 73vh; /* Ограничение по высоте */

        /* Центрирование */
        display: block;
        margin: 0 auto;

        /* Тени и цвет */
        color: black;
        filter: drop-shadow(-10px -10px 15px rgb(0 0 0 / 40%));

        /* Адаптивность для мобильных */
        @media (max-width: 768px) {
            max-width: 90vw;
            max-height: 70vh;
        }

        /* Для очень маленьких экранов */
        @media (max-width: 480px) {
            max-width: 95vw;
            max-height: 60vh;
        }
    }

    .cls-1 {
        fill: #64ff00;
    }

    .cls-2, .cls-3 {
        fill: #fff;
    }

    .cls-4 {
        stroke-width: 3px;
    }

    .cls-4, .cls-5, .cls-6, .cls-7, .cls-8, .cls-3, .cls-9, .cls-10 {
        stroke-miterlimit: 10;
    }

    .cls-4, .cls-7 {
        fill: none;
    }


    .cls-8 {
        stroke: #ff0013;
    }

    .cls-5 {
        fill: url(#linear-gradient);
    }
     .cls-9 {
        fill: url(#linear-gradient-water);
    }

    .cls-5, .cls-6, .cls-3, .cls-9, .cls-10 {
        stroke-width: 1px;
    }

    .cls-5, .cls-3, .cls-9, .cls-10 {
        stroke: #000;
    }

    .cls-6 {
        stroke: #00d4ef;
    }

    .cls-6, .cls-10 {
        fill: #00d4ef;
    }


    .cls-8 {
        fill: #ff0013;
    }

</style>
