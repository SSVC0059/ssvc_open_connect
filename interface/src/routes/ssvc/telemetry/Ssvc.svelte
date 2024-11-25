<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import type { SsvcOpenConnectMessage } from "$lib/types/models";
	import SsvcCommands from '$lib/components/SsvcCommands.svelte';
	import { notifications } from '$lib/components/toasts/notifications';
	import Warning from '~icons/tabler/alert-triangle';
	// error
	import {
		ssvcTelemetryStore,
		ssvcEventStore,
		commandState,
		ssvcCurrentVersionStore
	} from '$lib/stores/ssvcOpenConnect';
	import {
		getDescriptionEvent,
		getStageDescription,
		postVersion,
		isVersionSupported
	} from '$lib/utils/ssvcHelper';

	let ssvcTelemetry: SsvcOpenConnectMessage | null = null;
	let ssvcEvent: string = "";
	let errorCode: number = 0
	let descriptionEvent: string  = "";
	let ssvcVersion: string = "";
	let isWaitingResponse: boolean  = false;
	let waitingCommandMessage: string = "";

	let versionValid: boolean | null = null;
	const minSupportedVersion = '2.2.37';

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
	let unsubscribeSsvcCurrentVersionStore
	unsubscribeSsvcCurrentVersionStore = ssvcCurrentVersionStore.subscribe((value) => {
		ssvcVersion = value;
		if (ssvcVersion) {
			// Проверяем версию только если она существует
			versionValid = isVersionSupported(ssvcVersion, minSupportedVersion);
		} else {
			// Если версия пустая, указываем, что она неизвестна
			versionValid = null;
		}
	});

	onMount(async () => {
		await postVersion();
	});

	onDestroy(() => {
		unsubscribeCommandState()
		unsubscribeSsvcCurrentVersionStore()
		unsubscribeSsvcEventStore()
	})

	$: animationClass = errorCode === 0
			? 'animate-info bg-blue-500 text-white dark:bg-blue-400'
			: errorCode === 1
					? 'animate-critical bg-red-500 text-white dark:bg-red-400'
					: 'animate-warning bg-yellow-300 text-black dark:bg-yellow-500';

</script>
<SettingsCard>
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
	{#if versionValid === null}
		<div class="alert alert-warning">
			<Warning class="h-12 w-10 flex-shrink-0" />
			<div class="flex items-center">
				<p class="text-black font-semibold ml-3">
					Версия SSVC не была загружена. Обновите страницу
				</p>
			</div>
		</div>

		<!-- Состояние несовместимой версии -->
	{:else if !versionValid}
		<div class="alert alert-error">
			<Warning class="h-12 w-10 flex-shrink-0" />
			<div class="flex items-center">
				<p class="text-black font-semibold ml-3">
					Версия SSVC меньше поддерживаемой.
					<br>Текущая:
					<strong>{ssvcVersion}</strong>
					<br>
					Минимально поддерживаемая:
					<strong>{minSupportedVersion}</strong>
				</p>
			</div>
		</div>
		<!-- Состояние совместимой версии -->
	{/if}
	<span slot="title" class="text-2xl font-semibold  text-center" data-svelte-h="svelte-onyccr">Настройки</span>
	{#if ssvcTelemetry}
		<div class="p-6 rounded-lg space-y-6">
			<!-- Режим работы -->
			<div class="flex flex-row flex-wrap justify-between gap-x-2 border-b border-blue-500 dark:border-white/50">
				<h3 class="text-xl font-medium ">Режим работы</h3>
				<p>{ getStageDescription(ssvcTelemetry.type) }</p>
			</div>

			<!-- TP1 Target -->
			{#if ssvcTelemetry.tp1_target}
				<div class="flex flex-row flex-wrap justify-between gap-x-2 border-b border-blue-500 dark:border-white/50">
					<h3 class="text-xl font-medium ">Целевая температура</h3>
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
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Давление</td>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{ssvcTelemetry.common.mmhg}</td>
						</tr>
						<tr>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Температура колонны</td>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{ssvcTelemetry.common.tp1}</td>
						</tr>
						<tr>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Температура куба</td>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{ssvcTelemetry.common.tp2}</td>
						</tr>
						<tr>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Реле</td>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{ssvcTelemetry.common.relay ? 'Включено' : 'Отключено'}</td>
						</tr>
						<tr>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Сигнал</td>
							<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
								{
										ssvcTelemetry.common.signal ? 'Включен' : 'Отключен'
								}
							</td>
						</tr>
						</tbody>
					</table>
				</div>
			{/if}
		</div>
	{/if}

	{#if ssvcTelemetry}
		{#if ssvcTelemetry && ssvcTelemetry.type === "waiting"}
			<div class="component opacity-50 pointer-events-none">
				<SsvcCommands></SsvcCommands>
			</div>
		{:else}
			<div class="component">
				<SsvcCommands></SsvcCommands>
			</div>
		{/if}
	{/if}
<!--	<pre>{JSON.stringify(ssvcTelemetry, undefined, 2)}</pre>-->
</SettingsCard>