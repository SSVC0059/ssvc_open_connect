<script lang="ts">
	import Pencil from '~icons/tabler/pencil';

	let { value, unit, type, step, onSave } = $props();

	let isEditing = $state(false);
	let draftValue = $state(value);
</script>

{#if isEditing}
	<div class="flex flex-col sm:flex-row sm:flex-wrap gap-2 items-start sm:items-center">
		<input
			{type}
			bind:value={draftValue}
			class="min-w-0 flex-1 px-2 py-1 border rounded w-full sm:max-w-[8ch]"
			maxlength="4"
			size="4"
			{step}
		/>
		<div class="flex gap-2 flex-wrap">
			<button
				class="px-2 py-1 bg-green-800 text-white rounded whitespace-nowrap"
				onclick={() => {
					onSave(draftValue);
					isEditing = false;
				}}
			>
				✓
			</button>
			<button
				class="px-2 py-1 bg-gray-200 rounded whitespace-nowrap"
				onclick={() => {
					draftValue = value;
					isEditing = false;
				}}
			>
				×
			</button>
		</div>
	</div>
{:else}
	<div class="flex gap-2 items-center">
		<span class="truncate">{value} {unit}</span>
		<button
			class="text-blue-500 hover:text-blue-700 flex-shrink-0"
			onclick={() => (isEditing = true)}
		>
			<Pencil />
		</button>
	</div>
{/if}
