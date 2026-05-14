<script lang="ts">
	import { page } from '$app/stores';
	import TelegramSettings from './TelegramSettings.svelte';
	import VkSettings from './VkSettings.svelte';
	import type { SubsystemsState } from '$lib/types/ssvc';

	let {
		subsystemsState,
		onToggleTelegram,
		onToggleVk
	}: {
		subsystemsState: SubsystemsState;
		onToggleTelegram: () => Promise<void>;
		onToggleVk: () => Promise<void>;
	} = $props();

	const vkFeature = $derived(
		Boolean(($page.data as { features?: { vk_bot?: boolean } }).features?.vk_bot)
	);
</script>

<div class="messenger-stack">
	<h2 class="messenger-stack__title">Telegram</h2>
	<TelegramSettings disabled={!subsystemsState.telegram_bot} onToggle={onToggleTelegram} />

	{#if vkFeature}
		<h2 class="messenger-stack__title messenger-stack__title--second">ВКонтакте</h2>
		<VkSettings disabled={!subsystemsState.vk_bot} onToggle={onToggleVk} />
	{/if}
</div>

<style lang="scss">
	@use '$lib/styles/base/variables' as v;

	.messenger-stack {
		display: flex;
		flex-direction: column;
		gap: 0;
	}

	.messenger-stack__title {
		font-size: 1.125rem;
		font-weight: 600;
		margin: 0 0 0.75rem;
		color: var(--color-base-content, inherit);
	}

	.messenger-stack__title--second {
		margin-top: 1.5rem;
		padding-top: 1rem;
		border-top: 1px solid color-mix(in srgb, var(--color-base-content, #888) 12%, transparent);
	}
</style>
