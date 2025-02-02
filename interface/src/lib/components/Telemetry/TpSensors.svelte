<script lang="ts">
	export let name: string;
	export let temp: number

</script>

{#if temp !== null}
		<div class="text-center">
			{#if name}
				<p class="text-base font-medium">{name}:</p>
			{/if}
			<p class="text-lg ">{temp ? temp.toFixed(2): "-"} °C</p>
		</div>
{/if}