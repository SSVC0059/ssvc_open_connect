<script lang="ts">

	import type { SsvcSettings } from '$lib/types/ssvc';
	import { fetchSettings, updateSetting } from '$lib/api/ssvcApi';
	import { notifications } from '$lib/components/toasts/notifications';
	// Подкомпоненты
	import GeneralSettings from '$lib/components/SsvcSettings/GeneralSettings.svelte';
	import ValveBandwidth from '$lib/components/SsvcSettings/ValveBandwidth.svelte';
	import SpeedSettings from '$lib/components/SsvcSettings/SpeedSettings.svelte';
	import ParallelValve from '$lib/components/SsvcSettings/ParallelValve.svelte';
	import Reload from '~icons/tabler/reload';

	let ssvcSettings = $state<SsvcSettings | null>();
	let activeTab = $state(0);
	let isMobileMenuOpen = $state(false);
	function closeMobileMenu() {
		isMobileMenuOpen = false;
	}

	const loadSsvcSettings = async () => {
		try {
			ssvcSettings = await fetchSettings();
		} catch (err) {
			if (err instanceof Error) {
			}
		}
	};

	async function saveChanges(field: string, value: any) {
		let result = await updateSetting(field, value);
		if (result) {
			notifications.success('Настройка сохранена', 5000);
			await loadSsvcSettings();
		} else {
			notifications.error('Ошибка сохранения настроек', 5000);
		}
	}

	// Определяем тип для вкладки
	interface Tab {
		id: string;
		title: string;
		component: any;
		// Можно уточнить тип компонента при необходимости
		props: Record<string, unknown>;
	}

	// Реактивное вычисление вкладок
	const availableTabs = $derived(computeTabs(ssvcSettings));
	// Функция для вычисления вкладок
	function computeTabs(settings: SsvcSettings | null | undefined): Tab[] {
		if (!settings) return [];
		return [
			{
				id: 'general',
				title: 'Общие',
				component: GeneralSettings,
				props: { settings, onSave: saveChanges }
			},
			{
				id: 'valve-bandwidth',
				title: 'Клапаны',
				component: ValveBandwidth,
				props: { settings, onSave: saveChanges }
			},
			{
				id: 'speed',
				title: 'Параметры отбора',
				component: SpeedSettings,
				props: { settings, onSave: saveChanges }
			},
			{
				id: 'parallel',
				title: 'Параллельный отбор',
				component: ParallelValve,
				props: { settings, onSave: saveChanges }
			}
		];
	}

	$effect(() => {
		loadSsvcSettings();
	});

	$effect(() => {
		if (activeTab >= availableTabs.length && availableTabs.length > 0) {
			activeTab = 0;
		}
	});
</script>

<div class="container">
	<div class="tabs-container">
		<div class="mobile-tabs-header" class:menu-open={isMobileMenuOpen}>
			<button class="mobile-menu-toggle" onclick={() => isMobileMenuOpen = !isMobileMenuOpen}>
				<span class="mobile-menu-header">{availableTabs[activeTab]?.title ||
				'Меню'}</span>
				<svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
					<path d="M7 10l5 5 5-5z"/>
				</svg>
			</button>

			<div class="tabs-nav-wrapper mobile-tabs-dropdown">
				{#each availableTabs as tab, index}
					<button
						class="mobile-tab {activeTab === index ? 'mobile-tab-active' : ''}"
						onclick={() => {
															activeTab = index;
															isMobileMenuOpen = false;
													}}
					>
						{tab.title}
					</button>
				{/each}
				<button
					class="refresh-button mobile-menu-item"
					onclick={() => {
						loadSsvcSettings();
						isMobileMenuOpen = false;
					}}
				>
					<Reload />
					<span>Перезагрузить настройки</span>
				</button>
			</div>
		</div>

		<nav class="tabs-nav desktop-only">
			<div class="tabs-nav-content">
				{#each availableTabs as tab, index}
					<button
						class="tab {activeTab === index ? 'tab-active text-active' : ''}"
						onclick={() => (activeTab = index)}
					>
						{tab.title}
					</button>
				{/each}
			</div>

			<button class="refresh-button" onclick={loadSsvcSettings} title="Перезагрузить настройки">
				<Reload />
			</button>
		</nav>

		{#each availableTabs as tab, index}
			{#if activeTab === index}
				{@const Component = tab.component}
				<Component {...tab.props} />
			{/if}
		{/each}
	</div>
</div>