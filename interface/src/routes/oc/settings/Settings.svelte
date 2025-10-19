<script lang="ts">
	import ThermalSensorSettings from '$lib/components/OCSettings/ThermalSensorSettings.svelte';
	import { page } from '$app/state';
	import MenuIcon from '~icons/tabler/menu-2';
	import { fade, fly } from 'svelte/transition';
	import TelegramSettings from '$lib/components/OCSettings/TelegramSettings.svelte';
	import { getSubsystemState, setSubsystemState } from '$lib/api/ssvcApi';
	import type { SubsystemsState } from '$lib/types/ssvc';

	interface Tab {
		id: keyof SubsystemsState;
		title: string;
		component: any;
	}

	let subsystemsState = $state<SubsystemsState>({
		telegram_bot: false,
		thermal: false,
		// другие подсистемы
	});

	let error = $state('');
	let isLoading = $state(true);
	let filteredTabs = $state<Tab[]>([]);

	$effect(() => {
		loadSubsystemState();
	});

	async function loadSubsystemState() {
		try {
			const state = await getSubsystemState();
			if (state) {
				subsystemsState = state;
				if (state) {
					subsystemsState = state;
					// Фильтруем вкладки, оставляя только те, которые есть в ответе сервера
					filteredTabs = availableTabs.filter(tab => tab.id in state);
					$inspect(subsystemsState)
				}
			}
		} catch (err) {
			error = err instanceof Error ? err.message : 'Ошибка загрузки';
		} finally {
			isLoading = false;
		}
	}

	async function toggleSubsystem(subsystem: keyof typeof subsystemsState) {
		try {
			const newState = !subsystemsState[subsystem];
			const success = await setSubsystemState({
				[subsystem]: newState
			});

			if (success) {
				subsystemsState[subsystem] = newState;
				// Перезагружаем состояние после изменения
				await loadSubsystemState();
			}
		} catch (err) {
			error = err instanceof Error ? err.message : 'Ошибка изменения';
		}
	}

	const toggleSubsystemHandler = (subsystem: keyof SubsystemsState) => {
		return async () => {
			try {
				await toggleSubsystem(subsystem);
			} catch (err) {
				console.error(`Error toggling ${subsystem}:`, err);
			}
		};
	};

	let activeTab = $state(0);
	let isMobileMenuOpen = $state(false);

	const availableTabs: Tab[] = [
		{
			id: 'thermal',
			title: 'Термодатчики',
			component: ThermalSensorSettings
		},
		{
			id: 'telegram_bot',
			title: 'Telegram',
			component: TelegramSettings
		}
		// Здесь можно добавить другие вкладки
	];

	// Добавим вспомогательную функцию
	function isSubsystemEnabled(id: string): boolean {
		const key = id as keyof SubsystemsState;
		return subsystemsState[key] ?? false;
	}
</script>



<div class="container">
		<div class="tabs-container">
		<!-- Мобильное меню -->
		<div class="mobile-tabs-header" class:menu-open={isMobileMenuOpen}>
			<button
				class="mobile-menu-toggle"
				onclick={() => isMobileMenuOpen = !isMobileMenuOpen}
			>
				<span class="mobile-menu-header">
					{filteredTabs[activeTab]?.title || 'Меню'}
				</span>
				<svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
					<path d="M7 10l5 5 5-5z"/>
				</svg>
			</button>

			<div class="mobile-tabs-dropdown">
				{#each filteredTabs as tab, index}
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
			</div>
		</div>

		<!-- Десктопное меню -->
		<div class="tabs-nav desktop-only">
			{#each filteredTabs as tab, index}
				<button
					class="tab"
					class:tab-active={activeTab === index}
					onclick={() => (activeTab = index)}
				>

					{tab.title}
				</button>
			{/each}
		</div>

		<!-- Контент -->

		{#each filteredTabs as tab, index}
			{#if activeTab === index}
				{@const Component = tab.component}
				<Component
					disabled={!isSubsystemEnabled(tab.id)}
					onToggle={toggleSubsystemHandler(tab.id)}
				/>
			{/if}
		{/each}
	</div>
</div>
