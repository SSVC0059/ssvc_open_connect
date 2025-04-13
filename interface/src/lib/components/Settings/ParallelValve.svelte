<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';

	const { settings, onSave } = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: any) => void;
	}>();
</script>

<div class="mt-4">
	<table class="table-auto w-full border-collapse border border-gray-700 dark:border-gray-600">
		<thead>
			<tr class="bg-blue-500 text-white dark:bg-gray-800">
				<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Параметр</th>
				<th class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center">Значение</th>
			</tr>
		</thead>
		<tbody>
			<tr>
				<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left"
					>Время открытого клапана</td
				>
				<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center"
					>{settings.parallel[0]} сек</td
				>
			</tr>
			<tr>
				<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-left">Период, с</td>
				<td class="border border-gray-700 dark:border-gray-600 px-4 py-2 text-center"
					>{settings.parallel[1]} сек</td
				>
			</tr>
		</tbody>
	</table>
</div>

<style>
	.settings-table {
		@apply w-full border-collapse border border-gray-700 dark:border-gray-600;
	}
	.settings-table th,
	.settings-table td {
		@apply border border-gray-700 dark:border-gray-600;
	}

	.settings-table {
		@apply w-full border-collapse border border-gray-700 dark:border-gray-600;
		table-layout: fixed;
	}
	.settings-table th,
	.settings-table td {
		@apply border border-gray-700 dark:border-gray-600;
	}
	.settings-table th:first-child,
	.settings-table td:first-child {
		width: 60%;
	}
	.settings-table th:last-child,
	.settings-table td:last-child {
		width: 40%;
	}
	.editable-container {
		min-width: 100px;
		display: inline-block;
	}
</style>
