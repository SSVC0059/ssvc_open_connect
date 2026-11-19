<script lang="ts">
	import {
		FORMULA_END_TEMP,
		formulaStartTempForMode,
		minHysteresisFor,
		predecState,
		protectionThresholds
	} from '$lib/actions/rectificationRules';
	import type { SsvcSettings } from '$lib/types/ssvc';

	const {
		settings,
		onSave,
		tp1Target = null,
		predecAvailable = true
	} = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: unknown) => void;
		/** Текущая целевая ТД1 из телеметрии; null/undefined — данных нет. */
		tp1Target?: number | null;
		/** Подтверждена ли поддержка предекремента прошивкой (и API 1.9). */
		predecAvailable?: boolean;
	}>();

	/** 0.12 → «0,12»; 92 → «92»; 78.65 → «78,65». */
	function formatNumber(value: number): string {
		return value.toFixed(2).replace(/\.?0+$/, '').replace('.', ',');
	}

	function formatTemp(value: number): string {
		return `${formatNumber(value)} °C`;
	}

	const state = $derived(predecState(settings.predec, settings.hyst));
	const minHyst = $derived(minHysteresisFor(settings.predec));
	const thresholds = $derived(protectionThresholds(tp1Target, settings.hyst, settings.predec));
	const formulaStart = $derived(
		formulaStartTempForMode(settings.formula, settings.formula_start_temp)
	);
	const auto92WithoutPredec = $derived(settings.formula === 2 && settings.predec === 0);

	const stopValue = $derived(
		thresholds ? formatTemp(thresholds.stopAt) : `Т + ${formatNumber(settings.hyst)} °C`
	);
	const stopHint = $derived(
		thresholds ? 'Т отбора + гистерезис' : 'точная Т отбора появится в телеметрии'
	);

	const predecValue = $derived.by(() => {
		if (settings.predec === 0) return 'Выкл';
		if (state === 'hysteresis-too-small') return 'Не сработает';
		if (thresholds?.predecAt != null) return formatTemp(thresholds.predecAt);
		return `стоп − ${formatNumber(settings.predec / 100)} °C`;
	});

	const predecHint = $derived.by(() => {
		if (settings.predec === 0) return 'скорость заранее не снижается';
		if (state === 'hysteresis-too-small') {
			return `нужен гистерезис ≥ ${formatNumber(minHyst ?? 0)} °C`;
		}
		return `за ${formatNumber(settings.predec / 100)} °C до остановки`;
	});

	const formulaValue = $derived(
		settings.formula === 0
			? 'Выкл'
			: `${formatNumber(formulaStart)} → ${FORMULA_END_TEMP} °C`
	);

	const formulaHint = $derived.by(() => {
		if (settings.formula === 2) return 'полная скорость до первого предекремента';
		if (settings.formula === 1) return 'снижение с начала формулы';
		return 'снижение по ТД2 выключено';
	});

	// Правки — только по явному нажатию и только одного поля: пара predec + hyst
	// одним запросом не отправляется, чтобы не получить частичное применение.
	function raiseHysteresis() {
		if (minHyst !== null) onSave('hyst', minHyst);
	}

	function enablePredec() {
		onSave('predec', 7);
	}
</script>

