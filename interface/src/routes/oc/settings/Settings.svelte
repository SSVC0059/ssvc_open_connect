<script lang="ts">
	import { page } from '$app/stores';
	import { browser } from '$app/environment';
	import { tick } from 'svelte';
	import SensorsSettings from '$lib/components/OCSettings/SensorsSettings.svelte';
	import MessengerSettings from '$lib/components/OCSettings/MessengerSettings.svelte';
	import HardwareConfigTab from '$lib/components/OCSettings/HardwareConfigTab.svelte';
	import ProfileManager from '$lib/components/profiles/ProfileManager.svelte';
	import { goto } from '$app/navigation';
	import { fetchHardwareConfig, getSubsystemState, setSubsystemState } from '$lib/api/ssvcApi';
	import type { SubsystemsState } from '$lib/types/ssvc';
	import SettingsMobileScroll from '$lib/components/settings-layout/SettingsMobileScroll.svelte';

	const MOBILE_MQ = '(max-width: 767px)';

	type SettingsTabId = keyof SubsystemsState | 'profiles' | 'hardware' | 'messengers';

	interface Tab {
		id: SettingsTabId;
		title: string;
		component: any;
		isStatic?: boolean;
		alwaysShow?: boolean;
		noSubsystemToggle?: boolean;
		props?: Record<string, unknown>;
	}

	let subsystemsState = $state<SubsystemsState>({
		atm_sensor: false,
		i2c_bus: false,
		settings: false,
		telegram_bot: false,
		vk_bot: false,
		thermal: false
	});

	let error = $state('');
	let isLoading = $state(true);
	let filteredTabs = $state<Tab[]>([]);
	let activeTab = $state(0);

	async function loadAll() {
		error = '';
		try {
			const [state, hw] = await Promise.all([getSubsystemState(), fetchHardwareConfig()]);
			if (state) {
				subsystemsState = {
					atm_sensor: false,
					i2c_bus: false,
					settings: false,
					telegram_bot: false,
					vk_bot: false,
					thermal: false,
					...state
				};
			}

			filteredTabs = availableTabs.filter((tab) => {
				if (tab.id === 'hardware' || tab.id === 'profiles') return true;
				if (tab.id === 'atm_sensor') return hw?.pressureSensorEnabled === true;
				if (tab.alwaysShow) return true;
				if (!state) return false;
				return Boolean(state[tab.id as keyof SubsystemsState]);
			});
		} catch (err) {
			error = err instanceof Error ? err.message : 'Ошибка загрузки';
			filteredTabs = availableTabs.filter(
				(tab) => tab.id === 'hardware' || tab.id === 'profiles' || tab.alwaysShow
			);
		} finally {
			isLoading = false;
		}
	}

	const availableTabs: Tab[] = [
		{
			id: 'profiles',
			title: 'Профили',
			component: ProfileManager,
			isStatic: true
		},
		{
			id: 'hardware',
			title: 'Оборудование',
			component: HardwareConfigTab,
			alwaysShow: true,
			noSubsystemToggle: true,
			props: { onSaved: loadAll }
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
			noSubsystemToggle: true,
			props: { sensorType: 'pressure' }
		},
		{
			id: 'messengers',
			title: 'Мессенджеры',
			component: MessengerSettings,
			alwaysShow: true,
			noSubsystemToggle: true
		}
	];

	$effect(() => {
		void loadAll();
	});

	async function toggleSubsystem(subsystem: keyof typeof subsystemsState) {
		try {
			const newState = !subsystemsState[subsystem];
			const success = await setSubsystemState({
				[subsystem]: newState
			});

			if (success) {
				subsystemsState[subsystem] = newState;
				await loadAll();
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

	const tabFromUrl = $derived($page.url.searchParams.get('tab'));

	function isSubsystemEnabled(id: string): boolean {
		if (id === 'hardware' || id === 'profiles' || id === 'messengers') return true;

		const key = id as keyof SubsystemsState;
		return subsystemsState[key] ?? false;
	}

	function tabSupportsSubsystemToggle(tab: Tab): boolean {
		return !tab.isStatic && tab.id !== 'hardware' && !tab.noSubsystemToggle;
	}

	const mobileItems = $derived(
		filteredTabs.map((tab) => {
			if (tab.id === 'messengers') {
				return {
					id: String(tab.id),
					title: tab.title,
					component: tab.component,
					props: {
						subsystemsState,
						onToggleTelegram: toggleSubsystemHandler('telegram_bot'),
						onToggleVk: toggleSubsystemHandler('vk_bot')
					}
				};
			}
			return {
				id: String(tab.id),
				title: tab.title,
				component: tab.component,
				props: {
					...tab.props,
					disabled: !isSubsystemEnabled(tab.id),
					onToggle: tabSupportsSubsystemToggle(tab)
						? toggleSubsystemHandler(tab.id as keyof SubsystemsState)
						: undefined
				}
			};
		})
	);

	$effect(() => {
		const tabId = tabFromUrl;
		if (filteredTabs.length === 0) return;
		if (tabId === 'telegram_bot') {
			goto('/oc/settings?tab=messengers', { replaceState: true });
			return;
		}
		if (tabId) {
			const index = filteredTabs.findIndex((t) => String(t.id) === tabId);
			if (index !== -1) {
				activeTab = index;
				return;
			}
			goto('/oc/settings', { replaceState: true });
			return;
		}
		if (activeTab >= filteredTabs.length) {
			activeTab = 0;
		}
	});

	$effect(() => {
		const tabId = tabFromUrl;
		if (!browser || !tabId || isLoading || error) return;
		tick().then(() => {
			try {
				if (!window.matchMedia(MOBILE_MQ).matches) return;
			} catch {
				return;
			}
			const el = document.getElementById(`oc-settings-section-${tabId}`);
			if (el && typeof el.scrollIntoView === 'function') {
				el.scrollIntoView({ block: 'start', behavior: 'instant' });
			}
		});
	});

	function selectTab(index: number) {
		activeTab = index;
		const tab = filteredTabs[index];
		if (tab) {
			goto(`/oc/settings?tab=${tab.id}`, { replaceState: true });
		}
	}
</script>

{#if isLoading}
	<div class="loading-container flex flex-col items-center gap-2 py-6">
		<p class="loading-text">Загрузка...</p>
		<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
	</div>
{:else if error}
	<p class="error-text text-error">{error}</p>
{:else}
	<div class="hidden md:block">
		<div class="container">
			<div class="tabs-container">
				<div class="tabs tabs-lift tabs-md w-full" role="tablist" aria-label="Open Connect settings">
					{#each filteredTabs as tab, index}
						<input
							type="radio"
							name="oc_settings_tabs"
							role="tab"
							class="tab flex-1 whitespace-nowrap"
							aria-label={tab.title}
							checked={activeTab === index}
							onchange={() => selectTab(index)}
						/>
						<div role="tabpanel" class="tab-content mt-4 w-full">
							{#if activeTab === index}
								{#if tab.id === 'messengers'}
									<MessengerSettings
										{subsystemsState}
										onToggleTelegram={toggleSubsystemHandler('telegram_bot')}
										onToggleVk={toggleSubsystemHandler('vk_bot')}
									/>
								{:else}
									{@const Component = tab.component}
									<Component
										{...tab.props}
										disabled={!isSubsystemEnabled(tab.id)}
										onToggle={tabSupportsSubsystemToggle(tab)
											? toggleSubsystemHandler(tab.id as keyof SubsystemsState)
											: undefined}
									/>
								{/if}
							{/if}
						</div>
					{/each}
				</div>
			</div>
		</div>
	</div>

	<div class="flex min-h-0 flex-1 flex-col md:hidden">
		<SettingsMobileScroll
			items={mobileItems}
			sectionIdPrefix="oc-settings"
			tabFromUrl={tabFromUrl}
			pageTitle="Open Connect"
			denseGrid={true}
			sectionBodyCard={true}
		/>
	</div>
{/if}

<style lang="scss">
	.error-text {
		color: var(--red-600);
	}

	.tabs-container {
		display: flex;
		flex-direction: column;
	}

	:global(.tabs .tab) {
		font-size: 1.125rem;
		font-weight: 600;
	}

	:global(.tabs .tab:checked) {
		background-color: var(--color-primary);
	}
</style>
