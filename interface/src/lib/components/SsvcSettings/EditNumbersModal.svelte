<script lang="ts">
	import Pencil from '~icons/tabler/pencil';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

	import Portal from 'svelte-portal';

	let {
		values: initialValues,
		onSave,
		showValue = true,
		children
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
		children?: any;
	}>();

	let isEditing = $state(false);
	let draftValues = $state([...initialValues]);

	// Тип для хранения информации о выходе за границы
	type OutOfBounds = {
		belowMin: boolean;
		aboveMax: boolean;
	};

	// Оптимизированный эффект для управления overflow
	$effect(() => {
		if (!isEditing) return;

		const originalOverflow = document.body.style.overflow;
		document.body.style.overflow = 'hidden';

		return () => {
			document.body.style.overflow = originalOverflow;
		};
	});

	// Ленивая функция валидации вместо $derived
	function isValueOutOfBounds(value: number, min?: number, max?: number): OutOfBounds {
		return {
			belowMin: min !== undefined && value < min,
			aboveMax: max !== undefined && value > max
		};
	}

	// Функция для проверки всех полей перед сохранением
	function validateAllFields(): boolean {
		return draftValues.every((item) => {
			const bounds = isValueOutOfBounds(item.value, item.min, item.max);
			return !bounds.belowMin && !bounds.aboveMax;
		});
	}

	// Функция для открытия модального окна
	function openModal() {
		isEditing = true;
		draftValues = [...initialValues]; // Сбрасываем значения при открытии
	}

	// Обработчик сохранения
	function handleSave() {
		if (validateAllFields()) {
			onSave(draftValues);
			isEditing = false;
		}
	}

	// Обработчик отмены
	function handleCancel() {
		draftValues = [...initialValues];
		isEditing = false;
	}

	// Обработчик изменения инпута
	function handleInputChange(index: number, event: Event) {
		const target = event.target as HTMLInputElement;
		const newValue = parseFloat(target.value);
		if (!isNaN(newValue)) {
			draftValues[index].value = newValue;
		}
	}
</script>

{#if isEditing}
	<Portal>
			<div class="modal-overlay fade-in"></div>

			<div
				role="dialog"
				class="modal-dialog"
				transition:fly={{ y: 50 }}
				use:focusTrap
			>
				<div class="modal-content">
					<h2 class="modal-title">
						{initialValues.length > 1 ? 'Изменение переменных' : 'Изменение переменной'}
					</h2>
					<div class="modal-divider"></div>
					<div class="modal-body">
						{#each draftValues as { name, value, step, unit, precision, min, max }, i (name + i)}
							{@const bounds = isValueOutOfBounds(value, min, max)}
							<div class="input-group">
								<label class="input-label" for={`input-${i}`}>{name}</label>
								<div class="input-wrapper">
									<input
										id={`input-${i}`}
										type="number"
										value={value}
										oninput={(e) => handleInputChange(i, e)}
										class="input-field {bounds?.belowMin || bounds?.aboveMax ? 'input-error' : ''}"
										step={step ?? 1}
										{min}
										{max}
									/>
									{#if unit}
										<span class="input-unit">{unit}</span>
									{/if}
								</div>
								{#if precision !== undefined}
									<div class="precision-info">Точность: {precision} знак после запятой</div>
								{/if}
								{#if min !== undefined || max !== undefined}
									<span class="range-info {bounds?.belowMin || bounds?.aboveMax ? 'range-error' : ''}">
										Допустимый диапазон:
										{#if min !== undefined}{min}{:else}—{/if}
										до
										{#if max !== undefined}{max}{:else}—{/if}
										{#if bounds?.belowMin}
											<span class="error-message">(ниже минимума)</span>
										{:else if bounds?.aboveMax}
											<span class="error-message">(выше максимума)</span>
										{/if}
									</span>
								{/if}
							</div>
						{/each}
					</div>
					<div class="modal-divider"></div>
					<div class="modal-actions">
						<button
							class="btn btn-primary"
							onclick={handleSave}
							disabled={!validateAllFields()}
						>
							<Check class="btn-icon" />
							<span>Сохранить</span>
						</button>
						<button
							class="btn btn-warning"
							onclick={handleCancel}
						>
							<Cancel class="btn-icon" />
							<span>Отменить</span>
						</button>
					</div>
				</div>
			</div>
	</Portal>
{/if}

<div class="values-container">
	{#each initialValues as { name, value, unit }}
		<div class="value-item">
			{#if showValue}
				<span class="value-text">{value}</span>
			{/if}
			{#if unit}
				<span class="value-unit">{unit}</span>
			{/if}
		</div>
	{/each}

	{#if children}
		<button class="edit-button" onclick={openModal}>
			{@render children()}
		</button>
	{:else}
		<button class="edit-button" onclick={openModal}>
			<Pencil />
		</button>
	{/if}
</div>