<section class="protection-card">
	<div class="protection-head">
		<h3 class="protection-title">Защита от стопа</h3>
		<a
			class="protection-link"
			href="https://smartmodule.ru/portfolio/ssvc0059_v2/auto92/"
			target="_blank"
			rel="noopener"
		>
			Как это работает
		</a>
	</div>

	<div class="protection-metrics">
		<div class="protection-metric">
			<span class="protection-metric-label">Остановка отбора</span>
			<span class="protection-metric-value">{stopValue}</span>
			<span class="protection-metric-hint">{stopHint}</span>
		</div>

		<div class="protection-metric">
			<span class="protection-metric-label">Предекремент</span>
			<span class="protection-metric-value">{predecValue}</span>
			<span class="protection-metric-hint">{predecHint}</span>
		</div>

		<div class="protection-metric">
			<span class="protection-metric-label">Формула</span>
			<span class="protection-metric-value">{formulaValue}</span>
			<span class="protection-metric-hint">{formulaHint}</span>
		</div>
	</div>

	{#if state === 'hysteresis-too-small' && minHyst !== null}
		<div class="protection-alert">
			<p>
				Предекремент не сработает: гистерезис {formatTemp(settings.hyst)} меньше минимального
				{formatTemp(minHyst)} — контроллер отключит предекремент.
			</p>
			{#if predecAvailable}
				<button type="button" class="btn btn-warning btn-sm" onclick={raiseHysteresis}>
					Поднять гистерезис до {formatTemp(minHyst)}
				</button>
			{/if}
		</div>
	{/if}

	{#if auto92WithoutPredec}
		<div class="protection-alert">
			<p>
				Авто 92+ не сработает без предекремента: режим включается после первого
				предекремента.
			</p>
			{#if predecAvailable}
				<button type="button" class="btn btn-warning btn-sm" onclick={enablePredec}>
					Включить предекремент 0,07
				</button>
			{/if}
		</div>
	{/if}
</section>

<style lang="scss">
	.protection-card {
		padding: 1rem 1.25rem;
		border: 1px solid color-mix(in srgb, var(--color-base-content) 12%, transparent);
		border-radius: 1rem;
		background: var(--color-base-100);
	}

	.protection-head {
		display: flex;
		align-items: baseline;
		justify-content: space-between;
		gap: 1rem;
		margin-bottom: 0.85rem;
	}

	.protection-title {
		margin: 0;
		font-size: 1.125rem;
		font-weight: 600;
		color: var(--color-base-content);
	}

	.protection-link {
		flex-shrink: 0;
		font-size: 0.8125rem;
		color: var(--color-base-content);
		opacity: 0.8;
		text-decoration: underline;
		text-underline-offset: 0.15em;
	}

	.protection-metrics {
		display: grid;
		grid-template-columns: repeat(3, minmax(0, 1fr));
		gap: 0.75rem;
	}

	.protection-metric {
		display: flex;
		flex-direction: column;
		gap: 0.15rem;
		min-width: 0;
		padding: 0.65rem 0.8rem;
		border: 1px solid color-mix(in srgb, var(--color-base-content) 8%, transparent);
		border-radius: 0.65rem;
		background: color-mix(in srgb, var(--color-base-200) 65%, transparent);
	}

	.protection-metric-label {
		font-size: 0.75rem;
		font-weight: 600;
		letter-spacing: 0.02em;
		text-transform: uppercase;
		color: var(--color-base-content);
		opacity: 0.65;
	}

	.protection-metric-value {
		font-size: 1.05rem;
		font-weight: 600;
		line-height: 1.3;
		color: var(--color-base-content);
		overflow-wrap: anywhere;
	}

	.protection-metric-hint {
		font-size: 0.75rem;
		line-height: 1.35;
		color: var(--color-base-content);
		opacity: 0.7;
	}

	.protection-alert {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.75rem;
		margin-top: 0.75rem;
		padding: 0.6rem 0.8rem;
		border-left: 3px solid var(--yellow-500);
		border-radius: 0.5rem;
		background: color-mix(in srgb, var(--yellow-500) 12%, transparent);

		p {
			margin: 0;
			font-size: 0.8125rem;
			line-height: 1.4;
		}

		.btn {
			flex-shrink: 0;
		}
	}

	@media (max-width: 767px) {
		.protection-card {
			padding: 0.85rem 0.95rem;
		}

		.protection-head {
			flex-direction: column;
			align-items: flex-start;
			gap: 0.25rem;
		}

		.protection-metrics {
			grid-template-columns: 1fr;
		}

		.protection-alert {
			flex-direction: column;
			align-items: flex-start;
		}
	}
</style>
