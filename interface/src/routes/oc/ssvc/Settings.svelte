<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';

	// Подкомпоненты
	import GeneralSettings from '$lib/components/SsvcSettings/GeneralSettings.svelte';
	import ValveBandwidth from '$lib/components/SsvcSettings/ValveBandwidth.svelte';
	import SpeedSettings from '$lib/components/SsvcSettings/SpeedSettings.svelte';
	import ParallelValve from '$lib/components/SsvcSettings/ParallelValve.svelte';
	import ParallelValveLateHeads from '$lib/components/SsvcSettings/ParallelValveLateHeads.svelte';
	import { fetchSettings, updateSetting } from '$lib/api/settingsApi';

	let ssvcSetting = $state<SsvcSettings | null>(null); // Добавляем реактивное состояние
	let editingArray = $state('');

	async function getSvvcSetting(): Promise<void> {
		ssvcSetting = await fetchSettings();
	}

	async function saveChanges(field: string, value: unknown): Promise<void> {
		const success = await updateSetting(field, value);
		if (success) {
			await getSvvcSetting();
		} else {
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
