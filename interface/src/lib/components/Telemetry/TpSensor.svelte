<script lang="ts">
	interface Props {
		name: string;
		temp: number;
	}

	let { name, temp }: Props = $props();
</script>

{#if temp !== null}
	<div class="flex flex-col items-center justify-center text-center px-2">
		{#if name}
			<div class="text-sm font-semibold">
				{name}
			</div>
		{/if}
		<div class="text-2xl font-bold text-blue-600 dark:text-blue-300 whitespace-nowrap">
			{temp.toFixed(2)} °C
		</div>
	</div>
{/if}
