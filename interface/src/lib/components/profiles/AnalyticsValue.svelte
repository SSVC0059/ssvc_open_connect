<script lang="ts">
	let {
		label,
		value,
		initialValue = null,
		isLarge = false
	} = $props();

	const isModified = $derived(initialValue !== null && value !== initialValue);
</script>

<div class="analytics-item" class:modified={isModified} title={isModified ? `Исходное: ${initialValue}` : ''}>
	<span class="analytics-label">{label}</span>
	<div class="value-container">
		<span class="analytics-value" class:large={isLarge}>{value}</span>
		{#if isModified}
			<span class="initial-value">({initialValue})</span>
		{/if}
	</div>
</div>

<style lang="scss">
	.analytics-item {
		display: flex;
		flex-direction: column;
		background-color: var(--surface-alt-color);
		padding: 0.5rem 0.65rem; // Уменьшено
		border-radius: var(--border-radius);
		border: 1px solid transparent;
		transition: all 0.2s ease-in-out;

		&.modified {
			background-color: var(--orange-50);
			border-color: var(--orange-200);
		}
	}

	.analytics-label {
		font-size: 0.75rem; // Уменьшено
		color: var(--text-muted-color);
		margin-bottom: 0.15rem; // Уменьшено
	}

	.value-container {
		display: flex;
		align-items: baseline;
		gap: 0.4rem; // Уменьшено
	}

	.analytics-value {
		font-size: 1rem; // Уменьшено
		font-weight: 600;
		color: var(--primary-700);

		&.large {
			font-size: 1.15rem; // Уменьшено
		}
	}

	.initial-value {
		font-size: 0.75rem; // Уменьшено
		color: var(--text-muted-color);
		font-weight: 400;
		text-decoration: line-through;
	}
</style>
