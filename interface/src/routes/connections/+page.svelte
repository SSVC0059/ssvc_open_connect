<script lang="ts">
	import { page } from '$app/state';
	import MQTTView from './mqtt/MQTT.svelte';
	import NTPView from './ntp/NTP.svelte';

	type TabId = 'mqtt' | 'ntp';

	type Tab = {
		id: TabId;
		title: string;
		component: typeof MQTTView | typeof NTPView;
	};

	const tabs: Tab[] = [
		{ id: 'mqtt', title: 'MQTT', component: MQTTView },
		{ id: 'ntp', title: 'NTP', component: NTPView }
	];

	let activeIndex = $state(0);

	// Инициализация активной вкладки из query-параметра ?tab=
	$effect(() => {
		const tabId = $page.url.searchParams.get('tab') as TabId | null;
		if (tabId) {
			const idx = tabs.findIndex((t) => t.id === tabId);
			if (idx !== -1) {
				activeIndex = idx;
			}
		}
	});
</script>

<div
	class="mx-0 my-1 flex flex-col space-y-4
     sm:mx-8 sm:my-8"
>
	<div class="tabs-container">
		<div class="tabs tabs-lift tabs-md w-full" role="tablist">
			{#each tabs as tab, index}
				<input
					type="radio"
					name="connections_tabs"
					role="tab"
					class="tab flex-1 whitespace-nowrap"
					aria-label={tab.title}
					checked={activeIndex === index}
					on:change={() => (activeIndex = index)}
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
	@use '$lib/styles/base/variables' as v;

	.tabs-container {
		display: flex;
		flex-direction: column;
	}

	/* Общая нижняя граница под всеми табами */
	:global(.tabs.tabs-lift) {
		border-bottom: 1px solid oklch(var(--bc, 0.25 0 260) / 0.25);
	}

	/* Цвет нижней границы и шрифт вкладок */
	:global(.tabs.tabs-lift .tab) {
		border-bottom-color: oklch(var(--bc, 0.25 0 260) / 0.25);
		font-size: 1.125rem;
		font-weight: 600;
	}

	/* Активный таб — фон primary */
	:global(.tabs .tab:checked) {
		background-color: var(--color-primary);
	}
</style>

