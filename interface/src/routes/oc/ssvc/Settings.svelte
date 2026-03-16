<script lang="ts">
	import { page } from '$app/stores';
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { fetchSettings, updateSetting } from '$lib/api/ssvcApi';
	import { notifications } from '$lib/components/toasts/notifications';
	import GeneralSettings from '$lib/components/SsvcSettings/GeneralSettings.svelte';
	import ValveBandwidth from '$lib/components/SsvcSettings/ValveBandwidth.svelte';
	import SpeedSettings from '$lib/components/SsvcSettings/SpeedSettings.svelte';
	import ParallelValve from '$lib/components/SsvcSettings/ParallelValve.svelte';
	import Reload from '~icons/tabler/reload';
	import { goto } from '$app/navigation';

	let ssvcSettings = $state<SsvcSettings | null>();
	let activeTab = $state(0);

	const loadSsvcSettings = async () => {
		try {
			ssvcSettings = await fetchSettings();
		} catch (err) {
			if (err instanceof Error) {
				console.error(err);
			}
		}
	};

	async function saveChanges(field: string, value: unknown) {
		const result = await updateSetting(field, value);
		if (result) {
			notifications.success('Настройка сохранена', 5000);
			await loadSsvcSettings();
		} else {
			notifications.error('Ошибка сохранения настроек', 5000);
		}
	}

	interface Tab {
		id: string;
		title: string;
		component: any;
		props: Record<string, unknown>;
	}

	const availableTabs = $derived(computeTabs(ssvcSettings));

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
		const tabId = $page.url.searchParams.get('tab');
		if (tabId) {
			const index = availableTabs.findIndex((tab) => tab.id === tabId);
			if (index !== -1) {
				activeTab = index;
				return;
			}
		}

		if (activeTab >= availableTabs.length && availableTabs.length > 0) {
			activeTab = 0;
		}
	});

	function setActiveTab(index: number) {
		activeTab = index;
		const tab = availableTabs[index];
		if (tab) {
			goto(`/oc/ssvc?tab=${tab.id}`, { replaceState: true });
		}
	}
</script>

<div class="container oc-ssvc-settings-root">
	<div class="tabs-container">
		{#if !ssvcSettings}
			<div class="flex flex-col items-center justify-center gap-2 py-6">
				<p class="loading-text">Загрузка настроек SSVC...</p>
				<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
			</div>
		{:else}
			<div class="tabs tabs-lift tabs-md w-full" role="tablist" aria-label="SSVC settings tabs">
				{#each availableTabs as tab, index}
					<input
						type="radio"
						name="oc_ssvc_tabs"
						role="tab"
						class="tab flex-1 whitespace-nowrap"
						aria-label={tab.title}
						checked={activeTab === index}
						onchange={() => setActiveTab(index)}
					/>
					<div role="tabpanel" class="tab-content w-full mt-4">
						{#if activeTab === index}
							{@const Component = tab.component}
							<div class="settings-container">
								<Component {...tab.props} />
							</div>
						{/if}
					</div>
				{/each}
			</div>
		{/if}

		{#if ssvcSettings}
			<div class="ssvc-refresh-actions mt-6 flex justify-center">
				<button
					type="button"
					class="btn btn-ssvc-refresh"
					title="Перезагрузить настройки"
					onclick={loadSsvcSettings}
				>
					<Reload class="h-5 w-5" />
					<span>Обновить</span>
				</button>
			</div>
		{/if}
	</div>
</div>

<style lang="scss">
	.tabs-container {
		display: flex;
		flex-direction: column;
	}

	:global(.oc-ssvc-settings-root .settings-container) {
		width: 100%;
	}

	:global(.tabs .tab) {
		font-size: 1.125rem;
		font-weight: 600;
	}

	:global(.tabs .tab:checked) {
		background-color: var(--color-primary);
	}

	.btn-ssvc-refresh {
		background-color: var(--color-info);
		color: oklch(1 0 0);
		border: none;
	}

	.btn-ssvc-refresh:hover {
		opacity: 0.9;
	}
</style>
