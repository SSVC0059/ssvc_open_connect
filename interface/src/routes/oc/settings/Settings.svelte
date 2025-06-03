<script lang="ts">
	import ThermalSensorSettings from '$lib/components/OCSettings/ThermalSensorSettings.svelte';
	import { page } from '$app/state';
	import MenuIcon from '~icons/tabler/menu-2';
	import { fade, fly } from 'svelte/transition';
	import TelegramSettings from '$lib/components/OCSettings/TelegramSettings.svelte';

	interface Tab {
		id: string;
		title: string;
		component: any;
	}

	let activeTab = $state(0);
	let isMenuCollapsed = $state(true);

	const availableTabs: Tab[] = [
		{
			id: 'thermal',
			title: 'Термодатчики',
			component: ThermalSensorSettings
		},
		{
			id: 'telegram',
			title: 'Telegram',
			component: TelegramSettings
		}
		// Здесь можно добавить другие вкладки
	];

	function toggleMenu() {
		isMenuCollapsed = !isMenuCollapsed;
	}
</script>

<div
	class="relative min-w-full border border-gray-300 rounded-xl shadow-md overflow-hidden flex flex-col"
>
	<!-- Заголовок с кнопкой сворачивания -->
	<div
		class="flex items-center justify-between border-b border-gray-200 p-2 bg-white dark:bg-gray-800"
	>
		<h2 class="text-lg font-semibold px-2 tracking-wide">{page.data.title}</h2>
		<button
			class="md:hidden p-2 rounded-lg hover:bg-gray-100 dark:hover:bg-gray-700"
			onclick={toggleMenu}
		>
			<MenuIcon class="w-5 h-5" />
		</button>
	</div>

	<div class="flex flex-col md:flex-row relative">
		<!-- Мобильное меню поверх всего -->
		{#if !isMenuCollapsed}
			<div
				in:fly={{ y: -10, duration: 200 }}
				out:fade={{ duration: 200 }}
				class="absolute top-1 left-2 right-2 z-20 bg-white dark:bg-gray-800 border border-gray-200 dark:border-gray-700 shadow-lg rounded-xl p-2 md:hidden"
			>
				{#each availableTabs as tab, index}
					<button
						class={`w-full text-left px-4 py-3 text-sm font-semibold tracking-wide rounded-lg transition-colors
						${
							activeTab === index
								? 'bg-blue-50 text-blue-600 dark:bg-blue-900/30 dark:text-blue-400'
								: 'hover:bg-gray-100 dark:hover:bg-gray-700 text-gray-700 dark:text-gray-300'
						}`}
						onclick={() => {
							activeTab = index;
							isMenuCollapsed = true;
						}}
					>
						{tab.title}
					</button>
				{/each}
			</div>
		{/if}

		<!-- Десктопное меню -->
		<div
			class="hidden md:block bg-white dark:bg-gray-800 border-r border-gray-200 dark:border-gray-700 min-w-[220px] p-2"
		>
			{#each availableTabs as tab, index}
				<button
					class={`flex w-full text-left px-4 py-3 text-[15px] font-semibold tracking-wide rounded-lg transition-colors
					${
						activeTab === index
							? 'bg-blue-50 text-blue-600 dark:bg-blue-900/30 dark:text-blue-400'
							: 'hover:bg-gray-100 dark:hover:bg-gray-700 text-gray-700 dark:text-gray-300'
					}`}
					onclick={() => (activeTab = index)}
				>
					{tab.title}
				</button>
			{/each}
		</div>

		<!-- Контент -->
		<div class="flex-1 p-4 bg-white dark:bg-gray-800 min-h-[300px]">
			{#each availableTabs as tab, index}
				{#if activeTab === index}
					{@const Component = tab.component}
					<Component />
				{/if}
			{/each}
		</div>
	</div>
</div>

<style>
	.hide-scrollbar::-webkit-scrollbar {
		display: none;
	}

	.hide-scrollbar {
		scrollbar-width: none;
	}
</style>
