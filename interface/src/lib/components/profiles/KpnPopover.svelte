<script lang="ts">
	import AnalyticsValue from './AnalyticsValue.svelte';
	import HelpCircle from '~icons/tabler/help-circle';

	let { id, rangeKey, profile }: { id: string; rangeKey: 'heads' | 'late_heads' | 'hearts' | 'tails'; profile: any } =
		$props();

	const KPN_HELP = {
		title: 'Коэффициент паровой нагрузки (КПН)',
		description:
			'Отношение общего объема генерируемого пара к скорости его отбора. Отражает реальную разделительную способность колонны.',
		footer: 'Чем выше число, тем больше флегмы возвращается в колонну и тем чище продукт.',
		ranges: {
			heads: 'Головы: 50.0 – 80.0 (макс. очистка)',
			late_heads: 'Подголовники: ~50.0 – 60.0',
			hearts: 'Тело: 2.0 – 5.0 (оптимальный баланс)',
			tails: 'Хвосты: < 1.0 (режим прямотока)'
		}
	};

	const tooltipText = $derived(
		`${KPN_HELP.title}\n${KPN_HELP.description}\n${KPN_HELP.ranges[rangeKey]}\n${KPN_HELP.footer}`
	);
</script>

<div class="tooltip tooltip-left max-w-xs whitespace-pre-line" data-tip={tooltipText}>
	<div class="flex items-center gap-2 cursor-help" id={id}>
		<AnalyticsValue label="Коэффициент паровой нагрузки" value={profile.analytics.refluxRatio[rangeKey]} />
		<HelpCircle class="size-8 text-base-content/50 shrink-0" />
	</div>
</div>
