<script lang="ts">
	interface Props {
		name?: string;
		timeString?: string;
	}

	let { name = '', timeString = '-' }: Props = $props();
</script>

<div class="flex flex-col items-center gap-2 p-1 rounded-lg">
	<p class="text-base font-medium text-gray-600 dark:text-white">
		{name}
	</p>
	<span class="text-base font-semibold text-indigo-500">
		{timeString}
	</span>
</div>

<style>
	div {
		font-family: 'Arial', sans-serif;
	}
</style>
