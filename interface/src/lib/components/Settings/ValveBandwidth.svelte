<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';
	import EditNumbersModal from '$lib/components/Settings/EditNumbersModal.svelte';
	import Pencil from '~icons/tabler/pencil';

	const { settings, onSave } = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: any) => void;
	}>();
</script>

<div
	class="border border-gray-300 dark:border-gray-600 rounded-lg p-4 bg-white dark:bg-gray-800 shadow-sm"
>
	<div class="flex justify-between items-start mb-3">
		<h3 class="font-medium text-lg">Пропускная способность клапанов</h3>
		<EditNumbersModal
			onSave={(newValues) =>
				onSave('valve_bw', [newValues[0].value, newValues[1].value, newValues[2].value])}
			showValue={false}
			values={[
				{
					name: 'Головы',
					value: settings.valve_bw[0],
					min: 0,
					max: 20000
				},
				{
					name: 'Тело',
					value: settings.valve_bw[1],
					min: 0,
					max: 20000
				},
				{
					name: 'Подголовники | Хвосты',
					value: settings.valve_bw[2],
					min: 0,
					max: 20000
				}
			]}
		>
			<button class="text-blue-500 hover:text-blue-700">
				<Pencil class="w-5 h-5" />
			</button>
		</EditNumbersModal>
	</div>

	<div class="space-y-2">
		<div class="flex justify-between items-center">
			<span class="text-gray-600 dark:text-gray-400">Головы</span>
			<span class="font-medium">
				{settings.valve_bw[0]} мл/ч
			</span>
		</div>
		{#if settings.late_heads}
			<div class="flex justify-between items-center">
				<span class="text-gray-600 dark:text-gray-400">Подголовники</span>
				<span class="font-medium">
					{settings.valve_bw[2]} мл/ч
				</span>
			</div>
		{/if}

		<div class="flex justify-between items-center">
			<span class="text-gray-600 dark:text-gray-400">Тело:</span>
			<span class="font-medium">
				{settings.valve_bw[1]} мл/ч
			</span>
		</div>

		{#if settings.tails}
			<div class="flex justify-between items-center">
				<span class="text-gray-600 dark:text-gray-400">Хвосты</span>
				<span class="font-medium">
					{settings.valve_bw[2]} мл/ч
				</span>
			</div>
		{/if}
	</div>
</div>
