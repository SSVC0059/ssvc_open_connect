<script lang="ts">
	import { page } from '$app/stores';
	import ThermalSensorSettings from '$lib/components/OCSettings/ThermalSensorSettings.svelte';
	import TelegramSettings from '$lib/components/OCSettings/TelegramSettings.svelte';
	import ProfileManager from '$lib/components/profiles/ProfileManager.svelte';
	import { getSubsystemState, setSubsystemState } from '$lib/api/ssvcApi';
	import type { SubsystemsState } from '$lib/types/ssvc';

	interface Tab {
		id: keyof SubsystemsState | 'profiles'; // Allow 'profiles' as a valid ID
		title: string;
		component: any;
		isStatic?: boolean; // Flag for tabs that don't depend on subsystem state
	}

	let subsystemsState = $state<SubsystemsState>({
		telegram_bot: false,
		thermal: false,
		// другие подсистемы
	});

	let error = $state('');
	let isLoading = $state(true);
	let filteredTabs = $state<Tab[]>([]);

	const availableTabs: Tab[] = [
		{
			id: 'profiles',
			title: 'Профили',
			component: ProfileManager,
			isStatic: true
		},
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

	$effect(() => {
		loadSubsystemState();
	});

	async function loadSubsystemState() {
		try {
			const state = await getSubsystemState();
			if (state) {
				subsystemsState = state;
				// Фильтруем вкладки, оставляя статические и те, которые есть в ответе сервера
				filteredTabs = availableTabs.filter(tab => tab.isStatic || tab.id in state);
			} else {
				// Fallback if state is not returned, show only static tabs
				filteredTabs = availableTabs.filter(tab => tab.isStatic);
			}
		} catch (err) {
			error = err instanceof Error ? err.message : 'Ошибка загрузки';
			filteredTabs = availableTabs.filter(tab => tab.isStatic); // Show static tabs on error
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

	$effect(() => {
		const tabId = $page.url.searchParams.get('tab');
		if (tabId) {
			const index = filteredTabs.findIndex((tab) => tab.id === tabId);
			if (index !== -1) {
				activeTab = index;
			}
		}
	});

	function isSubsystemEnabled(id: string): boolean {
		const tab = availableTabs.find(t => t.id === id);
		if (tab?.isStatic) return true; // Static tabs are always "enabled"

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
		{#if isLoading}
			<p>Загрузка...</p> <!-- Or a spinner component -->
		{:else if error}
			<p style="color: red;">{error}</p>
		{:else}
			{#each filteredTabs as tab, index}
				{#if activeTab === index}
					{@const Component = tab.component}
					<Component
						disabled={!isSubsystemEnabled(tab.id)}
						onToggle={tab.isStatic ? undefined : toggleSubsystemHandler(tab.id as keyof SubsystemsState)}
					/>
				{/if}
			{/each}
		{/if}
	</div>
</div>
