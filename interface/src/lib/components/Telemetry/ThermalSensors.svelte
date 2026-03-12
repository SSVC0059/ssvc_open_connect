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

    type ExtendedSensorReading = {
        address: string;
        v: number;
        u: string;
        type: string;
    };

    type ZoneEntries = Array<[string, SensorReading[]]>;

    // 💡 Вспомогательная функция для получения ключей объекта зон,
    function getZoneEntries(response: TemperatureResponse | null): ZoneEntries {
        if (!response) return [];

        // Object.entries(response) дает нам [имя_зоны, SensorMap]
        return Object.entries(response).map(([zoneName, sensorMap]) => {
            // Преобразуем карту адресов в массив объектов SensorReading
            const sensorsArray: SensorReading[] = Object.entries(sensorMap).map(([address, details]) => ({
                address: address,
                data: {
                    v: details.v,
                    u: details.u,
                    type: details.type
                }
            }));

            return [zoneName, sensorsArray];
        });
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

<h2 class="title">Датчики</h2>
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

                    {@const statusClass = getStatusClass(sensor.data.v, thresholds)}

                    <div class="reading-item">
                        <span class="reading-label">{sensor.address.slice(-4)}</span>

                        <span class="reading-value {statusClass}">
                            {sensor.data.v.toFixed(2)}{sensor.data.u}
                        </span>

                        {#if thresholds && thresholds.enabled}
                            {@const minClass = sensor.data.v <= thresholds.min ? 'threshold-active-min' : ''}
                            {@const dangerousClass = sensor.data.v >= thresholds.dangerous && sensor.data.v < thresholds.critical ? 'threshold-active-dangerous' : ''}
                            {@const criticalClass = sensor.data.v >= thresholds.critical ? 'threshold-active-critical' : ''}

                            <div class="threshold-metadata">
                                Мин: <span class="{minClass}">{thresholds.min}{sensor.data.u}</span> |
                                Опас: <span class="{dangerousClass}">{thresholds.dangerous}{sensor.data.u}</span> |
                                Крит: <span class="{criticalClass}">{thresholds.critical}{sensor.data.u}</span>
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
  @use "$lib/styles/base/mixins" as *;

  .title {
    font-size: 1.25rem;
    text-align: center;
    color: var(--glass-primary-800);
    margin-bottom: 0.75rem;
    @include dark-theme-color;
  }

  /* В ThermalSensors.svelte */
  .sensor-grid {
      @include parameter-container;
      display: flex;
      flex-direction: column;
      gap: 1rem;

      flex: 1;            // Растягиваемся на всё место от .sensor-readings
      overflow-y: auto;   // ВКЛЮЧАЕМ СКРОЛЛ
      padding-right: 6px; // Отступ для полосы прокрутки

      /* Стилизация скроллбара (чтобы не был уродливым) */
      &::-webkit-scrollbar {
          width: 4px;
      }
      &::-webkit-scrollbar-thumb {
          background: var(--primary-500);
          border-radius: 10px;
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
        color: var(--blue-400);
        font-weight: 600;
      }

      .threshold-active-dangerous {
        color: var(--orange-500);
        font-weight: 700;
        background-color: var(--orange-500-light);
      }

      .threshold-active-critical {
        color: var(--red-600);
        font-weight: 800;
        background-color: var(--red-600-light);
      }
    }
  }

  // 💡 Стилизация для индикации порогов
  .status-min {
    color: var(--blue-400);
    font-weight: 600;
  }

  .status-dangerous {
    color: var(--orange-500);
    font-weight: 700;
    background-color: var(--orange-500-light);
    padding: 0 4px;
    border-radius: 4px;
  }

  .status-critical {
    color: var(--red-600);
    font-weight: 800;
    animation: pulse-red 1s infinite alternate;
    padding: 0 4px;
    border-radius: 4px;
    background-color: var(--red-600-light);
  }


  @keyframes pulse-red {
    from { box-shadow: 0 0 5px var(--red-600);
    }
    to { box-shadow: 0 0 8px var(--red-600), 0 0 12px var(--red-600-dark); }
  }
</style>