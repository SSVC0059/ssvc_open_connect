<script lang="ts">
	import { page } from '$app/stores';
	import SensorsSettings from '$lib/components/OCSettings/SensorsSettings.svelte';
	import TelegramSettings from '$lib/components/OCSettings/TelegramSettings.svelte';
	import ProfileManager from '$lib/components/profiles/ProfileManager.svelte';
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
		const tab = availableTabs.find((t) => t.id === id);
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
				onclick={() => (isMobileMenuOpen = !isMobileMenuOpen)}
			>
				<span class="mobile-menu-header">
					{filteredTabs[activeTab]?.title || 'Меню'}
				</span>
				<svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
					<path d="M7 10l5 5 5-5z" />
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
			<p>Загрузка...</p>
			<!-- Or a spinner component -->
		{:else if error}
			<p class="error-text">{error}</p>
		{:else}
			{#each filteredTabs as tab, index}
				{#if activeTab === index}
					{@const Component = tab.component}
					<Component
						{...tab.props}
						disabled={!isSubsystemEnabled(tab.id)}
						onToggle={tab.isStatic ? undefined : toggleSubsystemHandler(tab.id as keyof SubsystemsState)}
					/>
				{/if}
			{/each}
		{/if}
	</div>
</div>

<style lang="scss">
	@use "$lib/styles/base/variables" as v;
	@use "$lib/styles/base/mixins" as m;

	.error-text {
		color: var(--red-600);
	}

	/* ===== КОМПОНЕНТ ВКЛАДОК (TABS) ===== */
	.tabs-container {
		display: flex;
		flex-direction: column;

		/* Единый фон контента вкладок (как у панели профилей), чтобы Telegram не был темнее Профилей */
		.settings-container {
			background: oklch(var(--b1));
			border: 1px solid oklch(var(--bc) / 0.2);
			box-shadow: 0 4px 24px oklch(var(--bc) / 0.12);
		}

		/* Десктопная навигация */
		.tabs-nav {
			display: flex;
			justify-content: flex-start;
			align-items: flex-end;
			padding-bottom: 0.5rem;
			gap: 2rem;

			@media (max-width: v.$breakpoint-md) {
				display: none;
			}
		}

		.tab {
			padding: 0.3rem 0.25rem;
			white-space: nowrap;
			color: var(--primary-500);
			transition: var(--transition);
			border: none;
			background: none;
			cursor: pointer;
			font: inherit;

			@include m.dark-theme-color;

			&:hover {
				color: var(--primary-700);
			}

			&.tab-active {
				border-bottom: 2px solid var(--blue-600);
				color: var(--blue-600);
				border-radius: 0;
			}

			@media (min-width: v.$breakpoint-md) {
				font-size: clamp(1.125rem, 1.5vw + 0.5rem, 1.5rem);
			}
		}

		/* Мобильная навигация */
		.mobile-tabs-header {
			position: relative;
			display: none;

			@media (max-width: v.$breakpoint-md) {
				display: block;
				margin-bottom: 1rem;
			}

			.mobile-menu-header {
				@include m.dark-theme-color;
			}

			&.menu-open {
				.mobile-tabs-dropdown {
					opacity: 1;
					visibility: visible;
					transform: translateY(0);
				}

				.mobile-menu-toggle svg {
					transform: rotate(180deg);
				}
			}
		}

		.mobile-menu-toggle {
			@include m.glassmorphism;
			@include m.flex-center;
			gap: 0.5rem;
			width: 100%;
			padding: 0.75rem 1rem;
			font-size: 1rem;
			font-weight: 500;
			color: var(--primary-700);
			border: none;
			border-radius: var(--border-radius);
			cursor: pointer;
			transition: var(--transition);

			svg {
				transition: transform 0.2s ease;
			}

			&:hover {
				background: rgba(255, 255, 255, 0.9);
			}
		}

		.mobile-tabs-dropdown {
			@include m.glassmorphism;
			background: var(--white);
			position: absolute;
			top: 100%;
			left: 0;
			right: 0;
			z-index: var(--z-popover);
			margin-top: 0.25rem;
			border-radius: var(--border-radius);
			opacity: 0;
			visibility: hidden;
			transform: translateY(-10px);
			transition: all 0.2s ease;
			max-height: 60vh;
			overflow-y: auto;

			scrollbar-width: thin;
			scrollbar-color: var(--primary-300) transparent;

			&::-webkit-scrollbar {
				width: 4px;
			}

			&::-webkit-scrollbar-track {
				background: transparent;
			}

			&::-webkit-scrollbar-thumb {
				background: var(--primary-300);
				border-radius: 2px;
			}
		}

		.mobile-tab {
			display: block;
			width: 100%;
			padding: 0.75rem 1rem;
			text-align: left;
			font-size: 0.9rem;
			color: var(--primary-600);
			background: none;
			border: none;
			border-bottom: 1px solid var(--primary-200);
			cursor: pointer;
			transition: var(--transition);

			&:last-child {
				border-bottom: none;
			}

			&:hover {
				background: color-mix(in srgb, var(--primary-100) 30%, transparent);
				color: var(--primary-700);
			}

			&.mobile-tab-active {
				background: color-mix(in srgb, var(--primary-500) 10%, transparent);
				color: var(--primary-800);
				font-weight: 600;
			}
		}

		body & .desktop-only {
			@media (max-width: v.$breakpoint-md) {
				display: none;
			}
		}

		body & .mobile-only {
			@media (min-width: v.$breakpoint-md) {
				display: none;
			}
		}
	}
</style>
