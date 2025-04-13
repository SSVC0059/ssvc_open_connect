<script lang="ts">
	let {
		title,
		labels,
		values: initialValues,
		units,
		steps,
		onClose,
		onSave,
		step
	} = $props<{
		title: string;
		labels: string[];
		values: number[];
		units: string | string[];
		steps?: number | number[];
		onClose: () => void;
		onSave: (values: number[]) => void;
	}>();

	let localValues = $state([...initialValues]);
	let modalRef: HTMLDivElement;

	const isUnitsArray = Array.isArray(units);
	const isStepsArray = Array.isArray(steps);

	function getUnit(index: number): string {
		return isUnitsArray ? units[index] : units;
	}

	function getStep(index: number): number {
		if (isStepsArray) {
			return steps[index] ?? 1;
		}
		return steps ?? 1;
	}

	function updateValue(index: number, value: number) {
		localValues[index] = value;
	}

	function handleKeyDown(e: KeyboardEvent) {
		if (e.key === 'Escape') {
			onClose();
		}
	}

	function handleBackdropClick(e: MouseEvent) {
		if (e.target === modalRef) {
			onClose();
		}
	}

	$effect(() => {
		modalRef?.focus();
	});
</script>

<!-- Основной контейнер модального окна -->
<div
	class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center"
	role="dialog"
	aria-modal="true"
	aria-labelledby="modal-title"
	bind:this={modalRef}
	tabindex="-1"
	onkeydown={handleKeyDown}
	onclick={handleBackdropClick}
>
	<!-- Контент модального окна -->
	<div class="bg-white p-6 rounded-lg w-96 dark:bg-gray-800" role="document">
		<!-- Заголовок -->
		<h2 id="modal-title" class="text-xl mb-4 font-semibold dark:text-white">
			{title}
		</h2>

		<!-- Поля ввода -->
		{#each localValues as value, i}
			<div class="flex items-center gap-2 mb-3">
				<label for={`input-${i}`} class="w-32 dark:text-gray-200">
					{labels[i]}:
				</label>

				<input
					type="number"
					bind:value={localValues[i]}
					class="w-24 px-2 py-1 border rounded dark:bg-gray-700 dark:border-gray-600 dark:text-white"
					min="0"
					step={getStep(i)}
					aria-label={`Значение для ${labels[i]}`}
				/>

				<span class="dark:text-gray-300">
					{getUnit(i)}
				</span>
			</div>
		{/each}

		<!-- Кнопки действий -->
		<div class="flex justify-end gap-2 mt-6">
			<button
				onclick={onClose}
				class="px-4 py-2 bg-gray-200 rounded hover:bg-gray-300 dark:bg-gray-600 dark:hover:bg-gray-500 dark:text-white"
				type="button"
			>
				Отмена
			</button>

			<button
				onclick={() => onSave(localValues)}
				class="px-4 py-2 bg-blue-500 text-white rounded hover:bg-blue-600 dark:bg-blue-600 dark:hover:bg-blue-700"
				type="button"
			>
				Сохранить
			</button>
		</div>
	</div>
</div>
