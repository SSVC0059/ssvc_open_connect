<script lang="ts">
	import '$lib/styles/simple-grid-min.css';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import type { SsvcOpenConnectMessage } from '$lib/types/models';
	import { slide } from 'svelte/transition';
	import RectImg from '$lib/assets/rect.png';
	import TpSensor from '$lib/components/Telemetry/TpSensor.svelte';
	import TimeBlock from '$lib/components/Telemetry/TimeBlock.svelte';
	import ValveParameters from '$lib/components/Telemetry/ValveParameters.svelte';
	import Bottom from '$lib/components/Telemetry/Bottom.svelte';
	import { Popover } from 'flowbite-svelte';
	import Stages from '$lib/components/Telemetry/Stages.svelte';
	import StopsCounter from '$lib/components/Telemetry/StopsCounter.svelte';
	import Alc from '$lib/components/Telemetry/Alc.svelte';
	import PressureCubeSensor from '$lib/components/Telemetry/PressureCubeSensor.svelte';
	import { getStageDescription } from '$lib/utils/ssvcHelper';
	import HeatingMode from '$lib/components/Telemetry/HeatingMode.svelte';
	import Control from '$lib/components/Telemetry/Control.svelte';
	import Pause from '$lib/components/Telemetry/Pause.svelte';
	import ThermalSensors from '$lib/components/Telemetry/ThermalSensors.svelte';

	let data = $state<SsvcOpenConnectMessage | null>(null);

	const telemetry = $derived(data?.telemetry);
	const status = $derived(data?.status);

	let initialLoad = $state(true);
	let error = $state<string | null>(null);
	let rectificationStatus = $state<SsvcOpenConnectMessage | undefined>();
	let currentType: string = $state('');
	let needUpdateStatus: boolean = $state(false);
	let needUpdateRS: boolean = $state(false);

	const controller = new AbortController();

	async function fetchData(withStatus = false) {
		try {
			const url = withStatus ? '/rest/telemetry?mode=status' : '/rest/telemetry';
			const response = await fetch(url, {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json',
					Accept: '*/*'
				},
				signal: controller.signal
			});

			if (!response.ok) throw new Error(`HTTP ${response.status}`);

			const _data: SsvcOpenConnectMessage = await response.json();
			data = {
				// Сохраняем существующие данные только для поля 'status'
				status: withStatus ? _data.status : data?.status,

				// Копируем все данные из ответа
				..._data,

				// Перезаписываем telemetry новыми данными
				telemetry: {
					..._data.telemetry
				}
			};

			// Обновляем currentType если изменился тип
			if (_data.telemetry?.type !== currentType) {
				currentType = _data.telemetry?.type || '';
			}

			error = null;
		} catch (err) {
			if (err.name !== 'AbortError') {
				error = err instanceof Error ? err.message : 'Unknown error';
				console.error('Fetch error:', err);
			}
		} finally {
			if (initialLoad) initialLoad = false;
		}
	}

	$effect(() => {
		let isMounted = true;

		// Первый запрос со статусом
		fetchData(true);
		const interval = setInterval(() => fetchData(false), 1000);

		return () => {
			isMounted = false;
			clearInterval(interval);
			controller.abort();
		};
	});

	// Добавляем метод для явного обновления статуса
	async function updateStatus() {
		needUpdateStatus = true;
	}

	$effect(() => {
		// Безопасная проверка с учётом новой типизации
		const shouldFetch = data?.telemetry?.type && (needUpdateStatus || needUpdateRS);

		if (shouldFetch) {
			fetchData(true);
			updateStatus().finally(() => (needUpdateRS = false));
		}
	});

	function getDescriptionValve(name: string) {
		switch (name) {
			case 'heads':
				return 'Головы';
			case 'late_heads':
				return 'Подголовники';
			case 'hearts':
				return 'Тело';
			case 'tails':
				return 'Хвосты';
		}
	}
</script>

