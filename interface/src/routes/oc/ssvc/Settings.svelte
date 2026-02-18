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

<style lang="scss">
	@use "$lib/styles/base/variables" as v;
	@use "$lib/styles/base/mixins" as m;

	/* ===== КОМПОНЕНТ ВКЛАДОК (TABS) ===== */
	.tabs-container {
		display: flex;
		flex-direction: column;

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

			.tabs-nav-content {
				display: flex;
				gap: 2rem;
				overflow-x: auto;
				padding-bottom: 0.5rem;
				scrollbar-width: none;
				-ms-overflow-style: none;

				&::-webkit-scrollbar {
					display: none;
				}

				margin-bottom: -0.5rem;
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

		/* Стили для кнопки перезагрузки */
		.refresh-button {
			@include m.flex-center;
			width: 2.5rem;
			height: 2.5rem;
			border: none;
			border-radius: var(--border-radius);
			background: var(--primary-100);
			cursor: pointer;
			transition: var(--transition);
			flex-shrink: 0;
			margin-left: 2rem;
			margin-bottom: 0.5rem;

			svg {
				width: 1.25rem;
				height: 1.25rem;
				color: var(--primary-700);
				transition: var(--transition);
			}

			&:hover {
				background: var(--primary-200);
				box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
				svg {
					transform: rotate(15deg);
					color: var(--blue-600);
				}
			}

			&:active {
				background: var(--primary-300);
				transform: scale(0.98);
			}

			&.mobile-menu-item {
				display: flex;
				width: 100%;
				height: auto;
				padding: 0.75rem 1rem;
				margin: 0;
				border-radius: 0;
				background: none;
				justify-content: flex-start;
				gap: 1rem;
				color: var(--primary-600);
				border-bottom: 1px solid var(--primary-200);

				&:last-child {
					border-bottom: none;
				}

				&:hover {
					background: color-mix(in srgb, var(--primary-100) 30%, transparent);
					color: var(--primary-700);
					box-shadow: none;
					svg {
						transform: none;
					}
				}

				&:active {
					transform: none;
				}

				span {
					font-size: 0.9rem;
				}
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