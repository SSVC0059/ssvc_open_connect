<script lang="ts">
	import {
		updateAlarmThresholds,
		updateSensorZone
	} from '$lib/api/ssvcApi';
	import { availableZones } from '$lib/components/OCSettings/OSSettingsHelper';
	import { Spinner } from 'flowbite-svelte';
	import type { AlarmThresholdsState, ThresholdSettings, SensorReading } from '$lib/types/Sensors';

	let {
		sensor,
		onUpdate,
		alarmThresholdsState
	}: {
		sensor: SensorReading;
		onUpdate: () => void;
		alarmThresholdsState: AlarmThresholdsState | null;
	} = $props();


	let error: string | null = $state(null);
	let movingSensor: string | null = $state(null);
	let savingThresholds: boolean = $state(false);

	// --- Состояние для редактирования порогов ---
	let editingSensor: string | null = $state(null);
	let monitoringEnabled: boolean = $state(true);
	let minThreshold: number = $state(0);
	let dangerousThreshold: number = $state(0);
	let criticalThreshold: number = $state(0);


	let currentThresholds = $derived<ThresholdSettings | undefined>(
		// Используем имя пропса: alarmThresholdsState
		alarmThresholdsState ? alarmThresholdsState.thresholds[sensor.address] : undefined
	);

	function calculateTempPercentage(temp: number): number {
		const maxTemp = 100;
		return Math.min(Math.max((temp / maxTemp) * 100, 0), 100);
	}

	let selectedZoneForSensor: Record<string, string> = $state({});

	function getSelectedZone(address: string): string {
		return selectedZoneForSensor[address] || '';
	}

	function setSelectedZone(address: string, value: string) {
		selectedZoneForSensor[address] = value;
	}

	// Обработка перемещения датчика в новую зону
	async function moveSensor(address: string) {
		const newZone = selectedZoneForSensor[address];
		if (!newZone) {
			error = "Сначала выберите зону.";
			return;
		}

		movingSensor = address;
		error = null;

		try {
			const success = await updateSensorZone(address, newZone);
			if (success) {
				await new Promise(resolve => setTimeout(resolve, 1500));
				onUpdate?.();
				delete selectedZoneForSensor[address];
			} else {
				error = "Ошибка при перемещении зоны (API).";
			}
		} catch (err) {
			console.error('Move sensor error:', err);
			error = "Произошла ошибка сети/сервера при перемещении.";
		} finally {
			movingSensor = null;
		}
	}


	// Инициализируем пороги при начале редактирования
	function startEditing(sensor: SensorReading) {
		editingSensor = sensor.address;
		error = null;

		// УЛУЧШЕНИЕ: Используем $derived переменную currentThresholds
		const settings: ThresholdSettings | undefined = currentThresholds;
		if (settings) {
			// Устанавливаем текущие значения
			monitoringEnabled = settings.enabled;
			minThreshold = settings.min;
			dangerousThreshold = settings.dangerous;
			criticalThreshold = settings.critical;
		} else {
			// Используем значения по умолчанию, если настроек нет
			monitoringEnabled = true;
			minThreshold = 0.0;
			dangerousThreshold = 0.0;
			criticalThreshold = 0.0;
		}
	}

	// --- Функция: Сохранение порогов ---
	async function saveThresholds(address: string) {
		// ... (Валидация остается прежней)
		if (dangerousThreshold >= criticalThreshold) {
			error = "Порог предупреждения должен быть меньше критического порога.";
			return;
		}

		savingThresholds = true;
		error = null;

		try {
			// 1. Создаем обновленный объект ThresholdSettings для текущего сенсора
            const updatedSettings: ThresholdSettings = {
                enabled: monitoringEnabled,
                min: minThreshold,
                dangerous: dangerousThreshold,
                critical: criticalThreshold
            };

			// 2. Клонируем текущее состояние порогов, чтобы внести изменение
			const newState: AlarmThresholdsState = {
				thresholds: {
					// Используем пропс, как и было запланировано
					...(alarmThresholdsState?.thresholds || {}),
					[address]: updatedSettings // Обновляем настройки только для текущего адреса
				}
			};

			// 3. Используем API для полной структуры [cite: 21]
			const success = await updateAlarmThresholds(newState);

			if (success) {
				editingSensor = null;
				onUpdate?.();
			} else {
				error = "Ошибка сохранения порогов (API).";
			}
		} catch (err) {
			console.error('Save thresholds error:', err);
			error = "Произошла ошибка сети/сервера.";
		} finally {
			savingThresholds = false;
		}
	}


	function handleZoneChange(address: string, event: Event) {
		const select = event.target as HTMLSelectElement;
		if (select) {
			selectedZoneForSensor[address] = select.value;
		}
	}
