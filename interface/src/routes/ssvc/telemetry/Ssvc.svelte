<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { writable } from 'svelte/store';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import type { SsvcOpenConnectMessage } from "$lib/types/models";
	import SsvcCommands from '$lib/components/SsvcCommands.svelte';
	import { notifications } from '$lib/components/toasts/notifications';
	import Warning from '~icons/tabler/alert-triangle';
	import RectificationProcess from '$lib/components/RectificationProcess.svelte';
	import SsvcMetrics from '$lib/components/SsvcMetrics.svelte';

	// error
	import {
		ssvcTelemetryStore,
		ssvcEventStore,
		commandState,
	} from '$lib/stores/ssvcOpenConnect';
	import {
		getDescriptionEvent,
		getStageDescription,
	} from '$lib/utils/ssvcHelper';


	// Хранение состояния текущей активной вкладки
	const activeTab = writable('commonData'); // по умолчанию открыта вкладка "settings"


	let ssvcTelemetry: SsvcOpenConnectMessage | null = null;
	let ssvcEvent: string = "";
	let errorCode: number = 0
	let descriptionEvent: string  = "";
	let isWaitingResponse: boolean  = false;
	let waitingCommandMessage: string = "";

	ssvcTelemetryStore.subscribe(value => ssvcTelemetry = value);

	let unsubscribeSsvcEventStore
	unsubscribeSsvcEventStore = ssvcEventStore.subscribe((value) => {
		if (value !== null && value !== "") {
			ssvcEvent = value
			const result =  getDescriptionEvent(value)
			descriptionEvent = result.description
			errorCode = result.errorCode
		}else {
			ssvcEvent = ""
			descriptionEvent = ""
			errorCode = 0
		}

	});

	let unsubscribeCommandState
	unsubscribeCommandState = commandState.subscribe((value) => {
		isWaitingResponse = value.isWaiting
		waitingCommandMessage = value.message
		if (isWaitingResponse) {
			notifications.info("Ожидание ответа от SSVC...", 5000);
		}
		if (waitingCommandMessage !== "") {
			notifications.warning($commandState.message, 5000); // Уведомление с сообщением, если оно есть
		}
	});

	onMount(async () => {
	});

	onDestroy(() => {
		unsubscribeCommandState()
		unsubscribeSsvcEventStore()
	})


	$: animationClass = errorCode === 0
			? 'animate-info bg-blue-500 text-white dark:bg-blue-400'
			: errorCode === 1
					? 'animate-critical bg-red-500 text-white dark:bg-red-400'
					: 'animate-warning bg-yellow-300 text-black dark:bg-yellow-500';

	// Функция для переключения вкладок
	function setActiveTab(tab: string) {
		activeTab.set(tab);
	}

</script>

