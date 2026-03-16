<script lang="ts">
	import { page } from '$app/state';
	import SystemStatusTab from './SystemStatusTab.svelte';
	import SystemMetricsTab from './SystemMetricsTab.svelte';
	import SystemCoredumpTab from './SystemCoredumpTab.svelte';
	import SystemUpdateTab from './SystemUpdateTab.svelte';
	import { goto } from '$app/navigation';

	type TabId = 'status' | 'metrics' | 'coredump' | 'update';

	type Tab = {
		id: TabId;
		title: string;
		component:
			| typeof SystemStatusTab
			| typeof SystemMetricsTab
			| typeof SystemCoredumpTab
			| typeof SystemUpdateTab;
	};

	const tabs: Tab[] = [
		{ id: 'status', title: 'Status', component: SystemStatusTab },
		{ id: 'metrics', title: 'Metrics', component: SystemMetricsTab },
		{ id: 'coredump', title: 'Core dump', component: SystemCoredumpTab },
		{ id: 'update', title: 'Update', component: SystemUpdateTab }
	];

	let activeIndex = $state(0);

	// Инициализация активной вкладки из query-параметра ?tab=
	$effect(() => {
		const tabId = page.url.searchParams.get('tab') as TabId | null;
		if (tabId) {
			const idx = tabs.findIndex((t) => t.id === tabId);
			if (idx !== -1) {
				activeIndex = idx;
			}
		}
	});

	function setActiveTab(index: number) {
		activeIndex = index;
		const tab = tabs[index];
		if (tab) {
			goto(`/system?tab=${tab.id}`, { replaceState: true });
		}
	}
</script>

<div
	class="mx-0 my-1 flex flex-col space-y-4
     sm:mx-8 sm:my-8"
>
	<div class="tabs-container">
		<div class="tabs tabs-lift tabs-md w-full" role="tablist" aria-label="System tabs">
			{#each tabs as tab, index}
				<input
					type="radio"
					name="system_tabs"
					role="tab"
					class="tab flex-1 whitespace-nowrap"
					aria-label={tab.title}
					checked={activeIndex === index}
					onchange={() => setActiveTab(index)}
				/>
				<div role="tabpanel" class="tab-content w-full mt-4">
					{#if activeIndex === index}
						{@const Component = tab.component}
						<Component />
					{/if}
				</div>
			{/each}
		</div>
	</div>
</div>

<style lang="scss">
	.tabs-container {
		display: flex;
		flex-direction: column;
	}

	/* Цвет нижней границы и шрифт вкладок */
	:global(.tabs.tabs-lift .tab) {
		border-bottom-color: oklch(var(--bc, 0.25 0 260) / 0.25);
		font-size: 1.125rem;
		font-weight: 600;
	}

	/* Убираем нижнюю границу у содержимого вкладок */
	:global(.tabs.tabs-lift .tab-content) {
		border-bottom: none;
	}

	/* Активный таб — фон primary */
	:global(.tabs .tab:checked) {
		background-color: var(--color-primary);
	}
</style>

