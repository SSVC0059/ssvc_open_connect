<script lang="ts">
	import { page } from '$app/stores';
	import SensorsSettings from '$lib/components/OCSettings/SensorsSettings.svelte';
	import TelegramSettings from '$lib/components/OCSettings/TelegramSettings.svelte';
	import ProfileManager from '$lib/components/profiles/ProfileManager.svelte';
	import { goto } from '$app/navigation';
	import { getSubsystemState, setSubsystemState } from '$lib/api/ssvcApi';
	import type { SubsystemsState } from '$lib/types/ssvc';

	interface Tab {
		id: keyof SubsystemsState | 'profiles'; // Allow 'profiles' as a valid ID
		title: string;
		component: any;
		isStatic?: boolean; // Flag for tabs that don't depend on subsystem state
		alwaysShow?: boolean; // Flag for tabs that should always be visible
		props?: Record<string, any>;
	}

	let subsystemsState = $state<SubsystemsState>({
		atm_sensor: false,
		i2c_bus: false,
		settings: false,
		telegram_bot: false,
		thermal: false
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
			title: 'Датчики температуры',
			component: SensorsSettings,
			props: { sensorType: 'thermal' }
		},
		{
			id: 'atm_sensor',
			title: 'Датчики давления',
			component: SensorsSettings,
			props: { sensorType: 'pressure' }
		},
		{
			id: 'telegram_bot',
			title: 'Telegram',
			component: TelegramSettings,
			alwaysShow: true
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
				// Фильтруем вкладки:
				// - isStatic: всегда показываем
				// - alwaysShow: всегда показываем (для настроек типа Telegram)
				// - state[tab.id]: показываем, если подсистема включена (для датчиков)
				filteredTabs = availableTabs.filter(
					(tab) => tab.isStatic || tab.alwaysShow || state[tab.id as keyof SubsystemsState]
				);
			} else {
				// Fallback if state is not returned, show only static tabs
				filteredTabs = availableTabs.filter((tab) => tab.isStatic || tab.alwaysShow);
			}
		} catch (err) {
			error = err instanceof Error ? err.message : 'Ошибка загрузки';
			filteredTabs = availableTabs.filter((tab) => tab.isStatic || tab.alwaysShow); // Show static/alwaysShow tabs on error
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

	$effect(() => {
		const tabId = $page.url.searchParams.get('tab');
		if (tabId) {
			const index = filteredTabs.findIndex((tab) => tab.id === tabId);
			if (index !== -1) {
				activeTab = index;
			}
		}
	});

	function setActiveTab(index: number) {
		activeTab = index;
		const tab = filteredTabs[index];
		if (tab) {
			goto(`/oc/settings?tab=${tab.id}`, { replaceState: true });
		}
	}

	function isSubsystemEnabled(id: string): boolean {
		const tab = availableTabs.find((t) => t.id === id);
		if (tab?.isStatic) return true; // Static tabs are always "enabled"

		const key = id as keyof SubsystemsState;
		return subsystemsState[key] ?? false;
	}
</script>

<div class="container">
	<div class="tabs-container">
		{#if isLoading}
			<div class="loading-container flex flex-col items-center gap-2">
				<p class="loading-text">Загрузка...</p>
				<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
			</div>
		{:else if error}
			<p class="error-text">{error}</p>
		{:else}
			<!-- DaisyUI radio tabs-lift + tab content -->
			<div class="tabs tabs-lift tabs-md w-full" role="tablist">
				{#each filteredTabs as tab, index}
					<!-- Вкладка -->
					<input
						type="radio"
						name="oc_settings_tabs"
						role="tab"
						class="tab flex-1 whitespace-nowrap"
						aria-label={tab.title}
						checked={activeTab === index}
						onchange={() => setActiveTab(index)}
					/>
					<!-- Контент вкладки -->
					<div
						role="tabpanel"
						class="tab-content w-full mt-4"
					>
						{#if activeTab === index}
							{@const Component = tab.component}
							<Component
								{...tab.props}
								disabled={!isSubsystemEnabled(tab.id)}
								onToggle={tab.isStatic ? undefined : toggleSubsystemHandler(tab.id as keyof SubsystemsState)}
							/>
						{/if}
					</div>
				{/each}
			</div>
		{/if}
	</div>
</div>

<style lang="scss">
	@use "$lib/styles/base/variables" as v;
	@use "$lib/styles/base/mixins" as m;

	.error-text {
		color: var(--red-600);
	}

	.tabs-container {
		display: flex;
		flex-direction: column;
	}

	/* Tabs font similar to h3 in profile header */
	:global(.tabs .tab) {
		font-size: 1.125rem;
		font-weight: 600;
	}

	/* Active tab uses primary as background */
	:global(.tabs .tab:checked) {
		background-color: var(--color-primary);
	}
</style>
