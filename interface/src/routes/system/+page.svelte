<script lang="ts">
	import { page } from '$app/state';
	import { browser } from '$app/environment';
	import { tick } from 'svelte';
	import { goto } from '$app/navigation';
	import { user } from '$lib/stores/user';
	import SystemStatusTab from './SystemStatusTab.svelte';
	import SystemMetricsTab from './SystemMetricsTab.svelte';
	import SystemCoredumpTab from './SystemCoredumpTab.svelte';
	import SystemUpdateTab from './SystemUpdateTab.svelte';
	import SettingsMobileScroll from '$lib/components/settings-layout/SettingsMobileScroll.svelte';

	const MOBILE_MQ = '(max-width: 767px)';

	type TabId = 'status' | 'metrics' | 'coredump' | 'update';

	type SystemTab = {
		id: TabId;
		title: string;
		component:
			| typeof SystemStatusTab
			| typeof SystemMetricsTab
			| typeof SystemCoredumpTab
			| typeof SystemUpdateTab;
	};

	const allTabs: SystemTab[] = [
		{ id: 'status', title: 'Статус системы', component: SystemStatusTab },
		{ id: 'metrics', title: 'Системный монитор', component: SystemMetricsTab },
		{ id: 'coredump', title: 'Core dump', component: SystemCoredumpTab },
		{ id: 'update', title: 'Обновление системы', component: SystemUpdateTab }
	];

	const visibleTabs = $derived.by(() => {
		const f = page.data.features;
		const adminOk = !f.security || $user.admin;
		return allTabs.filter((tab) => {
			if (tab.id === 'status') return true;
			if (tab.id === 'metrics') return f.analytics;
			if (tab.id === 'coredump') return f.coredump;
			if (tab.id === 'update') {
				return (f.ota || f.upload_firmware || f.download_firmware) && adminOk;
			}
			return false;
		});
	});

	const tabFromUrl = $derived(page.url.searchParams.get('tab') as TabId | null);

	const mobileItems = $derived(
		visibleTabs.map((t) => ({
			id: t.id,
			title: t.title,
			component: t.component,
			props: {} as Record<string, unknown>
		}))
	);

	let activeIndex = $state(0);

	$effect(() => {
		const tabId = tabFromUrl;
		if (visibleTabs.length === 0) return;
		if (tabId) {
			const idx = visibleTabs.findIndex((t) => t.id === tabId);
			if (idx !== -1) {
				activeIndex = idx;
				return;
			}
			goto('/system', { replaceState: true });
			return;
		}
		if (activeIndex >= visibleTabs.length) {
			activeIndex = 0;
		}
	});

	$effect(() => {
		const tabId = tabFromUrl;
		if (!browser || !tabId || visibleTabs.length === 0) return;
		const ok = visibleTabs.some((t) => t.id === tabId);
		if (!ok) return;
		tick().then(() => {
			try {
				if (!window.matchMedia(MOBILE_MQ).matches) return;
			} catch {
				return;
			}
			const el = document.getElementById(`system-section-${tabId}`);
			if (el && typeof el.scrollIntoView === 'function') {
				el.scrollIntoView({ block: 'start', behavior: 'instant' });
			}
		});
	});

	function setActiveTab(index: number) {
		activeIndex = index;
		const tab = visibleTabs[index];
		if (tab) {
			goto(`/system?tab=${tab.id}`, { replaceState: true });
		}
	}
</script>

<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
	<div class="hidden md:block">
		<div class="tabs-container">
			<div class="tabs tabs-lift tabs-md w-full" role="tablist" aria-label="System tabs">
				{#each visibleTabs as tab, index}
					<input
						type="radio"
						name="system_tabs"
						role="tab"
						class="tab flex-1 whitespace-nowrap"
						aria-label={tab.title}
						checked={activeIndex === index}
						onchange={() => setActiveTab(index)}
					/>
					<div role="tabpanel" class="tab-content mt-4 w-full">
						{#if activeIndex === index}
							{@const Component = tab.component}
							<Component />
						{/if}
					</div>
				{/each}
			</div>
		</div>
	</div>

	<div class="flex min-h-0 flex-1 flex-col md:hidden">
		<SettingsMobileScroll
			items={mobileItems}
			sectionIdPrefix="system"
			tabFromUrl={tabFromUrl}
			pageTitle="Система"
		/>
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

	:global(.tabs.tabs-lift .tab-content) {
		border-bottom: none;
	}

	:global(.tabs .tab:checked) {
		background-color: var(--color-primary);
	}
</style>
