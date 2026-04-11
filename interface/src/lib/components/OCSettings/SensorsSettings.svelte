<svelte:options runes />

<script lang="ts">
	import { fetchAlarmThresholds, fetchSensorsTemperatureByZone } from '$lib/api/ssvcApi';
	import type { AlarmThresholdsState, SensorDetails, TemperatureResponse } from '$lib/types/Sensors';
	import { getZoneDescription } from '$lib/components/OCSettings/OSSettingsHelper';
	import SensorCard from '$lib/components/OCSettings/SensorCard.svelte';

	const { sensorType = 'temperature' } = $props<{ sensorType: 'temperature' | 'pressure' }>();

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
		temperatureResponse
			? Object.entries(temperatureResponse).map(([zone, sensors]) => {
					const expectedType = sensorType === 'temperature' ? 'thermal' : sensorType;
					const filteredSensors = Object.entries(sensors).filter(
						([, sensorData]) => sensorData.type === expectedType
					);
					return [zone, Object.fromEntries(filteredSensors)] as [string, Record<string, SensorDetails>];
				})
			: ([] as [string, Record<string, SensorDetails>][])
	);
	const title =
		sensorType === 'pressure' ? 'Датчики давления' : 'Датчики температуры';
	const description =
		sensorType === 'pressure'
			? 'Настройки порогов и зон для датчиков давления.'
			: '';
</script>

<div class="settings-container">
	<div class="settings-grid">
		<div class="settings-panel">
			<div class="settings-section">
				{#if description}
					<p class="settings-description">
						{description}
					</p>
				{/if}

				{#if isLoading}
					<div class="loading-container flex flex-col items-center gap-2">
						<p class="loading-text">Загрузка датчиков...</p>
						<span
							class="loading loading-spinner loading-lg text-primary"
							aria-hidden="true"
						></span>
					</div>
				{:else if error}
					<div class="error-container">
						<p class="error-text">Ошибка: {error}</p>
					</div>
				{:else}
					<div class="sensors-main-container">
						{#each zoneEntries as [zone, sensors]}
							{#if Object.keys(sensors).length > 0}
								<div class="zone-panel">
									<h2 class="zone-title">
										{zone === 'unknown'
											? 'Неизвестная зона'
											: getZoneDescription(zone)}
									</h2>
									<div class="sensors-grid">
										{#each Object.entries(sensors) as [address, temp] (address)}
											<SensorCard
												sensor={{ address, data: temp, zone }}
												{alarmThresholdsState}
												onUpdate={reloadSensors}
												sensorsType={sensorType}
											/>
										{/each}
									</div>
								</div>
							{/if}
						{:else}
							<div class="empty-state">
								<p class="empty-text">Нет данных о датчиках</p>
							</div>
						{/each}
					</div>
				{/if}
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	@use "$lib/styles/base/variables" as v;
	@use "$lib/styles/base/mixins" as m;

	/* ===== КОНТЕЙНЕР ДЛЯ ВСЕЙ СТРАНИЦЫ СЕНСОРОВ (.sensors-main-container) ===== */
	.sensors-main-container {
		margin: 0 auto;
		display: flex;
		flex-direction: column;
		gap: 1.25rem;
		width: 100%;
	}

	/* ===== ПАНЕЛЬ ЗОНЫ (.zone-panel) — без собственной тени/рамки, только layout ===== */
	.zone-panel {
		border-radius: 1rem;
		overflow: hidden;

		@media (prefers-color-scheme: dark) {
			background-color: var(--dark-surface-color);
		}
	}

	.zone-title {
		font-size: 1.125rem;
		font-weight: 600;
		color: var(--primary-800);
		padding: 0.75rem 1rem;
		margin: 0;
		text-align: center;
		border-bottom: 2px solid var(--primary-200);
		@include m.dark-theme-color;
	}

	@media (prefers-color-scheme: dark) {
		.zone-title {
			color: oklch(var(--bc));
		}
	}

	/* ===== СЕТКА ДЛЯ КАРТОЧЕК СЕНСОРОВ (.sensors-grid) ===== */
	.sensors-grid {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(20rem, 1fr));
		gap: 1.5rem;
		padding: 1rem;

		@media (min-width: v.$breakpoint-sm) {
			gap: 1.5rem;
		}
	}

	/* ===== СОСТОЯНИЯ ЗАГРУЗКИ И ОШИБКИ ===== */

	.loading-container,
	.error-container {
		@include m.flex-center;
		flex-direction: column;
		padding: 2rem;
		text-align: center;
		min-height: 200px;
	}

	.loading-text {
		font-size: 1.125rem;
		color: var(--primary-700);
		@include m.dark-theme-color;
	}

	.error-text {
		font-size: 1.125rem;
		font-weight: 600;
		color: var(--red-600);
	}

	/* Одноколоночная карточка настроек датчиков, в стиле Telegram-вкладки */
	.settings-grid {
		grid-template-columns: 1fr;

		@media (min-width: v.$breakpoint-md) {
			grid-template-columns: 1fr;
		}
	}
</style>
