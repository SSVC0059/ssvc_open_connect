<script lang="ts">
	// import { fetchSensorsTemperatureByZone } from '$lib/api/ssvcApi'; // Не используется
	import type { TemperatureResponse, SensorData } from '$lib/types/OCSettings';
	import { getZoneComponent, getZoneDescription } from '$lib/components/OCSettings/OSSettingsHelper';
	// import { groupByZone } from '$lib/utils/ssvcHelper'; // Больше не нужен

	const { temperatureResponse } = $props<{
		temperatureResponse: TemperatureResponse | undefined;
	}>();

	// 💡 Вспомогательная функция для получения ключей объекта зон,
	function getZoneEntries(response: TemperatureResponse | undefined): [string, SensorData[]][] {
		if (response && response.zones && Object.keys(response.zones).length > 0) {
			// TypeScript теперь знает, что Object.entries(response.zones)
			// возвращает [string, SensorData[]], что соответствует ожидаемому типу.
			return Object.entries(response.zones);
		}
		return [];
	}

	// 💡 Получаем сгруппированные данные для цикла
	const zoneEntries = $derived(getZoneEntries(temperatureResponse));
</script>

<h2 class="title">Датчики DS18B20</h2>
{#if temperatureResponse}
	<div class="sensor-grid">

		{#each zoneEntries as [zone, sensors]}
			<h4 class="section-title flex">
				{#if getZoneComponent(zone)}
					{@const ZoneComp = getZoneComponent(zone)}
					<ZoneComp class="zone-icon" />
				{/if}
				<span>{getZoneDescription(zone)}</span>
			</h4>
			<div class="readings-list">
				{#each sensors as sensor (sensor.address)}
					<div class="reading-item">
						<span class="reading-label">{sensor.address.slice(-4)}</span>
						<span class="reading-value">{sensor.temp.toFixed(2)}°C</span>
					</div>
				{/each}
			</div>
		{:else}
			<p class="no-data">Нет данных датчиков или зон для отображения.</p>
		{/each}
	</div>
{/if}

<style lang="scss">
  @use "$lib/styles/mixins.scss" as *;

  .title {
    font-size: 1.25rem;
    text-align: center;
    color: var(--glass-primary-800);
    margin-bottom: 0.75rem;
    @include dark-theme-color;
  }

  .sensor-grid {
    display: flex;
    flex-direction: column;
    @include parameter-container;

		.flex{
			display: flex;
			justify-content: center;
			gap: 1rem;
		}

  }

  .no-data {
    text-align: center;
    color: var(--glass-secondary-600);
    margin: 1rem 0;
  }
</style>