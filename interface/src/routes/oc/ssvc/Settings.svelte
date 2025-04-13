<script lang="ts">
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import type { SsvcSettings } from '$lib/types/models';

	// Подкомпоненты
	import GeneralSettings from '$lib/components/Settings/GeneralSettings.svelte';
	import ValveBandwidth from '$lib/components/Settings/ValveBandwidth.svelte';
	import SpeedSettings from '$lib/components/Settings/SpeedSettings.svelte';
	import ParallelValve from '$lib/components/Settings/ParallelValve.svelte';
	import ParallelValveLateHeads from '$lib/components/Settings/ParallelValveLateHeads.svelte';

	let ssvcSetting = $state<SsvcSettings | null>(null); // Добавляем реактивное состояние
	let editingArray = $state('');

	async function getSvvcSetting(): Promise<void> {
		try {
			const r = await fetch('/rest/settings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json',
					Accept: '*/*'
				}
			});
			const text = await r.text();
			try {
				let message = JSON.parse(text);
				console.log(message);
				ssvcSetting = message?.settings || null; // Обновляем реактивную переменную
			} catch (parseError) {
				console.error('JSON Parsing Error:', parseError);
				ssvcSetting = null; // Обновляем реактивную переменную
			}
		} catch (error) {
			console.error('Error:', error);
			ssvcSetting = null;
		}
	}

	async function saveChanges(field: string, value: any) {
		try {
			// Форматируем value для URL
			const formattedValue = Array.isArray(value)
				? `[${value.join(',')}]` // Массив → [1,2,3]
				: encodeURIComponent(value); // Строка → экранируем спецсимволы

			const url = `/rest/settings?${field}=${formattedValue}`;
			console.log('Отправка запроса:', url);

			const response = await fetch(url, {
				method: 'PUT',
				headers: {
					'Content-Type': 'application/json',
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
				}
			});

			if (response.ok) {
				await getSvvcSetting(); // Обновляем данные после успеха
			} else {
				throw new Error(`HTTP error: ${response.status}`);
			}
		} catch (error) {
			console.error('Ошибка сохранения:', error);
			alert('Не удалось сохранить изменения');
		}
	}

	function handleArrayUpdate(field: string, newValues: number[]) {
		saveChanges(field, newValues);
		editingArray = '';
	}

	$effect(() => {
		getSvvcSetting();
	});

	interface Tab {
		id: string;
		title: string;
		component: any; // Можно уточнить тип компонента
		props: Record<string, unknown>;
	}

	let activeTab = $state(0);
	let availableTabs = $state<Tab[]>([]);

	$effect(() => {
		if (ssvcSetting) {
			// Формируем динамический список вкладок на основе доступных данных
			const tabs = [
				{
					id: 'general',
					title: 'Общие',
					component: GeneralSettings,
					props: { settings: ssvcSetting, onSave: saveChanges }
				},
				{
					id: 'valve-bandwidth',
					title: 'Клапаны',
					component: ValveBandwidth,
					props: { settings: ssvcSetting, onSave: saveChanges }
				},
				{
					id: 'speed',
					title: 'Скорость',
					component: SpeedSettings,
					props: { settings: ssvcSetting, onSave: handleArrayUpdate }
				}
			];

			// Добавляем условные вкладки
			if (ssvcSetting.parallel) {
				tabs.push({
					id: 'parallel',
					title: 'Подголовники',
					component: ParallelValve,
					props: { settings: ssvcSetting, onSave: saveChanges }
				});
			}

			// Добавляем условные вкладки
			if (ssvcSetting.parallel_v1) {
				tabs.push({
					id: 'parallel-v1',
					title: 'Параллельный 1',
					component: ParallelValve,
					props: { settings: ssvcSetting, onSave: saveChanges }
				});
			}

			if (ssvcSetting.parallel_v3) {
				tabs.push({
					id: 'parallel-v3',
					title: 'Параллельный 3',
					component: ParallelValveLateHeads,
					props: { settings: ssvcSetting, onSave: saveChanges }
				});
			}

			availableTabs = tabs;

			// Сбрасываем активную вкладку если текущая больше не доступна
			if (activeTab >= tabs.length) {
				activeTab = 0;
			}
		}
	});
</script>

{#if ssvcSetting}
	<div class="p-2">
		<!-- Навигация по вкладкам -->
		<div class="flex flex-nowrap overflow-x-auto pb-1 -mb-px hide-scrollbar">
			{#each availableTabs as tab, index}
				<button
					class={`shrink-0 px-3 py-1.5 text-xs font-medium border-b-2 transition-colors
                    sm:text-sm sm:px-4 sm:py-2
                    ${
											activeTab === index
												? 'border-blue-500 text-blue-600 dark:text-blue-400 dark:border-blue-400'
												: 'border-transparent hover:text-gray-600 dark:hover:text-gray-300 text-gray-500 dark:text-gray-400'
										}`}
					onclick={() => (activeTab = index)}
				>
					{tab.title}
				</button>
			{/each}
		</div>

		<!-- Контент вкладок -->
		<div
			class="min-h-[500px] bg-white dark:bg-gray-800 rounded-lg shadow-sm p-4 border border-gray-200 dark:border-gray-700"
		>
			{#each availableTabs as tab, index}
				{#if activeTab === index}
					{@const Component = tab.component}
					<Component {...tab.props} />
				{/if}
			{/each}
		</div>
	</div>
{/if}

<style>
	/* Скрываем scrollbar для Webkit-браузеров */
	.hide-scrollbar::-webkit-scrollbar {
		display: none;
	}

	/* Скрываем scrollbar для Firefox */
	.hide-scrollbar {
		scrollbar-width: none;
	}
</style>
