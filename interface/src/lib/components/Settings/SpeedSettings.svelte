<script lang="ts">
	import Pencil from '~icons/tabler/pencil';
	import type { SsvcSettings } from '$lib/types/models';
	import EditNumbersModal from '$lib/components/Settings/EditNumbersModal.svelte';

	const { settings, onSave } = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: any) => void;
	}>();

	let editingField = $state<string | null>(null);
	let currentValues = $state<any[]>([]);

	const valveTypes = $derived([
		{
			key: 'heads',
			label: 'Головы',
			values: [
				{
					name: 'Время открытия клапана',
					value: settings.heads[0],
					step: 0.1,
					unit: 'сек',
					precision: 1,
					min: 0
				},
				{
					name: 'Периодичность',
					value: settings.heads[1],
					step: 0.1,
					unit: 'сек',
					precision: 1,
					min: 0
				}
			],
			hideDescription: false
		},
		{
			key: 'late_heads',
			label: 'Подголовники',
			condition: !!settings.late_heads,
			values: settings.late_heads
				? [
						{
							name: 'Время открытия клапана',
							value: settings.late_heads[0],
							step: 0.1,
							unit: 'сек',
							precision: 1,
							min: 0
						},
						{
							name: 'Периодичность',
							value: settings.late_heads[1],
							step: 0.1,
							unit: 'сек',
							precision: 1,
							min: 0
						}
					]
				: [],
			hideDescription: false
		},
		{
			key: 'hearts',
			label: 'Тело',
			values: [
				{
					name: 'Время открытия клапана',
					value: settings.hearts[0],
					step: 0.1,
					unit: 'сек',
					precision: 1,
					min: 0
				},
				{
					name: 'Периодичность',
					value: settings.hearts[1],
					step: 0.1,
					unit: 'сек',
					precision: 1,
					min: 0
				}
			],
			hideDescription: false
		},
		{
			key: 'tails',
			label: 'Хвосты',
			condition: !!settings.tails,
			values: settings.tails
				? [
						{
							name: 'Время открытия клапана',
							value: settings.tails[0],
							step: 0.1,
							unit: 'сек',
							precision: 1,
							min: 0
						},
						{
							name: 'Периодичность',
							value: settings.tails[1],
							step: 0.1,
							unit: 'сек',
							precision: 1,
							min: 0
						}
					]
				: [],
			hideDescription: false
		}
	]);
</script>

<div class="mt-4">
	<div class="grid grid-cols-1 md:grid-cols-2 gap-3">
		{#each valveTypes as { key, label, values, condition }}
			{#if condition !== false}
				<div
					class="border border-gray-300 dark:border-gray-600 rounded-lg p-4 bg-white dark:bg-gray-800 shadow-sm"
				>
					<div class="flex justify-between items-start mb-3">
						<h3 class="font-medium text-lg">{label}</h3>
						<EditNumbersModal
							values={[
								{
									name: 'Время открытия клапана',
									value: values[0].value,
									step: 0.1,
									precision: 1,
									min: 0,
									max: 99.9
								},
								{
									name: 'Период',
									value: values[1].value,
									min: 0,
									max: 999
								}
							]}
							onSave={(newValues) => onSave(key, [newValues[0].value, newValues[1].value])}
							showValue={false}
						>
							<button class="text-blue-500 hover:text-blue-700">
								<Pencil class="w-5 h-5" />
							</button>
						</EditNumbersModal>
					</div>

					<div class="space-y-2">
						<div class="flex justify-between items-center">
							<span class="text-gray-600 dark:text-gray-400">Время открытия:</span>
							<span class="font-medium">
								{values[0].value}
								{values[0].unit}
							</span>
						</div>

						<div class="flex justify-between items-center">
							<span class="text-gray-600 dark:text-gray-400">Периодичность:</span>
							<span class="font-medium">
								{values[1].value}
								{values[1].unit}
							</span>
						</div>
					</div>
				</div>
			{/if}
		{/each}
	</div>
</div>
