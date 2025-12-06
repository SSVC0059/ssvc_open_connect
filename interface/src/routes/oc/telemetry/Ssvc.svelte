<script lang="ts">
		import RectImg from '$lib/components/Telemetry/RectImg.svelte';
		import type { SsvcOpenConnectMessage } from '$lib/types/ssvc.ts';
        import {
            fetchAlarmThresholds,
            fetchSensorsTemperatureByZone,
            fetchTelemetry
        } from '$lib/api/ssvcApi';
		import Control from '$lib/components/Telemetry/Control.svelte';
		import ValveParameters from '$lib/components/Telemetry/ValveParameters.svelte';
		import { getDescriptionStage, getStageDescription } from '$lib/utils/ssvcHelper';
		import ThermalSensors from '$lib/components/Telemetry/ThermalSensors.svelte';
        import type {AlarmThresholdsState, TemperatureResponse} from '$lib/types/Sensors';
		import ApiVersionGuard from '$lib/components/ApiVersionGuard.svelte';

		let data = $state<SsvcOpenConnectMessage | null>();
		let temperatureResponse = $state<TemperatureResponse | null>();
        let alarmThresholds = $state<AlarmThresholdsState | null>();

		let telemetry = $derived(data?.telemetry)
		let status = $derived(data?.status)

		// параметры опроса
		const TELEMETRY_INTERVAL = 2000;
		const STATUS_INTERVAL = 3000;
		const BASE_INTERVAL = 1000; // Базовый интервал
		let TEMPERATURE_REQUEST_INTERVAL = 3000;

		let telemetryCounter = 0;
		let statusCounter = 0;

        const loadAlarmThresholds = async () => {
            try {
                alarmThresholds = await fetchAlarmThresholds();
            } catch (err) {
                if (err instanceof Error) {
                }
            }
        };

		// Функция для перезагрузки данных
		const reloadSensors = async () => {
			try {
				temperatureResponse = await fetchSensorsTemperatureByZone();
			} catch (err) {
				if (err instanceof Error) {
				}
			}
		};

		async function loadTelemetry(): Promise<void> {
			try {
				const shouldGetTelemetry = telemetryCounter % (TELEMETRY_INTERVAL / BASE_INTERVAL) === 0;
				const shouldGetStatus = statusCounter % (STATUS_INTERVAL / BASE_INTERVAL) === 0;

				let _data;
				if (shouldGetTelemetry && shouldGetStatus) {
					// Если совпали оба интервала - делаем оба запроса
					_data = await fetchTelemetry();
				} else if (shouldGetTelemetry) {
					_data = await fetchTelemetry();
				}

				if (_data) {
					// Собираем новый объект: берём предыдущие поля (если есть), затем накладываем _data.
					// Это позволяет сохранять status, если _data не содержит status.
					const base = { ...(data ?? {}) };
					const merged = {
						...base,
						..._data,
						telemetry: {
							...(_data.telemetry ?? base.telemetry ?? {})
						}
					};

					// Если в ответе нет status — сохраняем старый статус (если был)
					if (_data.status === undefined && base.status !== undefined) {
						merged.status = base.status;
					}

					data = merged;
				}

				telemetryCounter++;
				statusCounter++;
			} catch (err: any) {

			}
		}

		$effect(() => {

			loadTelemetry();
			reloadSensors()
            loadAlarmThresholds();

			const telemetryInt = setInterval(() => loadTelemetry(), BASE_INTERVAL);
			const thermal_sensors = setInterval(() => reloadSensors(), TEMPERATURE_REQUEST_INTERVAL);

			return () => {
				clearInterval(telemetryInt);
				clearInterval(thermal_sensors);
			};
		});

</script>

