<svelte:options runes />

<script lang="ts">
	import { fetchSensorsTemperatureByZone } from '$lib/api/settingsApi';
	import type { SensorData, SensorsData } from '$lib/types/OCSettings';
	import { getZoneDescription } from '$lib/components/OCSettings/OSSettingsHelper';
	import SensorCard from '$lib/components/OCSettings/SensorCard.svelte';

	let sensorData: SensorsData | null | undefined = $state();
	let isLoading: boolean = $state(true);
	let error: string | null = $state('');

	// Функция для перезагрузки данных
	const reloadSensors = async () => {
		try {
			sensorData = await fetchSensorsTemperatureByZone();
		} catch (err) {
			if (err instanceof Error) {
				error = err.message;
			} else {
				error = 'Неизвестная ошибка';
			}
		} finally {
			isLoading = false;
		}
	};

	// Автоматически запускается при монтировании компонента
	$effect(() => {
		reloadSensors();
	});

	// Вспомогательные функции
	const groupByZone = (data: SensorData[]): Record<string, SensorData[]> => {
		return data.reduce(
			(acc, sensor) => {
				const zone = sensor.zone || 'unknown';
				if (!acc[zone]) acc[zone] = [];
				acc[zone].push(sensor);
				return acc;
			},
			{} as Record<string, SensorData[]>
		);
	};

	// Форматирование времени
	const formatTimestamp = (timestamp: number): string => {
		return new Date(timestamp).toLocaleTimeString();
	};
</script>

{#if isLoading}
	<div class="flex flex-col items-center justify-center p-8 text-gray-600">
		<p class="mb-4">Загрузка датчиков...</p>
		<div class="w-8 h-8 border-4 border-blue-200 border-t-blue-500 rounded-full animate-spin"></div>
	</div>
{:else if error}
	<div class="p-4 text-red-600 bg-red-50 rounded-lg">
		<p>Ошибка: {error}</p>
	</div>
{:else}
	<div class="max-w-7xl mx-3 my-4 px-4 sm:px-6 py-4 space-y-5">
		{#if sensorData?.sensors?.length}
			{#each Object.entries(groupByZone(sensorData.sensors)) as [zone, sensors]}
				<div class="flex-col bg-white rounded-lg shadow">
					<h2
						class="px-4 text-center py-3 bg-gray-50 border-b border-gray-200 text-lg font-medium text-gray-800"
					>
						{zone === 'unknown' ? 'Неизвестная зона' : getZoneDescription(zone)}
					</h2>
					<div class="flex flex-col sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
						{#each sensors as sensor (sensor.address)}
							<SensorCard {sensor} onUpdate={reloadSensors} />
						{/each}
					</div>
				</div>
			{/each}
		{:else}
			<div class="p-4 text-center text-gray-500">
				<p>Нет данных о датчиках</p>
			</div>
		{/if}
	</div>
{/if}
