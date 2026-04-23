<script lang="ts">
	import type { Snippet } from 'svelte';

	const { title, defaultVisible = true, children } = $props<{
		title: string;
		defaultVisible?: boolean;
		children?: Snippet;
	}>();

	let isVisible = $state(defaultVisible);
</script>

<div class="space-y-2">
	<button
		type="button"
		class="flex items-center justify-between w-full text-xl font-semibold cursor-pointer hover:text-blue-600 dark:hover:text-blue-400 transition-colors"
		onclick={() => (isVisible = !isVisible)}
	>
		<span>{title}</span>
		<span class="ml-2 text-gray-500 dark:text-gray-400">
			{isVisible ? '▲' : '▼'}
		</span>
	</button>

	{#if isVisible}
		<div class="pt-2">
			{@render children?.()}
		</div>
	{/if}
</div>

<style>
	button {
		outline: none;
	}
</style>