<div class="telemetry-container">
	<div class="status-bar">
		<div class="status-left">
			<span class="status-item">
				<span class="label">Этап:</span> {getStageDescription(telemetry? telemetry.type : "")}
			</span>
		</div>
		<ApiVersionGuard requiredVersion={1.7}
										 message="Версия API SSVC не поддерживается. Обновите прошивку SSVC на актуальную"></ApiVersionGuard>
		<div class="status-right">
			<span class="status-item">
				<span class="label">T1:</span> {telemetry?.common.tp1}°C
			</span>
			<span class="status-item">
				<span class="label">T2:</span> {telemetry?.common.tp2}°C
			</span>
            {#if telemetry?.tank_mmhg}
                <span class="status-item">
                    <span class="label">Давление:</span> {telemetry?.tank_mmhg} ммРс
                </span>
            {/if}
		</div>
	</div>

	<!-- Main Content -->
	<main class="main-content">
		<div class="responsive-grid">
			<!-- Left Sidebar - Controls -->
			<div class="sidebar-left">
				<div class="glassmorphism panel">
					<div class="controls-container">
						<Control {status} />
						<div class="sensor-readings ">
							<h3 class="section-title">Данные датчиков</h3>
							{#if telemetry}
								<h5 class="section-title">SSVC</h5>
								<div class="readings-list">
									<div class="reading-item">
										<span class="reading-label">Колонна</span>
										<span class="reading-value">{telemetry.common.tp1}°C</span>
									</div>
									<div class="reading-item">
										<span class="reading-label">Куб</span>
										<span class="reading-value">{telemetry.common.tp2}°C</span>
									</div>
									{#if telemetry.tank_mmhg}
										<div class="reading-item">
											<span class="reading-label">Атмосферное давление:</span>
											<span class="reading-value">{telemetry.tank_mmhg} ммРс</span>
										</div>
									{/if}
								</div>
							{/if}
							{#if temperatureResponse}
								<ThermalSensors temperatureResponse={temperatureResponse} alarmThresholds={alarmThresholds}/>
							{/if}
						</div>
					</div>
				</div>
			</div>
			<!-- Center Panel - Column Schematic -->
			<div class="center-panel">
				<div class="glassmorphism panel">
					<h2 class="panel-title">{getStageDescription(telemetry? telemetry.type : "")}</h2>
						{#if telemetry && temperatureResponse}
							<div class="svg-container">
								<RectImg telemetry={telemetry} temperatureResponse={temperatureResponse} alarmThresholds={alarmThresholds}/>
							</div>
						{/if}
				</div>
			</div>

			<!-- Right Sidebar - Process Management -->
			<div class="sidebar-right">
				<div class="glassmorphism panel">
					<div class="stage-controls">
						{#if telemetry && telemetry.volume && Object.keys(telemetry.volume).length > 0}
							{#each Object.entries(telemetry.volume) as [name, value]}
								{#if value !== 0}
									<div class="valves-block">
										<ValveParameters
											n={getDescriptionStage(name) ?? ""}
											vo={name === telemetry.type ? telemetry.valveOpen : 0}
											o={name === telemetry.type ? telemetry.open : 0}
											p={name === telemetry.type ? telemetry.period : 0}
											s={name === telemetry.type ? telemetry.volumeSpeed ?? 0 : 0}
											f={value ?? 0}
											active={name === telemetry.type}
										/>
									</div>
								{/if}
							{/each}
						{/if}
					</div>
					<div class="parameters-readings parameters-bottom">
						<h3 class="section-title">Параметры работы</h3>
						{#if telemetry}
							<div class="readings-list">
								<div class="reading-item">
									<span class="reading-label">Количество стопов</span>
									<span class="reading-value">{telemetry.stops}</span>
								</div>
								{#if telemetry && telemetry.start_time}
									<span class="reading-item">
										<span class="reading-label">Время старта:</span> <span class="reading-value">{telemetry.start_time}</span>
									</span>
								{/if}
								{#if telemetry && telemetry.countdown}
									<span class="reading-item">
										<span class="reading-label">Время до завершения этапа:</span> <span class="reading-value">{telemetry.countdown}</span>
									</span>
								{/if}
								{#if telemetry && telemetry.time}
									<span class="reading-item">
										<span class="reading-label">Общее время:</span> <span class="reading-value">{telemetry.time}</span>
									</span>
								{/if}
								{#if telemetry && telemetry.alc}
									<span class="reading-item">
										<span class="reading-label">Количество спирта в кубе:</span> <span class="reading-value">{telemetry.alc}%</span>
									</span>
								{/if}
							</div>
						{/if}
					</div>
				</div>
			</div>
		</div>
	</main>
</div>

<style lang="scss">
	@use "$lib/styles/base/mixins" as *;

  // Base Styles
  .telemetry-container {
    .status-bar {
      max-width: 100%;
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      gap: 1rem; // Add gap for spacing
      padding: 0.5rem;

      .status-left,
      .status-right {
        display: flex;
        align-items: center;
        gap: 1rem;
        flex-wrap: wrap;
      }

      // Make ApiVersionGuard more subtle
      :global(.version-guard .unsupported-banner) {
        padding: 0.25rem 0.5rem;
        margin-bottom: 0;
        font-size: 0.8rem;
        border-radius: var(--border-radius);
      }

      .status-item {
        .label {
          font-weight: 700;
					color: var(--primary-800);
          @include dark-theme-color;
        }
        .time-value {
          font-weight: 700;
        }
      }

      // On mobile, stack everything and center it
      @media (max-width: 768px) {
        flex-direction: column;
        justify-content: center;
        gap: 0.5rem;

        .status-left, .status-right {
          justify-content: center;
        }
      }
    }

    .main-content {
      max-width: 100%;
      margin-left: auto;
      margin-right: auto;
      padding: 1rem 1rem;


      .responsive-grid {
        display: flex;
        flex-direction: column;
        gap: 1rem;

        .sidebar-right {
          display: flex;
          flex-direction: column;

          @media (min-width: 1024px) {
            grid-column: span 3;
          }

          .glassmorphism.panel {
            display: flex;
            flex-direction: column;
            justify-content: space-between;

            .stage-controls {
              flex: 1;

              .valves-block {
                @include glassmorphism;
                border-radius: var(--border-radius);

                &:not(:last-child) {
                  border-bottom: 1px solid var(--glass-primary-200);
                  margin-bottom: 1rem;
                  padding-bottom: 1.5rem;
                }
              }
            }

            .parameters-readings {
              @include parameter-container;
              margin-top: auto; // Гарантированно прижимает к низу
            }
          }
        }

        @media (min-width: 1024px) {
          flex-direction: row;

          .sidebar-left {
            flex: 0 0 25%; // 25% ширины, не растягивается
            min-width: 250px;
          }

          .center-panel {
            flex: 1; // Занимает оставшееся пространство
            min-width: 0; // Важно для корректного сжатия
          }

          .sidebar-right {
            flex: 0 0 25%; // 25% ширины
            min-width: 250px;
          }
        }
      }
		}

    .glassmorphism {
      @include glassmorphism;
      padding: 1rem 1rem;
      height: 81vh;
      box-sizing: border-box;;

      &.panel {
        margin-bottom: auto;

        .panel-title {
          font-size: 2rem;
          font-weight: 700;
          color: var(--primary-800);
          margin-bottom: 1rem;
          text-align: center;
          @include dark-theme-color;
        }
      }

      .controls-container {
        display: flex;
        flex-direction: column;
        gap: 1.5rem;

        .sensor-readings {
          @include parameter-container;
        }
      }

      .parameters-readings {
        @include parameter-container;
        margin-top: auto; // Гарантированно прижимает к низу
      }

    }
  }
</style>