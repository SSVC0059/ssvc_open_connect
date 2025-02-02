<script lang="ts">
	import '$lib/styles/simple-grid-min.css';
	import { onDestroy } from 'svelte';

	import { user } from '$lib/stores/user';
	import { page } from '$app/stores';
	import RectImg from '$lib/assets/rect.png';
	import {
			ssvcTelemetryStore,
			rectificationStatusStore,
			needUpdateRectificationStatusStore
		} from '$lib/stores/ssvcOpenConnect';
	import type {
		RectificationStatus,
		SsvcOpenConnectMessage
	} from '$lib/types/models';
	import { slide } from 'svelte/transition';
	import { Popover } from 'flowbite-svelte';
	import TimeBlock from '$lib/components/Telemetry/TimeBlock.svelte';
	import TpSensors from '$lib/components/Telemetry/TpSensors.svelte';
	import StopsCounter from '$lib/components/Telemetry/StopsCounter.svelte';
	import PressureCubeSensor from '$lib/components/Telemetry/PressureCubeSensor.svelte';
	import HeatingMode from '$lib/components/Telemetry/HeatingMode.svelte';
	import ValveParameters from '$lib/components/Telemetry/ValveParameters.svelte';
	import Stages from '$lib/components/Telemetry/Stages.svelte';
	import Bottom from '$lib/components/Telemetry/Bottom.svelte';
	import Pause from '$lib/components/Telemetry/Pause.svelte';
	import Control from "$lib/components/Telemetry/Control.svelte";

	let needUpdateStatus: boolean = false;
	let currentType: string = "";
	let ssvcMessage: SsvcOpenConnectMessage;
	let rectificationStatus: RectificationStatus;
	let needUpdateRS: boolean = false;

	const unsubscribeSsvc = ssvcTelemetryStore.subscribe((value) => {
		if (value) {
			ssvcMessage = value;
			if (currentType != ssvcMessage.type) {
				currentType = ssvcMessage.type;
				needUpdateStatus = true;
			}else {
				needUpdateStatus = false;
			}
		}
	});

	const unsubscribeNeedUpdate =  needUpdateRectificationStatusStore.subscribe((value) => {
		if (value) {
			needUpdateRS = value;
		}
	});

	onDestroy(() => {
		unsubscribeSsvc();
		unsubscribeNeedUpdate();
	});

	$: {
		if ((ssvcMessage?.type && needUpdateStatus) || needUpdateRS) {
			(async () => {
				const status = await fetchRectificationStatus();
				rectificationStatusStore.set(status);
				needUpdateRectificationStatusStore.set(false)
			})();
		}
	}

	// Функция для выполнения GET-запроса
	async function  fetchRectificationStatus(): Promise<RectificationStatus | null > {
		console.log('fetchRectificationStatus');
		try {
			const r = await fetch('/rest/status?type=ssvc', {
				method: 'GET',
				headers: {
					Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json',
					'Accept': '*/*'
				}
			});
			const text = await r.text();
			try {
				rectificationStatus = JSON.parse(text);
				if (rectificationStatus) {
					return rectificationStatus;
				}else
					return null;
			} catch (parseError) {
				console.error('JSON Parsing Error:', parseError);
				return null;
			}
		} catch (error) {
			console.error('Error:', error);
			return null;
		}
	}

	function getDescriptionValve(name: string) {
		switch (name) {
			case 'heads':
				return "Головы"
			case 'late_heads':
				return "Подголовники"
			case 'hearts':
				return "Тело"
			case 'tails':
				return 'Хвосты'
		}
	}

