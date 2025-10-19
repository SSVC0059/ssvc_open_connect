<svelte:options runes />

<script lang="ts">
	import { fetchAlarmThresholds, fetchSensorsTemperatureByZone } from '$lib/api/ssvcApi';
	import type { TemperatureResponse, SensorData } from '$lib/types/OCSettings';
	import { getZoneDescription } from '$lib/components/OCSettings/OSSettingsHelper';
	import SensorCard from '$lib/components/OCSettings/SensorCard.svelte';
	import type { AlarmThresholdsState } from '$lib/types/ssvc';

	let temperatureResponse: TemperatureResponse | null | undefined = $state();
	let alarmThresholdsState: AlarmThresholdsState | null = $state(null); // <-- НОВОЕ СОСТОЯНИЕ
	let isLoading: boolean = $state(true);
	let error: string | null = $state(null);

	// Функция для перезагрузки данных
	const reloadSensors = async () => {
		try {
			// Загружаем обе коллекции
			const [tempResp, thresholdsResp] = await Promise.all([
				fetchSensorsTemperatureByZone(),
				fetchAlarmThresholds()
			]);
			temperatureResponse = tempResp;
			alarmThresholdsState = thresholdsResp; // <-- Сохраняем пороги

		} catch (err) {
			console.error('Ошибка загрузки данных:', err);
			error = 'Ошибка при загрузке данных о датчиках и порогах.';
		} finally {
			isLoading = false;
		}
	};

	// Автоматически запускается при монтировании компонента
	$effect(() => {
		reloadSensors();
	});

	// Форматирование времени
	const formatTimestamp = (timestamp: number): string => {
		return new Date(timestamp).toLocaleTimeString();
	};

	const zoneEntries = $derived(
		(temperatureResponse && temperatureResponse.zones)
			? Object.entries(temperatureResponse.zones)
			: []
	);

</script>

{#if isLoading}
	<div class="loading-container">
		<p class="loading-text">Загрузка датчиков...</p>
		<div class="loading-spinner"></div>
	</div>
{:else if error}
	<div class="error-container">
		<p class="error-text">Ошибка: {error}</p>
	</div>
{:else}
	<div class="sensors-main-container">
		{#each zoneEntries as [zone, sensors]}
			<div class="zone-panel">
				<h2 class="zone-title">
					{zone === 'unknown' ? 'Неизвестная зона' : getZoneDescription(zone)}
				</h2>
				<div class="sensors-grid">
					{#each sensors as sensor (sensor.address)}
						<SensorCard
							{sensor}
							{alarmThresholdsState} onUpdate={reloadSensors}
						/>
					{/each}
				</div>
			</div>
		{:else}
			<div class="empty-state">
				<p class="empty-text">Нет данных о датчиках</p>
			</div>
		{/each}
	</div>
{/if}

