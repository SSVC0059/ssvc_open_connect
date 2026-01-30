<script lang="ts">
	// Props
	let {
		label = '',
		openTime = $bindable(),
		period = $bindable(),
		initialOpenTime = 0,
		initialPeriod = 0,
		readonly = false
	} = $props();

	// Определяем, были ли значения изменены
	const isPeriodModified = $derived(period !== initialPeriod);
	const isOpenTimeModified = $derived(openTime.toFixed(1) !== initialOpenTime.toFixed(1));
	const isModified = $derived(isPeriodModified || isOpenTimeModified);

	// Сброс к исходным значениям
	function resetToInitial() {
		openTime = initialOpenTime;
		period = initialPeriod;
	}
</script>

<div class="ssvc-item" class:modified={isModified}>
	<span class="ssvc-label">{label}:</span>
	<div class="controls">
		<div class="ssvc-inputs">
			<div class="input-wrapper">
				<input
					type="number"
					class="input-field ssvc-input"
					class:modified={isPeriodModified}
					bind:value={period}
					step="1"
					{readonly}
					title={isPeriodModified ? `Исходное значение: ${initialPeriod}` : ''}
				/>
				{#if isPeriodModified}
					<span class="initial-value">было: {initialPeriod}</span>
				{/if}
			</div>
			<span class="separator">,</span>
			<div class="input-wrapper">
				<input
					type="number"
					class="input-field ssvc-input"
					class:modified={isOpenTimeModified}
					bind:value={openTime}
					step="0.1"
					{readonly}
					title={isOpenTimeModified ? `Исходное значение: ${initialOpenTime.toFixed(1)}` : ''}
				/>
				{#if isOpenTimeModified}
					<span class="initial-value">было: {initialOpenTime.toFixed(1)}</span>
				{/if}
			</div>
		</div>
		<button
			class="reset-btn"
			class:visible={isModified}
			onclick={resetToInitial}
			title="Сбросить к расчетному значению"
			tabindex={isModified ? 0 : -1}
		>
			↺
		</button>
	</div>
</div>

<style lang="scss">
	.ssvc-item {
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 0.35rem 0.75rem;
		border-radius: var(--border-radius);
		font-family: var(--font-mono);
		font-size: 0.875rem;
		transition: background-color 0.2s;
		border: 1px solid transparent;
		min-height: 54px;

		&.modified {
			background-color: var(--orange-50);
		}
	}

	.ssvc-label {
		color: var(--text-muted-color);
		flex-shrink: 0;
	}

	.controls {
		display: flex;
		align-items: center;
		gap: 0.5rem;
	}

	.ssvc-inputs {
		display: flex;
		align-items: flex-start;
		gap: 0.35rem;
		font-weight: 600;
	}

	.input-wrapper {
		display: flex;
		flex-direction: column;
		align-items: center;
		gap: 2px;
	}

	.ssvc-input {
		width: 4.5rem;
		padding: 0.25rem 0.4rem;
		text-align: center;
		font-family: inherit;

		&.modified {
			border-color: var(--orange-400);
			background-color: var(--orange-100);
		}
	}

	.initial-value {
		font-size: 0.7rem;
		color: var(--text-muted-color);
		font-weight: 400;
		line-height: 1;
	}

	.separator {
		color: var(--text-muted-color);
		padding-top: 0.25rem;
	}

	.reset-btn {
		background: none;
		border: none;
		cursor: pointer;
		color: var(--orange-600);
		font-size: 1.1rem;
		padding: 0 0.4rem;
		transition: all 0.2s ease-in-out;
		visibility: hidden; // Скрываем, но оставляем в потоке
		opacity: 0;

		&.visible {
			visibility: visible; // Делаем видимой
			opacity: 1;
		}

		&:hover:not(.visible) {
			cursor: default;
		}

		&:hover.visible {
			color: var(--orange-800);
		}
	}
</style>