</script>
<!--{JSON.stringify(rectificationStatus, null, 2)}-->
<!--{JSON.stringify(ssvcMessage, null, 2)}-->
{#if ssvcMessage}

	<div class="container">
	<!--	Шапка-->
	<div>
		<div class="row flex mt-2">
			<div class="col ">
				<div class="item bg-gray-100 border border-black rounded-lg shadow-sm  mx-1">
					<TimeBlock name="Время старта" timeString="{ssvcMessage.start_time? ssvcMessage.start_time : '-'}"/>
				</div>
			</div>
			<div class="col">
				<div class="item bg-gray-100 border border-black rounded-lg shadow-sm  mx-1">
					<TimeBlock name="Время окончания" timeString="{ssvcMessage.end_time? ssvcMessage.end_time : '-'}"/>
				</div>
			</div>
		</div>
	</div>
	<!--	Разделитель-->
	<div class="row flex justify-center">
		<div class="my-1"></div>
	</div>
	<!--	Остновная часть-->
	<div >
		<div class="row bg-gray-100">
			<div class="col-6 flex justify-center mx-1 item ">
				<div class=" relative ">
					<img src={RectImg} alt="Rect" class="object-contain max-w-full h-auto" />
					<!-- Надпись на изображении -->
					{#if ssvcMessage.tp1_target}
						<div id="tpTarget" class="absolute top-1/3 left-1/2 transform -translate-x-1/2 -translate-y-1/3 bg-white bg-opacity-40">
							<TpSensors name ="" temp={ssvcMessage.tp1_target} />
							<Popover  class="w-64 text-sm font-light text-gray-500 bg-white dark:text-gray-400 dark:border-gray-600 dark:bg-gray-800"
												title="Целевая температура" transition={slide} triggeredBy="#tpTarget" >
								Целевая температура в колоне
							</Popover>
						</div>
					{/if}
					<div class="absolute top-1/2 left-1/2 transform -translate-x-1/2 -translate-y-1/5 bg-white bg-opacity-40">
						<TpSensors name="Колонна" temp={ssvcMessage.common.tp1} />
					</div>
					<div class="absolute top-3/4 left-1/2 transform -translate-x-1/2  -translate-y-3">
						<TpSensors name="Куб" temp={ssvcMessage.common.tp2} />
						{#if ssvcMessage.tank_mmhg}
							<PressureCubeSensor pressute="{ssvcMessage.tank_mmhg}" />
						{/if}
					</div>
					<div class="absolute top-3/4 left-1/2 transform -translate-x-1/2  -translate-y-3">
						{#if ssvcMessage.stop || rectificationStatus?.response.status === "paused"}
							<Pause/>
						{/if}
					</div>
					<div class="absolute top-3/4 left-1/2 transform -translate-x-1/2  translate-y-14">
						<HeatingMode overclockingOn={ssvcMessage.common.overclockingOn} heatingOn={ssvcMessage.common.heatingOn} />
					</div>
				</div>
			</div>
			<!--		Правая колонка-->
			<div class="col">
				{#if ssvcMessage.volume && Object.keys(ssvcMessage.volume).length > 0}
					<div class="item border border-black rounded-lg shadow-sm my-1">
						{#if ssvcMessage.volume}
							{#each Object.entries(ssvcMessage.volume) as [name, value]}
								{#if name === ssvcMessage.type}
									<ValveParameters
										n={getDescriptionValve(name)}
										vo={ssvcMessage.valveOpen}
										p={ssvcMessage.period}
										o={ssvcMessage.open}
										f={value? value : 0}
										s={ssvcMessage.volumeSpeed? ssvcMessage.volumeSpeed : 0}
									/>
								{:else}
									<ValveParameters
										n={getDescriptionValve(name)}
										f={value ? value : 0}
										c={true}
									/>
								{/if}
							{/each}
						{/if}
					</div>
				{/if}
				<div class="absolute my-2 items-end">
					<Stages/>
					{#if ssvcMessage.stops}
						<div class="item ">
							<StopsCounter stops="{ssvcMessage.stops}"/>
						</div>
					{/if}
				</div>
			</div>
		</div>
	</div>
	<!--	Разделитель-->

	<div class="row">
		<div class="col">
			<div class="flex flex-row flex-wrap justify-center items-center gap-2">
				<Control/>
			</div>
		</div>
	</div>
	<div class="row">
		<div class="col flex justify-center bg-gray-100">
			<Bottom type="{ssvcMessage.type}"
			event="{ssvcMessage.info ? ssvcMessage.info : ''}"/>
		</div>
	</div>
	</div>
{/if}


<style>
    @media (max-width: 768px) {
        .item {
            font-size: 12px;
            padding: 8px;
        }

        img {
            width: 100%; /* Scale image for smaller screens */
            height: auto;
        }
    }
    div {
        font-family: 'Arial', sans-serif;
    }

</style>
