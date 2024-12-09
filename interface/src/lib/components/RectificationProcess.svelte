<script lang="ts">
	export let telemetryData: {
		open: number | null;
		period: number | null;
		valveBandwidthHeads: number | null;
		valveBandwidthHearts: number | null;
		valveBandwidthTails: number | null;
	} | null = null;
</script>

<table class="min-w-full text-left text-sm">
	<thead>
	<tr>
		<th class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Параметр</th>
		<th class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Значение</th>

	</tr>
	</thead>
	<tbody>
	{#if telemetryData}
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Время открытия клапана, с</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{telemetryData.open ?? '-'}</td>
		</tr>
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Переодичность открытия</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{telemetryData.period ?? '-'}</td>
		</tr>
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Отобрано (головы), мл</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{telemetryData.valveBandwidthHeads ?? '-'}</td>
		</tr>
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Отобрано (тело), мл</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{telemetryData.valveBandwidthHearts ?? '-'}</td>
		</tr>
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Отобрано (хвосты), мл</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">{telemetryData.valveBandwidthTails ?? '-'}</td>
		</tr>
	{/if}
	</tbody>
</table>
