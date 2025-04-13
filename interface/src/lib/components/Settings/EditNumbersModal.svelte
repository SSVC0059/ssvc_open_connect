<script lang="ts">
	import Pencil from '~icons/tabler/pencil';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

	let {
		values: initialValues,
		onSave,
		showValue = true
	} = $props<{
		values: Array<{
			name: string;
			value: number;
			step?: number;
			unit?: string;
			precision?: number;
			min?: number;
			max?: number;
		}>;
		onSave: (
			values: Array<{
				name: string;
				value: number;
				step?: number;
				unit?: string;
				precision?: number;
				min?: number;
				max?: number;
			}>
		) => void;
		showValue?: boolean;
	}>();

	let isEditing = $state(false);
	let draftValues = $state([...initialValues]);

	// Тип для хранения информации о выходе за границы
	type OutOfBounds = {
		belowMin: boolean;
		aboveMax: boolean;
	};

	let bodyOverflow = '';
	$effect(() => {
		if (isEditing) {
			bodyOverflow = document.body.style.overflow;
			document.body.style.overflow = 'hidden';
		} else {
			document.body.style.overflow = bodyOverflow;
		}
	});

	// Исправленный вариант с использованием нового синтаксиса $derived
	const isOutOfBounds = $derived<OutOfBounds[]>(
		draftValues.map((item) => {
			const { value, min, max } = item;
			return {
				belowMin: min !== undefined && value < min,
				aboveMax: max !== undefined && value > max
			};
		})
	);
</script>

{#if isEditing}
	<div class="fixed inset-0 z-40 bg-black/50 backdrop-blur-sm fade-in"></div>

	<div
		role="dialog"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div
			class="bg-base-100 shadow-secondary/30 rounded-box pointer-events-auto flex max-h-full min-w-fit max-w-md flex-col justify-between p-4 shadow-lg"
		>
			<h2 class="text-base-content text-center text-2xl font-bold">Изменение переменной</h2>
			<div class="divider my-2"></div>
			<div class="overflow-y-auto space-y-4 p-4">
				{#each draftValues as { name, value, step, unit, precision, min, max }, i}
					<div class="flex flex-col gap-2">
						<label class="font-medium text-center">{name}</label>
						<div class="flex items-center gap-2">
							<input
								type="number"
								bind:value={draftValues[i].value}
								class="min-w-0 flex-1 px-2 py-1 border rounded transition-colors {isOutOfBounds[i]
									?.belowMin || isOutOfBounds[i]?.aboveMax
									? 'bg-red-100 border-red-500'
									: ''}"
								step={step ?? 1}
								{min}
								{max}
							/>
							{#if unit}
								<span class="whitespace-nowrap">{unit}</span>
							{/if}
						</div>
						{#if precision !== undefined}
							<div class="text-sm text-gray-500">Точность: {precision} знак после запятой</div>
						{/if}
						{#if min !== undefined || max !== undefined}
							<span
								class:text-red-500={isOutOfBounds[i]?.belowMin || isOutOfBounds[i]?.aboveMax}
								class="whitespace-nowrap text-sm text-gray-500"
							>
								Допустимый диапазон:
								{#if min !== undefined}{min}{:else}—{/if}
								до
								{#if max !== undefined}{max}{:else}—{/if}
								{#if isOutOfBounds[i]?.belowMin}{/if}
							</span>
						{/if}
					</div>
				{/each}
			</div>
			<div class="divider my-2"></div>
			<div class="flex flex-wrap justify-end gap-2">
				<button
					class="btn btn-primary inline-flex flex-none items-center"
					onclick={() => {
						onSave(draftValues);
						isEditing = false;
					}}
				>
					<Check class="mr-2 h-5 w-5" />
					<span>Сохранить</span>
				</button>
				<button
					class="btn btn-warning text-warning-content inline-flex flex-none items-center"
					onclick={() => {
						draftValues = [...initialValues];
						isEditing = false;
					}}
				>
					<Cancel class="mr-2 h-5 w-5" />
					<span>Отменить</span>
				</button>
			</div>
		</div>
	</div>
{/if}

<div class="flex gap-2 items-center">
	{#each initialValues as { name, value, unit }}
		<div class="flex items-baseline gap-1">
			{#if showValue}
				<!-- Добавляем условие отображения -->
				<span>{value}</span>
			{/if}
			{#if unit}
				<span class="text-sm opacity-75">{unit}</span>
			{/if}
		</div>
	{/each}
	<button
		class="text-blue-500 hover:text-blue-700 flex-shrink-0"
		onclick={() => (isEditing = true)}
	>
		<Pencil />
	</button>
</div>

<style>
	/* Анимация появления оверлея */
	.fade-in {
		animation: fadeIn 0.2s ease-out;
	}

	@keyframes fadeIn {
		from {
			opacity: 0;
		}
		to {
			opacity: 1;
		}
	}
</style>
