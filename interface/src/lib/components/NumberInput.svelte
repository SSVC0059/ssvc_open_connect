<script lang="ts">
	let {
		value = $bindable(),
		step = 1,
		min = -Infinity,
		max = Infinity,
		unit = '',
		placeholder = ''
	} = $props<{
		value: number;
		step?: number;
		min?: number;
		max?: number;
		unit?: string;
		placeholder?: string;
	}>();

	function increment() {
		const newValue = Number((value + step).toPrecision(15));
		if (newValue <= max) {
			value = newValue;
		}
	}

	function decrement() {
		const newValue = Number((value - step).toPrecision(15));
		if (newValue >= min) {
			value = newValue;
		}
	}
</script>

<div class="number-input-wrapper">
	<button class="control-button" onclick={decrement}>-</button>
	<div class="input-unit-container">
		<input
			bind:value
			type="number"
			{min}
			{max}
			{step}
			{placeholder}
			class="input-field"
		/>
		{#if unit}
			<span class="unit-label">{unit}</span>
		{/if}
	</div>
	<button class="control-button" onclick={increment}>+</button>
</div>

<style>
	.number-input-wrapper {
		display: flex;
		align-items: center;
		gap: 4px;
		width: 100%;
		max-width: 180px;
		margin: 0 auto;
	}

	.input-unit-container {
		display: flex;
		align-items: center;
		flex-grow: 1;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		background-color: var(--input-bg);
		height: 36px;
		overflow: hidden;
	}

	.input-field {
		flex-grow: 1;
		min-width: 0;
		width: 100%;
		height: 100%;
		box-sizing: border-box;
		border: none;
		background: transparent;
		color: var(--text-color);
		text-align: center;
		padding: 0 2px;
		font-size: 16px;
	}

	/* Hide default number input arrows */
	.input-field::-webkit-outer-spin-button,
	.input-field::-webkit-inner-spin-button {
		-webkit-appearance: none;
		margin: 0;
	}
	.input-field[type='number'] {
		-moz-appearance: textfield;
		appearance: textfield;
	}

	.unit-label {
		padding-right: 10px;
		color: #888;
		font-size: 14px;
		white-space: nowrap;
	}

	.control-button {
		width: 36px;
		height: 36px;
		border: 1px solid var(--border-color);
		background-color: var(--secondary-bg);
		border-radius: 4px;
		font-size: 20px;
		font-weight: bold;
		color: var(--text-color);
		cursor: pointer;
		flex-shrink: 0;
		display: flex;
		align-items: center;
		justify-content: center;
		line-height: 1;
	}

	.control-button:hover {
		background-color: var(--hover-bg);
	}
</style>
