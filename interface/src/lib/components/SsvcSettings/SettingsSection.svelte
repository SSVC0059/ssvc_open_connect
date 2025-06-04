<script lang="ts">
	const { title, defaultVisible = true } = $props<{
		title: string;
		defaultVisible?: boolean;
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
			<slot />
		</div>
	{/if}
</div>

<style>
	button {
		outline: none;
	}
</style>
