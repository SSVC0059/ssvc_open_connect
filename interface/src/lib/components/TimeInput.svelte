<script lang="ts">
	let {
		value = $bindable(),
		step = 1,
		min = 0,
		max = 86399 // 23:59:59 in seconds
	} = $props<{
		value: number; // value in seconds
		step?: number;
		min?: number;
		max?: number;
	}>();

	function formatTime(totalSeconds: number): string {
		const hours = Math.floor(totalSeconds / 3600).toString().padStart(2, '0');
		const minutes = Math.floor((totalSeconds % 3600) / 60).toString().padStart(2, '0');
		const seconds = (totalSeconds % 60).toString().padStart(2, '0');
		return `${hours}:${minutes}:${seconds}`;
	}

	let formattedTime = $derived(formatTime(value));

	function parseTime(timeStr: string): number {
		const parts = timeStr.split(':').map(Number);
		if (parts.length === 3 && parts.every(p => !isNaN(p))) {
			return parts[0] * 3600 + parts[1] * 60 + parts[2];
		}
		return value; // return current value if parsing fails
	}

	function handleInput(event: Event) {
		const target = event.target as HTMLInputElement;
		const parsedValue = parseTime(target.value);
		if (parsedValue >= min && parsedValue <= max) {
			value = parsedValue;
		} else {
			// Reset input to last valid value if out of bounds
			target.value = formattedTime;
		}
	}

	function increment() {
		const newValue = value + step;
		if (newValue <= max) {
			value = newValue;
		}
	}

	function decrement() {
		const newValue = value - step;
		if (newValue >= min) {
			value = newValue;
		}
	}
</script>

<div class="number-input-wrapper">
	<button class="control-button" onclick={decrement}>-</button>
	<div class="input-unit-container">
		<input
			type="text"
			bind:value={formattedTime}
			onchange={handleInput}
			class="input-field"
			placeholder="HH:MM:SS"
		/>
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
		padding: 0 8px;
		font-size: 16px;
		font-family: monospace; /* Helps with character spacing */
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
