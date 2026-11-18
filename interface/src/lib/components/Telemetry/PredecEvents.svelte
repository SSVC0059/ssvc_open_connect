<script lang="ts">
	import type { PredecEvent } from './predecEvents';
	import {
		predecVerdict,
		predecVerdictHint,
		type PredecVerdict
	} from '$lib/actions/rectificationRules';

	const { events }: { events: PredecEvent[] } = $props();

	// Короткая оценка для строки события; полный текст с причиной — в подсказке.
	const VERDICT_LABEL: Record<PredecVerdict, string> = {
		early: 'рано',
		normal: 'в норме 92–94 °C',
		late: 'поздно'
	};

	/** Температура с запятой и двумя знаками; пусто — прочерк. */
	function formatTemp(value?: number): string {
		if (typeof value !== 'number' || !Number.isFinite(value)) return '—';
		return value.toFixed(2).replace('.', ',');
	}
</script>

{#if events.length > 0}
	<div class="predec-events">
		{#each events as event (event.index)}
			{@const verdict = event.index === 1 ? predecVerdict(event.td2) : null}
			{@const summary = `Предекремент №${event.index}: ТД1 ${formatTemp(event.td1)} °C, ТД2 ${formatTemp(event.td2)} °C`}
			<div class="reading-item">
				<span class="reading-label">{summary}</span>
				<span class="reading-value"
					>{verdict
						? VERDICT_LABEL[verdict]
						: event.index === 1
							? 'нет данных'
							: '—'}</span
				>
			</div>
			{#if event.count > 1}
				<div class="predec-events-note">{event.count} срабатываний за интервал</div>
			{/if}
			{#if verdict}
				<div class="predec-events-hint">{predecVerdictHint(verdict)}</div>
			{/if}
		{/each}
		{#if events.length > 1}
			<p class="predec-events-caption">
				Оценка 92–94 °C относится к первому зафиксированному предекременту; для следующих показана
				только температура.
			</p>
		{/if}
	</div>
{:else}
	<p class="predec-events-empty">Пока не было</p>
{/if}

<style lang="scss">
	@use '$lib/styles/base/mixins' as *;

	.predec-events {
		display: flex;
		flex-direction: column;
		margin-top: 0.5rem;

		.reading-item {
			display: flex;
			flex-direction: column;
			align-items: flex-start;
			gap: 0.1rem;
			padding: 0.25rem 0;
			border-bottom: 1px dashed var(--primary-200);
		}

		.reading-label {
			font-size: 0.85rem;
			color: var(--glass-primary-800);
			@include dark-theme-color;
		}

		.reading-value {
			font-size: 0.9rem;
			font-weight: 600;
			color: var(--glass-primary-800);
			@include dark-theme-color;
		}

		.predec-events-note {
			font-size: 0.8rem;
			font-weight: 600;
			color: var(--orange-500);
		}

		.predec-events-hint {
			font-size: 0.8rem;
			color: var(--glass-secondary-700);
			@include dark-theme-color;
		}

		.predec-events-caption {
			margin: 0.4rem 0 0;
			font-size: 0.75rem;
			line-height: 1.35;
			color: var(--glass-secondary-600);
		}
	}

	.predec-events-empty {
		margin: 0.35rem 0 0;
		text-align: center;
		font-size: 0.85rem;
		color: var(--glass-secondary-600);
	}
</style>
