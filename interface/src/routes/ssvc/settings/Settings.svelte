<script lang="ts">
	import { slide } from 'svelte/transition';
	import { user } from '$lib/stores/user';
	import { page } from '$app/stores';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import { formatSecondsToHHMMSS } from '$lib/utils/ssvcHelper'

	import type { SsvcOpenConnectMessage, SsvcSettings } from '$lib/types/models';

	let message: SsvcOpenConnectMessage;

	async function getSvvcSetting(): Promise<SsvcSettings | null> {
		try {
			const r = await fetch('/rest/settings', {
				method: 'GET',
				headers: {
					Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json',
					'Accept': '*/*'
				}
			});
			const text = await r.text();
			try {
				message = JSON.parse(text);
				if (message.settings) {
					return message.settings;
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

	let isHeadsSettingsVisible = true;
	let isAdditionalSettingsVisible = true;
	let isParallelSettingsVisible = true;
	let isParallelSettingsVisible1 = true;
	let isParallelSettingsVisible3 = true;
	let isValveBandwidthVisible = true;

</script>

{#await getSvvcSetting() then ssvcSetting}
	{#if ssvcSetting}
		<SettingsCard>
	<span slot="title" class="text-2xl font-semibold text-center">Настройки</span>
	<div class="p-6 rounded-lg space-y-6">
		<button
			type="button"
			class="flex items-center justify-between w-full text-1xl font-semibold text-center cursor-pointer underline"
			on:click={() => (isAdditionalSettingsVisible = !isAdditionalSettingsVisible)}
		>
			<span>Общие настройки</span>
			<span class="ml-2">{isAdditionalSettingsVisible ? "▲" : "▼"}</span>
		</button>

		{#if isAdditionalSettingsVisible}
			<div transition:slide>
				<div class="overflow-x-auto">
					<table class="table-auto w-full border-collapse border border-gray-700 dark:border-gray-600">
						<thead>
						<tr class="bg-blue-500 text-white dark:bg-gray-800">
							<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Параметр</th>
							<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Значение</th>
						</tr>
						</thead>
						<tbody>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Гистерезис при отборе тела</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.hyst} °С</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Декремент при отборе тела</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.decrement}  %</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Время отбора голов</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{formatSecondsToHHMMSS(ssvcSetting.heads_timer)}</td>
						</tr>
						{#if ssvcSetting.late_heads_timer}
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Время отбора подголовников</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{formatSecondsToHHMMSS(ssvcSetting.late_heads_timer)}</td>
							</tr>
						{/if}
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Таймер фиксации температуры отбора тела</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{formatSecondsToHHMMSS(ssvcSetting.hearts_timer)}</td>
						</tr>
						{#if ssvcSetting.tail_temp}
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Температура завершения отбора хвостов</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.tail_temp} °С</td>
							</tr>
						{/if}
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Звук</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.sound ? 'Включен' : 'Отключен'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Барометр</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.pressure ? 'Включен' : 'Отключен'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Реле инвертировано</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.relay_inverted ? 'Да' : 'Нет'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Автовключение реле</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.relay_autostart ? 'Да' : 'Нет'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Автоматический переход между этапами</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.auto_mode ? 'Да' : 'Нет'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Продолжать прерванный процесс</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.autoresume ? 'Да' : 'Нет'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Температура ТД2 завершения отбора тела</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.hearts_finish_temp} °С</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Отложенный пуск</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.start_delay} с</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Фильтр ТД</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.tp_filter}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Подсветка</td>
							<div class="text-center">
								<p>
									{#if ssvcSetting.backlight === 'active'}
										Включена
									{:else if ssvcSetting.backlight === 'always'}
										Всегда включено
									{:else if ssvcSetting.backlight === 'off'}
										Отключена
									{:else}
										Статус не определен
									{/if}
								</p>
							</div>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Сдвиг температуры отбора тела на 0.07°С</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.hearts_temp_shift ? 'Включен' : 'Отключен'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Пауза "на себя"</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.hearts_pause ? 'Включен' : 'Отключен'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Формула</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.formula ? 'Включен' : 'Отключен'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Температура начала формулы</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.formula_start_temp} °С</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Поправка давления в кубе</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.tank_mmhg} мм рт.ст.</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Поправка ТД2</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.tp2_shift} °С</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Контроль ТД1 для выхода SIGNAL</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.signal_tp1_control ? 'Включен' : 'Отключен'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Инвертировать SIGNAL</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.signal_inverted ? 'Включен' : 'Отключен'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Целевая температура на ТД1 при контроле ТД1 для выхода SIGNAL</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.tp1_control_temp} °С</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Старт по ТД1 при контроле ТД1 для выхода SIGNAL.</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.tp1_control_start ? 'Включен' : 'Отключен'}</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Лимит стабилизации</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.tp1_control_start === 0 ? 'Выключен' : ssvcSetting.tp1_control_start} сек</td>
						</tr>
						<tr>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Завершить этап при превышении лимита стабилизации</td>
							<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.tp1_control_start ? 'Да' : 'Нет'}</td>
						</tr>
						</tbody>
					</table>
				</div>
			</div>
		{/if}

		<button
			type="button"
			class="flex items-center justify-between w-full text-1xl font-semibold text-center cursor-pointer underline"
			on:click={() => (isValveBandwidthVisible = !isValveBandwidthVisible)}
		>
			<span>Пропускная способность клапанов</span>
			<span class="ml-2">{isValveBandwidthVisible ? "▲" : "▼"}</span>
		</button>

		{#if isValveBandwidthVisible}
			<div transition:slide>
				<div class="overflow-x-auto">
					<table class="table-auto w-full border-collapse border border-gray-700 dark:border-gray-600">
						<thead>
						<tr class="bg-blue-500 text-white dark:bg-gray-800">
							<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Тип клапана</th>
							<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Пропускная способность</th>
						</tr>
						</thead>
						<tbody>
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Головы</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.valve_bw[0]} мл/ч</td>
							</tr>
							{#if ssvcSetting.late_heads}
								<tr>
									<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Подголовники</td>
									<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.valve_bw[2]} мл/ч</td>
								</tr>
							{/if}
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Тело</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.valve_bw[1]} мл/ч</td>
							</tr>
						{#if !ssvcSetting.late_heads}
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Хвосты</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.valve_bw[2]} мл/ч</td>
							</tr>
						{/if}
						</tbody>
					</table>
				</div>
			</div>
		{/if}

		<button
			type="button"
			class="flex items-center justify-between w-full text-1xl font-semibold text-center cursor-pointer underline"
			on:click={() => (isHeadsSettingsVisible = !isHeadsSettingsVisible)}
		>
			<span>Настройки скорости отбора</span>
			<span class="ml-2">{isHeadsSettingsVisible ? "▲" : "▼"}</span>
		</button>

		{#if isHeadsSettingsVisible}
			<div transition:slide>
				<div class="overflow-x-auto">
					<div class="overflow-x-auto">
						<div class="overflow-x-auto">
							<table class="table-auto w-full border-collapse border border-gray-700 dark:border-gray-600">
							  <caption class="text-lg font-semibold mb-4 text-center">
								Параметры работы клапанов
							  </caption>
							  <thead>
								<tr class="bg-blue-500 text-white dark:bg-gray-800">
								  <th class="border  sm:text-sm border-gray-700 dark:border-gray-600 py-2 text-center w-1/3">
									Тип клапана
								  </th>
								  <th class="border border-gray-700 dark:border-gray-600py-2 text-center w-1/3">
									Время открытия клапана
								  </th>
								  <th class="border border-gray-700 dark:border-gray-600 py-2 text-center w-1/3">
									Периодичность
								  </th>
								</tr>
							  </thead>
							  <tbody>
								<tr>
								  <td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">
									Головы
								  </td>
								  <td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">
									{ssvcSetting.heads[0]} сек
								  </td>
								  <td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">
									{ssvcSetting.heads[1]} сек
								  </td>
								</tr>
								{#if ssvcSetting.late_heads}
								  <tr>
									<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">
									  Подголовники
									</td>
									<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">
									  {ssvcSetting.late_heads[0]} сек
									</td>
									<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">
									  {ssvcSetting.late_heads[1]} сек
									</td>
								  </tr>
								{/if}
								<tr>
								  <td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">
									Тело
								  </td>
								  <td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">
									{ssvcSetting.hearts[0]} сек
								  </td>
								  <td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">
									{ssvcSetting.hearts[1]} сек
								  </td>
								</tr>
								{#if ssvcSetting.tails}
								  <tr>
									<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">
									  Хвосты
									</td>
									<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">
									  {ssvcSetting.tails[0]} сек
									</td>
									<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">
									  {ssvcSetting.tails[1]} сек
									</td>
								  </tr>
								{/if}
							  </tbody>
							</table>
						  </div>
					  </div>					  
				</div>
			</div>
		{/if}
		
		{#if ssvcSetting.parallel_v1}
			<button
				type="button"
				class="flex items-center justify-between w-full text-1xl font-semibold text-center cursor-pointer underline"
				on:click={() => (isParallelSettingsVisible1 = !isParallelSettingsVisible1)}
			>
				<span>Скорость параллельного отбора клапана 1 (головы)</span>
				<span class="ml-2">{isParallelSettingsVisible1 ? "▲" : "▼"}</span>
			</button>

			{#if isParallelSettingsVisible1}
				<div transition:slide>
					<div class="overflow-x-auto">
						<table class="table-auto w-full border-collapse border border-gray-700 dark:border-gray-600">
							<thead>
							<tr class="bg-blue-500 text-white dark:bg-gray-800">
								<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Параметр</th>
								<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Значение</th>
							</tr>
							</thead>
							<tbody>
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Время открытого клапана</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.parallel_v1[0]} сек</td>
							</tr>
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Период, с</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.parallel_v1[1]} сек</td>
							</tr>
							</tbody>
						</table>
					</div>
				</div>
			{/if}
		{/if}

		{#if ssvcSetting.parallel}
			<button
				type="button"
				class="flex items-center justify-between w-full text-1xl font-semibold text-center cursor-pointer underline"
				on:click={() => (isParallelSettingsVisible = !isParallelSettingsVisible)}
			>
				<span>Скорость параллельного отбора клапана 3 (подголовники)</span>
				<span class="ml-2">{isParallelSettingsVisible ? "▲" : "▼"}</span>
			</button>

			{#if isParallelSettingsVisible}
				<div transition:slide>
					<div class="overflow-x-auto">
						<table class="table-auto w-full border-collapse border border-gray-700 dark:border-gray-600">
							<thead>
							<tr class="bg-blue-500 text-white dark:bg-gray-800">
								<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Параметр</th>
								<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Значение</th>
							</tr>
							</thead>
							<tbody>
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Время открытого клапана</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.parallel[0]} сек</td>
							</tr>
							<tr>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Период, с</td>
								<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{ssvcSetting.parallel[1]} сек</td>
							</tr>
							</tbody>
						</table>
					</div>
				</div>
			{/if}
		{/if}

		{#if ssvcSetting.parallel_v3}
			<button
				type="button"
				class="flex items-center justify-between w-full text-1xl font-semibold text-center cursor-pointer underline"
				on:click={() => (isParallelSettingsVisible3 = !isParallelSettingsVisible3)}
			>
				<span>Скорость параллельного отбора клапана 3</span>
				<span class="ml-2">{isParallelSettingsVisible3 ? "▲" : "▼"}</span>
			</button>
			{#if isParallelSettingsVisible3}
				<div transition:slide>
					<div class="space-y-6">
						<h3 class="text-lg font-semibold  text-center">Параллельный отбор клапаном №3 (Хвосты)</h3>
						{#if ssvcSetting.parallel_v3.length > 0}
						<div class="overflow-x-auto ">
							<table class="table-auto w-full border-collapse border border-gray-700 dark:border-gray-600">
								<thead>
									<tr class="bg-blue-500 text-white dark:bg-gray-800">
										<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Температура диапазона</th>
										<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Время открытого клапана</th>
										<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Период</th>
									</tr>
								</thead>
								<tbody>
								{#each ssvcSetting.parallel_v3 as row}
									<tr>
										<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">{row[0]} °С</td>
										<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{row[1]} сек</td>
										<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">{row[2]} сек</td>
									</tr>
								{/each}
								</tbody>
							</table>
						</div>	
						{:else}
							<p class="text-center text-gray-500 dark:text-gray-400"></p>
						{/if}
					</div>
				</div>
			{/if}
		{/if}


	</div>
</SettingsCard>

	{:else}
		<p>Data not available</p>
	{/if}
{:catch error}
	<p>Error: {error.message}</p>
{/await}

