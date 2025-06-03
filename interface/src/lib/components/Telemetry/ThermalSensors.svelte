<script lang="ts">
	import { fetchSensorsTemperatureByZone } from '$lib/api/settingsApi';
	import type { SensorData, SensorsData } from '$lib/types/OCSettings';
	import { getZoneComponent } from '$lib/components/OCSettings/OSSettingsHelper';

	let sensorData: SensorsData | undefined = $state();
	let isLoading: boolean = $state(true);
	let error: string | null = $state('');

	const controller = new AbortController();

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
		const interval = setInterval(() => reloadSensors(), 1000);

		return () => {
			clearInterval(interval);
			controller.abort();
		};
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
{:else if sensorData}
	<div class="flex flex-col gap-1 px-1 py-1">
		{#if sensorData?.sensors?.length}
			{#each Object.entries(groupByZone(sensorData.sensors)) as [zone, sensors]}
				<div
					class="inline-block bg-white dark:bg-gray-800 rounded-md border border-gray-200 dark:border-gray-700 overflow-hidden"
				>
					<div class=" flex-row flex-wrap gap-1 p-1">
						<h2
							class="px-1 py-0.5 text-center text-[10px] font-semibold tracking-tight bg-gray-100 dark:bg-gray-700 text-gray-700 dark:text-gray-300 border-b border-gray-200 dark:border-gray-600
							whitespace-nowrap overflow-hidden text-ellipsis"
						>
							<span>
								{#if getZoneComponent(zone)}
									<svelte:component this={getZoneComponent(zone)} class="w-5 h-5" />
								{/if}
							</span>
						</h2>
						{#each sensors as sensor (sensor.address)}
							<div
								class=" text-center px-1.5 py-0.5 text-[10px] bg-gray-100 dark:bg-gray-700 text-gray-700 dark:text-gray-300 rounded"
							>
								{sensor.temperature.toFixed(2)}°C
							</div>
						{/each}
					</div>
				</div>
			{/each}
		{/if}
	</div>
{/if}
