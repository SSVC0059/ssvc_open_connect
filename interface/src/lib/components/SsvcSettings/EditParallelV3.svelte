<script lang="ts">
	import Pencil from '~icons/tabler/pencil';
	import { focusTrap } from '$lib/actions/focusTrap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

	import Portal from 'svelte-portal';

	type ParallelPoint = [number, number, number];

	let {
		points: initialPoints,
		onSave,
		children
	} = $props<{
		points: Array<[number, number, number]>; // [temperature, valveTime, periodicity]
		onSave: (points: ParallelPoint[]) => void;
		children?: any;
	}>();

	let isEditing = $state(false);
	let draftPoints = $state<Array<[number, number, number]>>([]);

	$effect(() => {
		if (!isEditing) return;

		const originalOverflow = document.body.style.overflow;
		document.body.style.overflow = 'hidden';

		return () => {
			document.body.style.overflow = originalOverflow;
		};
	});

	function openModal() {
		isEditing = true;
		// Сортируем точки по температуре при открытии
		draftPoints = [...initialPoints].sort((a, b) => a[0] - b[0]);
	}

	// Обработчик сохранения
	function handleSave() {
		onSave([...draftPoints].sort((a, b) => a[0] - b[0]));
		isEditing = false;
	}

	// Обработчик отмены
	function handleCancel() {
		draftPoints = [...initialPoints];
		isEditing = false;
	}

	// Обработчик изменения значений
	function handlePointChange(pointIndex: number, fieldIndex: number, event: Event) {
		const target = event.target as HTMLInputElement;
		const newValue = parseFloat(target.value);

		if (!isNaN(newValue)) {
			const newPoints = [...draftPoints];
			const newPoint = [...newPoints[pointIndex]] as [number, number, number];
			newPoint[fieldIndex] = newValue;
			newPoints[pointIndex] = newPoint;
			draftPoints = newPoints;
		}
	}

	// Названия полей для отображения
	const fieldUnits = ['°С', 'Сек', 'Сек'];

	// Шаги для полей
	const fieldSteps = [1, 0.1, 1]; // Температура, Время клапана, Периодичность
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
			<div
				class="modal-content"
			>
				<h2 class="modal-title">
					Редактирование точек параллельного отбора
				</h2>
				<div class="modal-divider"></div>
				<div class="modal-body">
					<div class="compact-points">
						<div class="points-header">
							<span class="point-label">Температура</span>
							<span class="point-label">Время открытия клапана</span>
							<span class="point-label">Периодичность</span>
						</div>
						{#each draftPoints as point, pointIndex (pointIndex)}
							<div class="point-row">
								{#each [0, 1, 2] as fieldIndex}
									<div class="point-value">
										<div class="point-input">
											<input
												type="number"
												max="1000"
												value={point[fieldIndex]}
												oninput={(e) => handlePointChange(pointIndex, fieldIndex, e)}
												step={fieldSteps[fieldIndex]}
											/>
										</div>
										<div class="point-info" >
											Шаг: {fieldSteps[fieldIndex]} {fieldUnits[fieldIndex]}
										</div>
									</div>
								{/each}
							</div>
						{/each}
					</div>
				</div>

				<div class="divider my-2"></div>
				<div class="flex flex-wrap justify-end gap-2">
					<button
						class="btn btn-primary inline-flex flex-none items-center"
						onclick={handleSave}
					>
						<Check class="mr-2 h-5 w-5" />
						<span>Сохранить</span>
					</button>
					<button
						class="btn btn-warning text-warning-content inline-flex flex-none items-center"
						onclick={handleCancel}
					>
						<Cancel class="mr-2 h-5 w-5" />
						<span>Отменить</span>
					</button>
				</div>
			</div>
		</div>
	</Portal>

{/if}

<div class="flex gap-2 items-center">
	<!-- Способ 1: Просто оборачиваем children в div с обработчиком -->
	{#if children}
		<button onclick={openModal}>
			{@render children()}
		</button>
	{:else}
		<button
			class="text-blue-500 hover:text-blue-700 flex-shrink-0"
			onclick={openModal}
		>
			<Pencil class="w-10 h-10"/>
		</button>
	{/if}
</div>

<style lang="scss">
	@use "$lib/styles/base/mixins" as m;

	/* ===== ОСНОВНЫЕ СТИЛИ МОДАЛЬНЫХ ОКОН ===== */
	:global(.modal-overlay) {
		position: fixed;
		inset: 0;
		z-index: var(--z-modal-backdrop);
		background: color-mix(in srgb, var(--black) 50%, transparent);
		backdrop-filter: blur(4px);

		&.fade-in {
			animation: fadeIn 0.2s ease-out;
		}
	}

	:global(.modal-dialog) {
		position: fixed;
		inset: 0;
		z-index: var(--z-modal);
		display: grid;
		place-items: center;
		pointer-events: none;
	}

	:global(.modal-content) {
		background: var(--white);
		border-radius: var(--border-radius);
		box-shadow: var(--glass-shadow);
		pointer-events: auto;
		display: flex;
		flex-direction: column;
		justify-content: space-between;
		padding: 1rem;
		max-height: 100vh;
		min-width: fit-content;
		max-width: min(28rem, 90vw);
		border: var(--glass-border);
		background: var(--glass-background);
		backdrop-filter: blur(8px);
	}

	:global(.modal-title) {
		color: inherit;
		text-align: center;
		font-size: 1.5rem;
		font-weight: 700;
		line-height: 2rem;
		margin: 0;
	}

	:global(.modal-divider) {
		height: 1px;
		background-color: currentColor;
		opacity: 0.2;
		margin: 0.5rem 0;
		border: none;
	}

	:global(.modal-body) {
		overflow-y: auto;
		padding: 1rem;
		display: flex;
		flex-direction: column;
		gap: 1rem;

		scrollbar-width: thin;
		scrollbar-color: var(--primary-300) transparent;

		&::-webkit-scrollbar {
			width: 4px;
		}

		&::-webkit-scrollbar-track {
			background: transparent;
		}

		&::-webkit-scrollbar-thumb {
			background: var(--primary-300);
			border-radius: 2px;
		}
	}

	:global(.modal-actions) {
		display: flex;
		flex-wrap: wrap;
		justify-content: flex-end;
		gap: 0.5rem;
	}

</style>