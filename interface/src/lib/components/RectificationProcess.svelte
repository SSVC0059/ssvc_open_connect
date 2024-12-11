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
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Время открытия клапана</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
				{
					telemetryData.open
							? `${telemetryData.open } сек`
						: '-'
				}
			</td>
		</tr>
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Переодичность открытия</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
				{
					telemetryData.period
							? `${telemetryData.period } сек`
							: '-'
				}
			</td>
		</tr>
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Отобрано (головы)</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
				{
					telemetryData.valveBandwidthHeads !== undefined && telemetryData.valveBandwidthHeads !== null
							? `${telemetryData.valveBandwidthHeads} мл`
							: '-'
				}
			</td>
		</tr>
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Отобрано (тело), мл</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
				{
					telemetryData.valveBandwidthHearts !== undefined && telemetryData.valveBandwidthHearts !== null
							? `${telemetryData.valveBandwidthHearts} мл`
							: '-'
				}
		</tr>
		<tr>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">Отобрано (хвосты), мл</td>
			<td class="py-2 px-4 border-b border-blue-500 dark:border-white/50">
				{
					telemetryData.valveBandwidthTails !== undefined && telemetryData.valveBandwidthTails !== null
							? `${telemetryData.valveBandwidthTails} мл`
							: '-'
				}
		</tr>
	{/if}
	</tbody>
</table>