</script>

{#if sensor}
	<div class="sensor-card">
		{#if error}
			<div class="error-message text-red-500 p-2 my-2 border border-red-500 rounded">
				{error}
			</div>
		{/if}

		<div class="sensor-header">
			<div class="sensor-address">
				<span class="sensor-span">Адрес:</span>
				<span class="sensor-value font-mono" title={sensor.address}>{sensor.address}</span>
			</div>
			<div class="sensor-temperature">
				<span class="sensor-span">Темп.:</span>
				<span class="sensor-value">{sensor.temp}°C</span>
			</div>
		</div>

		<div class="temperature-bar-compact-container">
			<div class="temperature-bar-compact">
				{#if currentThresholds?.dangerous}
					<div
						class="temperature-threshold-marker warning-marker"
						style={`left: ${calculateTempPercentage(currentThresholds.dangerous)}%`}
						title={`Предупреждение: ${currentThresholds.dangerous}°C`}
					></div>
				{/if}
				{#if currentThresholds?.critical}
					<div
						class="temperature-threshold-marker critical-marker"
						style={`left: ${calculateTempPercentage(currentThresholds.critical)}%`}
						title={`Критично: ${currentThresholds.critical}°C`}
					></div>
				{/if}
				<div
					class="temperature-fill-compact"
					style={`width: ${calculateTempPercentage(sensor.temp)}%`}
				></div>
			</div>

			<div class="temperature-labels-compact">
				<span>0°C</span>
				<span>25°C</span>
				<span>50°C</span>
				<span>75°C</span>
				<span>100°C</span>
			</div>
		</div>

		<div class="sensor-controls">
			{#if editingSensor === sensor.address}
				<div class="threshold-editor">
					<div class="input-group">
						<span class="input-span">Минимум (°C)</span>
						<div class="input-wrapper">
							<input
								type="number"
								bind:value={minThreshold}
								class="input-field"
								min="0"
								step="0.01"
								max="100"
							/>
							<span class="input-unit">°C</span>
						</div>
					</div>
					<div class="input-group">
						<span class="input-span">Порог предупреждения (°C)</span>
						<div class="input-wrapper">
							<input
								type="number"
								bind:value={dangerousThreshold}
								class="input-field"
								min="0"
								step="0.01"
								max="100"
							/>
							<span class="input-unit">°C</span>
						</div>
					</div>
					<div class="input-group">
						<span class="input-span">Критический порог (°C)</span>
						<div class="input-wrapper">
							<input
								type="number"
								bind:value={criticalThreshold}
								class="input-field"
								min="0"
								step="0.01"
								max="100"
							/>
							<span class="input-unit">°C</span>
						</div>
					</div>
					<div class="control-actions">
						<button
							onclick={() => saveThresholds(sensor.address)}
							class="btn btn-primary"
							disabled={savingThresholds}
						>
							{#if savingThresholds}
								<Spinner />
							{:else}
								Сохранить
							{/if}
						</button>
						<button
							onclick={() => (editingSensor = null)}
							class="btn"
							disabled={savingThresholds}
						>
							Отмена
						</button>
					</div>
				</div>
			{:else}
				<div class="sensor-actions">
					<button
						onclick={() => startEditing(sensor)}
						class="btn btn-primary"
					>
						Пороги
					</button>
					<div class="zone-controls-compact">
						<select
							class="input-field"
							value={getSelectedZone(sensor.address)}
							onchange={(e) => handleZoneChange(sensor.address, e)}
							disabled={movingSensor === sensor.address}
						>
							<option value="">Зона...</option>
							{#each availableZones as zoneOption}
								<option value={zoneOption.value} disabled={zoneOption.value === sensor.zone}>
									{zoneOption.label}
								</option>
							{/each}
						</select>
						<button
							onclick={() => moveSensor(sensor.address)}
							class="btn btn-warning"
							disabled={movingSensor === sensor.address || !getSelectedZone(sensor.address)}
						>
							{#if movingSensor === sensor.address}
								<Spinner />
							{:else}
								Переместить
							{/if}
						</button>
					</div>
				</div>
			{/if}
		</div>
	</div>
{/if}