{#if telemetry}
	<!-- Общий контейнер высотой 85% от экрана -->
	<div class="flex flex-col h-[85vh] overflow-hidden text-gray-900 dark:text-gray-100">
		<!-- Верхняя панель -->
		<div class="p-2 h-[10vh]">
			<div
				class="grid grid-cols-2 gap-2 h-full items-center bg-white/40 dark:bg-gray-800/40 backdrop-blur rounded-lg shadow px-4 py-2"
			>
				<TimeBlock name="Старт" timeString={telemetry.start_time || '-'} />
				<TimeBlock name="Конец" timeString={telemetry.end_time || '-'} />
			</div>
		</div>

		<!-- Центральная часть -->
		<div class="px-2 h-[65vh]">
			<!-- grid layout с одинаковой высотой колонок -->
			<div class="grid grid-cols-2 gap-2 h-full">
				<!-- Левая колонка -->
				<div class="flex flex-col h-full min-h-0 space-y-2">
					<!-- Блок с изображением -->
					<div
						class="relative flex-1 bg-white/40 dark:bg-gray-800/40 backdrop-blur rounded-lg shadow p-2 flex items-center justify-center overflow-hidden"
					>
						<img
							alt="Rectification"
							class="w-full h-full max-w-full max-h-full object-contain"
							src={RectImg}
						/>

						{#if telemetry.tp1_target}
							<div
								id="tpTarget"
								class="absolute top-[33%] left-1/2 -translate-x-1/2 translate-y-1/3 bg-white bg-opacity-40"
							>
								<TpSensor name="" temp={telemetry.tp1_target} />
								<Popover
									class="w-64 text-sm font-light text-gray-500 bg-white dark:text-gray-400 dark:border-gray-600 dark:bg-gray-800"
									title="Целевая температура"
									transition={slide}
									triggeredBy="#tpTarget"
								>
									Целевая температура в колоне
								</Popover>
							</div>
						{/if}
						<div
							class="absolute top-12 left-1/4 transform -translate-x-1/2 -translate-y-1/5 bg-white bg-opacity-40 dark:bg-opacity-0"
						>
							<ThermalSensors />
						</div>
						<div
							class="absolute top-1/2 left-1/2 transform -translate-x-1/2 -translate-y-1/5 bg-white bg-opacity-40 dark:bg-opacity-0"
						>
							<TpSensor name="Колонна" temp={telemetry.common.tp1} />
						</div>
						<div class="absolute top-[75%] left-1/2 transform -translate-x-1/2 -translate-y-3">
							<TpSensor name="Куб" temp={telemetry.common.tp2} />
						</div>
						<div class="absolute top-[86%] left-1/2 transform -translate-x-1/2 -translate-y-3">
							{#if telemetry.tank_mmhg}
								<PressureCubeSensor pressure={telemetry.tank_mmhg} />
							{/if}
						</div>
						<div class="absolute top-[93%] left-1/2 transform -translate-x-1/2 -translate-y-3">
							{#if telemetry.alc}
								<Alc alcohol={telemetry.alc}></Alc>
							{/if}
						</div>
						<div class="absolute top-[87%] left-1/2 transform -translate-x-1/2 -translate-y-3">
							<HeatingMode
								heatingOn={telemetry.common.heatingOn}
								overclockingOn={telemetry.common.overclockingOn}
							/>
						</div>
						<div class="absolute top-[74%] left-1/2 transform -translate-x-1/2 -translate-y-3">
							{#if telemetry.stop || (data?.status && data.status.status === 'paused')}
								<Pause />
							{/if}
						</div>
					</div>

					<!-- Компонент управления -->
					<div class="p-2 rounded-lg bg-white/30 dark:bg-gray-800/30 shadow">
						<Control {status} onStatusUpdate={updateStatus} />
					</div>
				</div>

				<!-- Правая колонка -->
				<div class="flex flex-col h-full min-h-0">
					<div
						class="flex flex-col flex-1 min-h-0 divide-y divide-gray-300/40 dark:divide-gray-600/40"
					>
						<!-- Блок с клапанами -->
						{#if telemetry.volume && Object.keys(telemetry.volume).length > 0}
							<div
								class="basis-[60%] min-h-0 overflow-auto p-1 sm:p-4 bg-white/40 dark:bg-gray-800/40 rounded-lg"
							>
								<div class="flex flex-col h-full justify-around space-y-1">
									{#each Object.entries(telemetry.volume) as [name, value]}
										{#if name === telemetry.type}
											<ValveParameters
												n={getDescriptionValve(name)}
												vo={telemetry.valveOpen}
												p={telemetry.period}
												o={telemetry.open}
												f={value ?? 0}
												s={telemetry.volumeSpeed ?? 0}
											/>
										{:else}
											<ValveParameters n={getDescriptionValve(name)} f={value ?? 0} c={true} />
										{/if}
									{/each}
								</div>
							</div>
						{/if}

						<div class="flex-grow"></div>

						<div class="basis-[20%] min-h-0 overflow-hidden p-2 sm:p-4">
							<Stages {data} />
						</div>

						<!-- Счётчик остановок -->
						{#if telemetry.type === 'hearts'}
							<div class="basis-[20%] min-h-0 overflow-hidden p-2 sm:p-4">
								<StopsCounter stops={telemetry.stops} />
							</div>
						{/if}
					</div>
				</div>
			</div>
		</div>

		<!-- Нижний блок -->
		<Bottom {telemetry} {status} {getStageDescription} />
	</div>
{/if}