<SettingsCard>

	<div class="flex space-x-2 border-b border-gray-300 pb-2">
		<!-- Кнопки для переключения вкладок -->
		<button
			on:click={() => setActiveTab('commonData')}
			class="px-4 py-2 rounded-lg text-sm font-medium transition-colors
			   {$activeTab === 'commonData'
			       ? 'bg-blue-500 text-white'
			       : 'bg-gray-200 text-gray-700 hover:bg-gray-300'}"
		>
			Общие данные
		</button>
		<button
			on:click={() => setActiveTab('rectification')}
			class="px-4 py-2 rounded-lg text-sm font-medium transition-colors
			   {$activeTab === 'rectification'
			       ? 'bg-blue-500 text-white'
			       : 'bg-gray-200 text-gray-700 hover:bg-gray-300'}"
		>
			Ректификация
		</button>
		<button
			on:click={() => setActiveTab('metrics')}
			class="px-4 py-2 rounded-lg text-sm font-medium transition-colors
			   {$activeTab === 'metrics'
			       ? 'bg-blue-500 text-white'
			       : 'bg-gray-200 text-gray-700 hover:bg-gray-300'}"
		>
			Метрики
		</button>
	</div>

	{#if ssvcEvent}
			<div
				class={`alert p-4 border rounded-md font-bold text-center ${animationClass}`}
		>
			<h3 class="text-lg">
				{errorCode === 0 ? 'Информация: ' : errorCode === 1 ? 'Ошибка: ' : 'Внимание: '}
			</h3>
			<p>{descriptionEvent}</p>
		</div>
	{/if}

	<span slot="title" class="text-2xl font-semibold  text-center" data-svelte-h="svelte-onyccr">Настройки</span>
	{#if ssvcTelemetry}
		{#if ssvcTelemetry.info !== "" }
			<div class="alert alert-warning">
				<Warning class="h-12 w-10 flex-shrink-0" />
				<div class="flex items-center">
					<p class="text-black font-semibold ml-3">
						<!--					TODO-->
						{ssvcTelemetry.info}
					</p>
				</div>
			</div>
		{/if}
		{#if ssvcTelemetry.uartCommunicationError }
			<div class="alert alert-warning">
				<Warning class="h-12 w-10 flex-shrink-0" />
				<div class="flex items-center">
					<p class="text-black font-semibold ml-3">
						Проблема связи UART. Перезагрузите SSVC
					</p>
				</div>
			</div>
		{/if}
		<div class="p-6 rounded-lg space-y-6">
			<!-- Режим работы -->
			<div class="flex flex-row flex-wrap justify-between gap-x-2 border-b border-blue-500 dark:border-white/50">
				<h3 class="text-xl font-medium ">Режим работы</h3>
				<p>{ getStageDescription(ssvcTelemetry.type) }</p>
			</div>
			{#if $activeTab === 'metrics'}
				<SsvcMetrics />
			{/if}
			{#if $activeTab === 'rectification'}
				{#if ssvcTelemetry.rectificationStart}
					<div class="space-y-2 ">
						<h3 class="text-xl font-medium text-center">
							Данные ректификации
							{#if ssvcTelemetry.stop}
								<dif>
									Отбор остановлен. Отклонение температуры: {(ssvcTelemetry.common?.tp1 - ssvcTelemetry.tp1_target).toFixed (2)} С
								</dif>
							{/if}
						</h3>

						{#if ssvcTelemetry.rectificationStart}
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Время старта</td>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{ssvcTelemetry.rectificationStart}</td>
						{/if}
						{#if ssvcTelemetry.rectificationEnd}
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Время окончания</td>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{ssvcTelemetry.rectificationEnd}</td>
						{/if}
						<RectificationProcess
							telemetryData={{
							open: ssvcTelemetry.open ?? null,
							period: ssvcTelemetry.period ?? null,
							valveBandwidthHeads: ssvcTelemetry.valveBandwidthHeads ?? null,
							valveBandwidthHearts: ssvcTelemetry.valveBandwidthHearts ?? null,
							valveBandwidthTails: ssvcTelemetry.valveBandwidthTails ?? null
						}}
						/>
					</div>
				{/if}
			{/if}

			{#if $activeTab === 'commonData'}

				<!-- TP1 Target -->
				{#if ssvcTelemetry.tp1_target}
					<div class="flex flex-row flex-wrap justify-between gap-x-2 border-b border-blue-500 dark:border-white/50">
						<h3 class="text-xl font-medium ">Температура стабилизации колонны</h3>
						<p>{ssvcTelemetry.tp1_target}</p>
					</div>
				{/if}

				<!-- Common Data -->
				{#if ssvcTelemetry.common}
					<div class="space-y-2 ">
						<h3 class="text-xl font-medium text-center">Общие данные</h3>
						<table class="min-w-full text-left text-sm ">
							<thead>
							<tr>
								<th class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Параметр</th>
								<th class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Значение</th>
							</tr>
							</thead>
							<tbody>
							<tr>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Атмосферное давление</td>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{ssvcTelemetry.common.mmhg} мм рт. ст.</td>
							</tr>
							<tr>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Температура колонны</td>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
									{
											(ssvcTelemetry.tp1_sap ? ssvcTelemetry.tp1_sap : ssvcTelemetry.common.tp1).toFixed(2)
									} °С
								</td>
							</tr>
							<tr>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Температура куба</td>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
									{
											(ssvcTelemetry.tp2_sap ? ssvcTelemetry.tp2_sap : ssvcTelemetry.common.tp2).toFixed(2)
									} °С
								</td>
							</tr>
							<tr>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Реле</td>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{ssvcTelemetry.common.relay ? 'Замкнуто' : 'Разомкнуто'}</td>
							</tr>
							<tr>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Сигнал</td>
								<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
									{
											ssvcTelemetry.common.signal ? 'Замкнут' : 'Разомкнут'
									}
								</td>
							</tr>
							</tbody>
						</table>
					</div>
				{/if}
			{/if}
		</div>
		{#if ssvcTelemetry && ssvcTelemetry.type === "waiting"}
			<div class="component opacity-50 pointer-events-none">
				<SsvcCommands></SsvcCommands>
			</div>
		{:else}
			<div class="component">
				<SsvcCommands></SsvcCommands>
			</div>
		{/if}

	{:else}
		<div
			class="alert p-4 border rounded-md font-bold text-current animate-critical bg-red-500 text-white dark:bg-red-400 flex justify-center"
		>
			Данные не поступают. Перезагрузите SSVC
		</div>
	{/if}

<!--	<pre>{JSON.stringify(ssvcTelemetry, undefined, 2)}</pre>-->
</SettingsCard>

