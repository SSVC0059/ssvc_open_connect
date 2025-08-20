<svelte:options runes />

<script lang="ts">
	import type { SensorData } from '$lib/types/OCSettings';

	import { updateSensorThresholds, updateSensorZone } from '$lib/api/settingsApi';
	import { availableZones } from '$lib/components/OCSettings/OSSettingsHelper';
	import { Spinner } from 'flowbite-svelte';

	let { sensor, onUpdate } = $props();

	let error: string | null = $state('');
	let selectedZone: string = $state(''); // For dropdown selection
	let movingSensor: string | null = $state(null);

	// For threshold editing
	let editingSensor: string | null = $state(null);
	let warningThreshold: number = $state(0);
	let criticalThreshold: number = $state(0);

	function calculateTempPercentage(temp: number): number {
		const maxTemp = 100; // Новый максимум
		return Math.min(Math.max((temp / maxTemp) * 100, 0), 100);
	}

	// Handle moving sensor to new zone
	let selectedZoneForSensor: Record<string, string> = $state({}); // Объект для хранения выбранных зон для каждого датчика

	// Добавьте эту функцию для безопасного доступа к значению
	function getSelectedZone(address: string): string {
		return selectedZoneForSensor[address] || '';
	}

	function setSelectedZone(address: string, value: string) {
		selectedZoneForSensor[address] = value;
	}

	// Обработка перемещения датчика в новую зону
	async function moveSensor(address: string) {
		const zone = selectedZoneForSensor[address];
		if (!zone) return;

		movingSensor = address;
		try {
			const success = await updateSensorZone(address, zone);
			if (success) {
				await new Promise(resolve => setTimeout(resolve, 1500));
				onUpdate?.(); // Вызываем коллбэк после успешного обновления
			}
		} catch (err) {
			if (err instanceof Error) {
				error = err.message;
			} else {
				error = 'Ошибка при перемещении датчика';
			}
		} finally {
			movingSensor = null;
		}
	}

	async function saveThresholds(address: string) {
		try {
			const success = await updateSensorThresholds(address, warningThreshold, criticalThreshold);
			if (success) {
				onUpdate?.(); // Уведомляем родителя
				editingSensor = null;
			}
		} catch (err) {
			error = err instanceof Error ? err.message : 'Ошибка при сохранении порогов';
		}
	}

	function startEditing(sensor: SensorData) {
		editingSensor = sensor.address;
		warningThreshold = sensor.warningThreshold || 0;
		criticalThreshold = sensor.criticalThreshold || 0;
	}

	function handleZoneChange(address: string, event: Event) {
		const select = event.target as HTMLSelectElement;
		if (select) {
			selectedZoneForSensor[address] = select.value;
		}
	}
</script>

{#if sensor}
	<div class="w-full rounded-lg border border-gray-200 p-4 hover:shadow-md transition-shadow">
		<div class=" ">
			<!-- Заголовок с адресом и температурой -->
			<div
				class="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-2 sm:gap-4 p-3 bg-gray-50 rounded-lg"
			>
				<div class="min-w-0 w-full sm:w-auto">
					<p class="truncate flex items-center" title={sensor.address}>
						<span class="text-gray-400 text-base sm:text-xl font-normal mr-1.5">Адрес</span>
						<span class="text-gray-700 font-mono">:</span>
						<span class="ml-1.5 text-gray-800 font-medium italic truncate">{sensor.address}</span>
					</p>
				</div>
				<span
					class="flex-shrink-0 font-bold text-lg sm:text-xl whitespace-nowrap px-3 py-1 rounded-md bg-white shadow-sm border border-gray-200 w-full sm:w-auto text-center sm:text-left"
				>
					<span class="text-gray-600 text-sm font-normal mr-1">Темп.:</span>
					<span class="text-gray-900">{sensor.temperature.toFixed(1)}°C</span>
				</span>
			</div>

			<!-- Temperature bar -->
			<div class="space-y-2">
				<div class="h-2.5 bg-gray-100 rounded-full overflow-hidden relative">
					{#if sensor.warningThreshold}
						<div
							class="absolute top-0 h-full w-0.5 bg-yellow-500"
							style={`left: ${calculateTempPercentage(sensor.warningThreshold)}%`}
							title={`Предупреждение: ${sensor.warningThreshold}°C`}
						></div>
					{/if}
					{#if sensor.criticalThreshold}
						<div
							class="absolute top-0 h-full w-0.5 bg-red-500"
							style={`left: ${calculateTempPercentage(sensor.criticalThreshold)}%`}
							title={`Критично: ${sensor.criticalThreshold}°C`}
						></div>
					{/if}
					<div
						class="h-full bg-gradient-to-r from-blue-400 to-green-400"
						style={`width: ${calculateTempPercentage(sensor.temperature)}%`}
					></div>
				</div>
			</div>

			<!-- Температурные метки -->
			<div class="flex justify-between text-xs text-gray-500 overflow-hidden">
				<span class="whitespace-nowrap">0°C</span>
				<span class="whitespace-nowrap">25°C</span>
				<span class="whitespace-nowrap">50°C</span>
				<span class="whitespace-nowrap">75°C</span>
				<span class="whitespace-nowrap">100°C</span>
			</div>

			<!-- Управляющие элементы -->
			<div class="mt-auto space-y-3">
				{#if editingSensor === sensor.address}
					<div class="space-y-3">
						<div>
							<label class="block text-sm font-medium text-gray-700 mb-1">
								Порог предупреждения (°C)
							</label>
							<input
								type="number"
								bind:value={warningThreshold}
								class="w-full border border-gray-300 rounded-md px-2 py-1 text-sm"
								min="0"
								max="100"
							/>
						</div>
						<div>
							<label class="block text-sm font-medium text-gray-700 mb-1">
								Критический порог (°C)
							</label>
							<input
								type="number"
								bind:value={criticalThreshold}
								class="w-full border border-gray-300 rounded-md px-2 py-1 text-sm"
								min="0"
								max="100"
							/>
						</div>
						<div class="flex flex-col sm:flex-row space-y-2 sm:space-y-0 sm:space-x-2">
							<button
								onclick={() => saveThresholds(sensor.address)}
								class="flex-1 px-3 py-1 bg-green-500 text-white rounded-md text-sm hover:bg-green-600"
							>
								Сохранить
							</button>
							<button
								onclick={() => (editingSensor = null)}
								class="flex-1 px-3 py-1 bg-gray-300 text-gray-700 rounded-md text-sm hover:bg-gray-400"
							>
								Отмена
							</button>
						</div>
					</div>
				{:else}
					<div class="flex flex-col sm:flex-row space-y-2 sm:space-y-0 sm:space-x-2">
						<button
							onclick={() => startEditing(sensor)}
							class="px-3 py-1 bg-blue-500 text-white rounded-md text-sm hover:bg-blue-600 whitespace-nowrap flex-shrink-0"
						>
							Пороги
						</button>
						<div class="flex-1 flex flex-col sm:flex-row gap-2 min-w-0">
							<select
								class="flex-1 border border-gray-300 rounded-md px-2 py-1 text-sm min-w-0"
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
								class="px-3 py-1 bg-purple-500 text-white rounded-md text-sm hover:bg-purple-600 disabled:bg-gray-300 whitespace-nowrap flex-shrink-0"
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
	</div>
{/if}
