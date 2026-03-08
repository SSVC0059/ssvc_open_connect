<script lang="ts">
	import { updateAlarmThresholds, updateSensorZone } from '$lib/api/ssvcApi';
	import { availableZones } from '$lib/components/OCSettings/OSSettingsHelper';
	import type { AlarmThresholdsState, ThresholdSettings, SensorReading } from '$lib/types/Sensors';
	import ThresholdCalculatorModal from './ThresholdCalculatorModal.svelte';

	let {
		sensor,
		onUpdate,
		alarmThresholdsState,
		sensorsType
	}: {
		sensor: SensorReading;
		onUpdate: () => void;
		alarmThresholdsState: AlarmThresholdsState | null;
		sensorsType: string;
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
	let showCalculator = $state(false);

	let currentThresholds = $derived<ThresholdSettings | undefined>(
		alarmThresholdsState ? alarmThresholdsState.thresholds[sensor.address] : undefined
	);

	function calculateTempPercentage(temp: number): number {
		const maxTemp = 100;
		return Math.min(Math.max((temp / maxTemp) * 100, 0), 100);
	}

	function calculatePressurePercentage(value: number): number {
		const min = 730;
		const max = 790;
		return Math.min(Math.max(((value - min) / (max - min)) * 100, 0), 100);
	}

	let selectedZoneForSensor: Record<string, string> = $state({});

	function getSelectedZone(address: string): string {
		return selectedZoneForSensor[address] || sensor.zone || '';
	}

	async function moveSensor(address: string) {
		const newZone = selectedZoneForSensor[address];
		if (!newZone) {
			error = 'Сначала выберите зону.';
			return;
		}

		movingSensor = address;
		error = null;
		try {
			const success = await updateSensorZone(address, newZone);
			if (success) {
				await new Promise((resolve) => setTimeout(resolve, 1000));
				onUpdate?.();
				delete selectedZoneForSensor[address];
			} else {
				error = 'Ошибка при перемещении зоны (API).';
			}
		} catch (err) {
			console.error('Move sensor error:', err);
			error = 'Произошла ошибка сети/сервера при перемещении.';
		} finally {
			movingSensor = null;
		}
	}

	function startEditing(sensor: SensorReading) {
		editingSensor = sensor.address;
		error = null;
		const settings: ThresholdSettings | undefined = currentThresholds;
		if (settings) {
			monitoringEnabled = settings.enabled;
			minThreshold = settings.min;
			dangerousThreshold = settings.dangerous;
			criticalThreshold = settings.critical;
		} else {
			monitoringEnabled = true;
			minThreshold = 0.0;
			dangerousThreshold = 0.0;
			criticalThreshold = 0.0;
		}
	}

	async function saveThresholds(address: string) {
		if (dangerousThreshold >= criticalThreshold) {
			error = 'Порог предупреждения должен быть меньше критического порога.';
			return;
		}

		savingThresholds = true;
		error = null;

		try {
			const updatedSettings: ThresholdSettings = {
				enabled: monitoringEnabled,
				min: minThreshold,
				dangerous: dangerousThreshold,
				critical: criticalThreshold
			};
			const newState: AlarmThresholdsState = {
				thresholds: {
					...(alarmThresholdsState?.thresholds || {}),
					[address]: updatedSettings
				}
			};
			const success = await updateAlarmThresholds(newState);

			if (success) {
				editingSensor = null;
				onUpdate?.();
			} else {
				error = 'Ошибка сохранения порогов (API).';
			}
		} catch (err) {
			console.error('Save thresholds error:', err);
			error = 'Произошла ошибка сети/сервера.';
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

	function handleThresholdsCalculated(thresholds: {
		min: number;
		dangerous: number;
		critical: number;
	}) {
		minThreshold = thresholds.min;
		dangerousThreshold = thresholds.dangerous;
		criticalThreshold = thresholds.critical;
		showCalculator = false;
	}
</script>

{#if sensor}
	<div class="sensor-card">
		{#if error}
			<div class="error-message text-red-500 p-2 my-2 border border-red-500 rounded">{error}</div>
		{/if}

		<div class="sensor-header">
			<div class="sensor-address">
				<span class="sensor-span">Адрес:</span>
				<span class="sensor-value font-mono" title={sensor.address}>{sensor.address}</span>
			</div>
		<div class="sensor-temperature">
			{#if sensor.data.type === 'thermal'}
				<span class="sensor-span">Темп.:</span>
			{:else if sensor.data.type?.toLowerCase() === 'pressure'}
				<span class="sensor-span">Давление.:</span>
			{/if}

				<span class="sensor-value"> {sensor.data?.v ?? '--'} {sensor.data?.u ?? ''} </span>
			</div>
		</div>

		{#if sensor.data.type === 'thermal'}
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
						style={`width: ${calculateTempPercentage(sensor.data?.v ?? 0)}%`}
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
		{:else if sensor.data.type === 'pressure'}
			<div class="temperature-bar-compact-container">
				<div class="temperature-bar-compact">
					{#if currentThresholds?.dangerous}
						<div
							class="temperature-threshold-marker warning-marker"
							style={`left: ${calculatePressurePercentage(currentThresholds.dangerous)}%`}
							title={`Предупреждение: ${currentThresholds.dangerous} ${sensor.data.u}`}
						></div>
					{/if}
					{#if currentThresholds?.critical}
						<div
							class="temperature-threshold-marker critical-marker"
							style={`left: ${calculatePressurePercentage(currentThresholds.critical)}%`}
							title={`Критично: ${currentThresholds.critical} ${sensor.data.u}`}
						></div>
					{/if}
					<div
						class="temperature-fill-compact"
						style={`width: ${calculatePressurePercentage(sensor.data?.v ?? 0)}%`}
					></div>
				</div>

				<div class="temperature-labels-compact">
					<span>730 mmРс</span>
					<span>760 mmРс</span>
					<span>790 mmРс</span>
				</div>
			</div>
		{/if}

		<div class="sensor-controls">
			{#if editingSensor === sensor.address}
				<div class="threshold-editor">
					<div class="input-group">
						<span class="input-span">Минимум ({sensor.data?.u ?? ''})</span>
						<div class="input-wrapper">
							<input
								type="number"
								bind:value={minThreshold}
								class="input-field"
								min="0"
								step="0.01"
								max="1000"
							/>
							<span class="input-unit">{sensor.data?.u ?? ''}</span>
						</div>
					</div>
					<div class="input-group">
						<span class="input-span">Порог предупреждения ({sensor.data.u})</span>
						<div class="input-wrapper">
							<input
								type="number"
								bind:value={dangerousThreshold}
								class="input-field"
								min="0"
								step="0.01"
								max="1000"
							/>
							<span class="input-unit">{sensor.data.u}</span>
						</div>
					</div>
					<div class="input-group">
						<span class="input-span">Критический порог ({sensor.data.u})</span>
						<div class="input-wrapper">
							<input
								type="number"
								bind:value={criticalThreshold}
								class="input-field"
								min="0"
								step="0.01"
								max="1000"
							/>
							<span class="input-unit">{sensor.data.u}</span>
						</div>
					</div>
					<div class="control-actions">
						<button
							onclick={() => saveThresholds(sensor.address)}
							class="btn btn-primary"
							disabled={savingThresholds}
						>
							{#if savingThresholds}
								<span class="loading loading-spinner loading-md inline-block shrink-0" aria-hidden="true"></span>
							{:else}
								Сохранить
							{/if}
						</button>
						<button
							onclick={() => (editingSensor = null)}
							class="btn btn-back"
							disabled={savingThresholds}
						>
							Отмена
						</button>
						{#if sensor.data.type === 'pressure'}
							<button
								onclick={() => (showCalculator = true)}
								class="btn btn-warning"
								disabled={savingThresholds}
							>
								Авторасчет
							</button>
						{/if}
					</div>
				</div>
			{:else}
				<div class="sensor-actions">
					<button onclick={() => startEditing(sensor)} class="btn btn-primary"> Пороги </button>
					<div class="zone-controls-compact">
						<select
							class="input-field"
							value={getSelectedZone(sensor.address)}
							onchange={(e) => handleZoneChange(sensor.address, e)}
							disabled={movingSensor === sensor.address}
						>
							<option value="">Зона...</option>
							{#each availableZones as zoneOption}
								<option value={zoneOption.value} disabled={sensor.zone ? zoneOption.value === sensor.zone : false}>
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
								<span class="loading loading-spinner loading-md inline-block shrink-0" aria-hidden="true"></span>
							{:else}
								Переместить
							{/if}
						</button>
					</div>
				</div>
			{/if}
		</div>
	</div>

	{#if sensor.data.type?.toLowerCase() === 'pressure'}
		<ThresholdCalculatorModal
			show={showCalculator}
			currentValue={sensor.data?.v ?? 0}
			onSave={handleThresholdsCalculated}
			onClose={() => (showCalculator = false)}
		/>
	{/if}
{/if}

<style lang="scss">
	@use "$lib/styles/base/variables" as v;
	@use "$lib/styles/base/mixins" as m;

	.sensor-card {
		padding: 1rem;
		border-radius: var(--border-radius);
		margin-bottom: 1rem;
		border: var(--glass-border);
	}

	.sensor-header {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
		margin-bottom: 1rem;

		@media (min-width: v.$breakpoint-sm) {
			flex-direction: row;
			justify-content: space-between;
			align-items: center;
		}
	}

	.sensor-address,
	.sensor-temperature {
		display: flex;
		align-items: center;
		gap: 0.5rem;
		min-width: 0;
	}

	.sensor-label {
		color: var(--primary-600);
		font-weight: 500;
		font-size: 0.9rem;
		flex-shrink: 0;
	}

	.sensor-value {
		color: var(--primary-800);
		font-weight: 600;
		font-size: 1rem;

		&.font-mono {
			font-family: monospace;
			font-style: italic;
			font-size: 0.9rem;
		}
	}

	@media (prefers-color-scheme: dark) {
		.sensor-temperature,
		.sensor-value {
			color: oklch(var(--bc));
		}
	}

	.temperature-bar-compact-container {
		margin: 0.5rem 0;
	}

	.temperature-bar-compact {
		height: 0.625rem;
		background-color: var(--primary-100);
		border-radius: 9999px;
		overflow: hidden;
		position: relative;
		margin-bottom: 0.25rem;
	}

	.temperature-fill-compact {
		height: 100%;
		background: linear-gradient(90deg, var(--blue-500), var(--red-600));
		border-radius: 9999px;
		transition: width 0.3s ease;
	}

	.temperature-threshold-marker {
		position: absolute;
		top: 0;
		height: 100%;
		width: 2px;
		z-index: var(--z-dropdown);
	}

	.warning-marker {
		background-color: var(--yellow-500);
	}

	.critical-marker {
		background-color: var(--red-500);
	}

	.temperature-labels-compact {
		display: flex;
		justify-content: space-between;
		font-size: 0.75rem;
		color: var(--primary-500);
		overflow: hidden;

		span {
			white-space: nowrap;
		}
	}

	.sensor-controls {
		margin-top: 1rem;
	}

	.sensor-actions {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;

		@media (min-width: v.$breakpoint-sm) {
			flex-direction: row;
			align-items: stretch;
		}
	}

	.zone-controls-compact {
		display: flex;
		flex-direction: column;
		gap: 0.5rem;
		flex: 1;
		min-width: 0;

		@media (min-width: v.$breakpoint-sm) {
			flex-direction: row;
		}

		:global(.input-field) {
			flex: 1;
			min-width: 0;
		}
	}

	.threshold-editor {
		display: flex;
		flex-direction: column;
		gap: 1rem;
	}

	.control-actions {
		display: flex;
		gap: 0.5rem;

		:global(.btn) {
			flex: 1;
			justify-content: center;
		}
	}
</style>
