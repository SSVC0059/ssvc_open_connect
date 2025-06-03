<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();
</script>

<div class="space-y-4">
	<h2 class="text-xl font-semibold text-gray-700">Пропускная способность клапанов</h2>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_head">Головы</label>
		<input
			bind:value={settings.valve_bw[0]}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_head"
			type="number"
		/>
	</div>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_hearts">Тело</label>
		<input
			bind:value={settings.valve_bw[1]}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_hearts"
			type="number"
		/>
	</div>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_tails"
			>Подголовники/Хвосты</label
		>
		<input
			bind:value={settings.valve_bw[2]}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_tails"
			type="number"
		/>
	</div>
</div>
