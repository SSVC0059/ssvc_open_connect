<script lang="ts">
    // Импорты типов из Sensors.ts
    import type {TemperatureResponse, SensorReading, AlarmThresholdsState, ThresholdSettings} from '$lib/types/Sensors';
    // Импорты вспомогательных функций
    import { getZoneComponent, getZoneDescription } from '$lib/components/OCSettings/OSSettingsHelper';

    // 💡 Входящие свойства компонента
    const { temperatureResponse, alarmThresholds } = $props<{
        temperatureResponse: TemperatureResponse | null;
        alarmThresholds?: AlarmThresholdsState | null;
    }>();

    type ZoneEntries = Array<[string, SensorReading[]]>;

    // 💡 Вспомогательная функция для получения ключей объекта зон,
    function getZoneEntries(response: TemperatureResponse | null): ZoneEntries {
        if (!response) {
            return [];
        }

        return Object.entries(response).map(([zoneName, sensorMap]) => {
            // Преобразуем SensorMap в массив SensorReading[]
            const sensorsArray: SensorReading[] = Object.entries(sensorMap).map(([address, temp]) => ({
                address: address,
                temp: temp
            }));


            return [zoneName, sensorsArray];
        }) as ZoneEntries;
    }

    // 💡 Получаем сгруппированные данные для цикла (Derived State)
    const zoneEntries = $derived(getZoneEntries(temperatureResponse));

    // 💡 Преобразуем thresholds в Map для быстрого доступа по адресу датчика
    const sensorThresholdsMap = $derived((): Map<string, ThresholdSettings> => {
        if (!alarmThresholds) return new Map<string, ThresholdSettings>();
        // TypeScript теперь знает, что entries в thresholds — это пары [string, ThresholdSettings]
        return new Map(Object.entries(alarmThresholds.thresholds));
    });

    // 💡 Функция для определения класса статуса
    function getStatusClass(temp: number, thresholds: ThresholdSettings | undefined): string {
        // Проверяем, существует ли порог и включен ли он
        if (!thresholds || !thresholds.enabled) return '';

        // Проверка критического порога
        if (temp >= thresholds.critical) return 'status-critical';
        // Проверка опасного порога
        if (temp >= thresholds.dangerous) return 'status-dangerous';
        // Проверка минимального порога
        if (temp <= thresholds.min) return 'status-min';

        return ''; // Норма
    }
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
                    {@const thresholds = sensorThresholdsMap().get(sensor.address)}

                    {@const statusClass = getStatusClass(sensor.temp, thresholds)}

                    <div class="reading-item">
                        <span class="reading-label">{sensor.address.slice(-4)}</span>
                        <span class="reading-value {statusClass}">
                            {sensor.temp.toFixed(2)}°C
                        </span>
                        {#if thresholds && thresholds.enabled}

                            {@const minClass = sensor.temp <= thresholds.min ? 'threshold-active-min' : ''}
                            {@const dangerousClass = sensor.temp >= thresholds.dangerous && sensor.temp < thresholds.critical ? 'threshold-active-dangerous' : ''}
                            {@const criticalClass = sensor.temp >= thresholds.critical ? 'threshold-active-critical' : ''}

                            <div class="threshold-metadata">
                                Мин: <span class="{minClass}">{thresholds.min}°C</span> |
                                Опас: <span class="{dangerousClass}">{thresholds.dangerous}°C</span> |
                                Крит: <span class="{criticalClass}">{thresholds.critical}°C</span>
                            </div>
                        {/if}
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

  .readings-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    padding: 0.5rem 1rem;
    .reading-item {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      padding: 0.25rem 0;
      border-bottom: 1px dashed var(--glass-secondary-300);

    .reading-label {
      font-size: 0.9rem;
      color: var(--glass-secondary-700);
    }

      .reading-value {
        font-size: 1.1rem;
        font-weight: 500;
        transition: color 0.3s, background-color 0.3s;
      }

      .threshold-metadata {
        font-size: 0.75rem;
        color: var(--glass-secondary-500);
        flex-basis: 100%; // Занимает всю ширину под температурой
        text-align: right;
        margin-top: -0.25rem;

      span {
        font-weight: 400;
        padding: 0 2px;
        border-radius: 2px;
      }
      }

      // ИСПРАВЛЕНИЕ: Удалено !important
      .threshold-active-min {
        color: var(--color-blue-400, #4C9AFF);
        font-weight: 600;
      }

      .threshold-active-dangerous {
        color: var(--color-orange-500, #FF9900);
        font-weight: 700;
        background-color: var(--color-orange-500-light, rgba(255, 153, 0, 0.15));
      }

      .threshold-active-critical {
        color: var(--color-red-600, #D93F36);
        font-weight: 800;
        background-color: var(--color-red-600-light, rgba(217, 63, 54, 0.25));
      }
    }
  }

  // 💡 Стилизация для индикации порогов
  .status-min {
    color: var(--color-blue-400, #4C9AFF);
    font-weight: 600;
  }

  .status-dangerous {
    color: var(--color-orange-500, #FF9900);
    font-weight: 700;
    background-color: var(--color-orange-500-light, rgba(255, 153, 0, 0.15));
    padding: 0 4px;
    border-radius: 4px;
  }

  .status-critical {
    color: var(--color-red-600, #D93F36);
    font-weight: 800;
    animation: pulse-red 1s infinite alternate;
    padding: 0 4px;
    border-radius: 4px;
    background-color: var(--color-red-600-light, rgba(217, 63, 54, 0.25));
  }


  @keyframes pulse-red {
    from { box-shadow: 0 0 5px var(--color-red-600, #D93F36);
    }
    to { box-shadow: 0 0 8px var(--color-red-600, #D93F36), 0 0 12px var(--color-red-600-dark, #A02B25); }
  }
</style>