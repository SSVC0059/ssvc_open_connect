<script lang="ts">
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Portal from 'svelte-portal';

	let {
		show,
		currentValue,
		onSave,
		onClose
	}: {
		show: boolean;
		currentValue: number;
		onSave: (thresholds: {
			min: number;
			dangerous: number;
			critical: number;
		}) => void;
		onClose: () => void;
	} = $props();

	let dangerousOffset = $state(0);
	let criticalOffset = $state(0);
	let offsetUnit = $state('Pa');

	const units = {
		Pa: 1,
		kPa: 1000,
		hPa: 100,
		bar: 100000,
		mbar: 100,
		atm: 101325,
		mmhg: 133.322
	};

	function convertToPa(value: number, unit: keyof typeof units): number {
		return value * units[unit];
	}

	let thresholds = $derived.by(() => {
		const dangerousPa = convertToPa(dangerousOffset, offsetUnit as keyof typeof units);
		const criticalPa = convertToPa(criticalOffset, offsetUnit as keyof typeof units);

		const min = currentValue;
		const dangerous = min + dangerousPa / units.mmhg; // Конвертируем обратно в ммРс
		const critical = min + criticalPa / units.mmhg;

		return { min, dangerous, critical };
	});

	function handleSave() {
		onSave({
			min: parseFloat(thresholds.min.toFixed(2)),
			dangerous: parseFloat(thresholds.dangerous.toFixed(2)),
			critical: parseFloat(thresholds.critical.toFixed(2))
		});
	}

	$effect(() => {
		if (!show) return;

		const handleKeydown = (event: KeyboardEvent) => {
			if (event.key === 'Escape') {
				onClose();
			}
		};

		window.addEventListener('keydown', handleKeydown);

		return () => {
			window.removeEventListener('keydown', handleKeydown);
		};
	});
</script>

{#if show}
	<Portal>
		<div class="modal-overlay fade-in" onclick={onClose} onkeydown={() => {}} role="button" tabindex="0"></div>

		<div role="dialog" class="modal-dialog" transition:fly={{ y: 50 }} use:focusTrap>
			<div class="modal-content">
				<h2 class="modal-title">Авторасчет порогов</h2>
				<div class="modal-divider"></div>
				<div class="modal-body">
					<p class="info-text info-text--centered info-text--mb">
						Текущее значение <strong>{currentValue.toFixed(2)} ммРс</strong> будет установлено как
						минимальный порог.
					</p>

					<div class="input-group">
						<label class="input-label" for="dangerous-offset">Отклонение "Предупреждение"</label>
						<div class="input-wrapper">
							<input
								id="dangerous-offset"
								type="number"
								bind:value={dangerousOffset}
								class="input-field"
							/>
						</div>
					</div>

					<div class="input-group">
						<label class="input-label" for="critical-offset">Отклонение "Критический"</label>
						<div class="input-wrapper">
							<input
								id="critical-offset"
								type="number"
								bind:value={criticalOffset}
								class="input-field"
							/>
						</div>
					</div>

					<div class="input-group">
						<label class="input-label" for="offset-unit">Единица измерения</label>
						<select id="offset-unit" bind:value={offsetUnit} class="input-field">
							{#each Object.keys(units) as unit}
								<option value={unit}>{unit}</option>
							{/each}
						</select>
					</div>

					<div class="calculated-thresholds info-text info-text--centered info-text--mt">
						<p>Предупреждение: <strong>{thresholds.dangerous.toFixed(2)} ммРс</strong></p>
						<p>Критический: <strong>{thresholds.critical.toFixed(2)} ммРс</strong></p>
					</div>
				</div>
				<div class="modal-divider"></div>
				<div class="modal-actions">
					<button class="btn btn-primary" onclick={handleSave}>Сохранить</button>
					<button class="btn btn-back" onclick={onClose}>Отмена</button>
				</div>
			</div>
		</div>
	</Portal>
{/if}

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

	.info-text--centered {
		text-align: center;
	}
	.info-text--mb {
		margin-bottom: 1rem;
	}
	.info-text--mt {
		margin-top: 1rem;
	}
</style>
