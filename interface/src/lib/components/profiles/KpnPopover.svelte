<script lang="ts">
    import AnalyticsValue from './AnalyticsValue.svelte';
    import { Popover } from 'flowbite-svelte';
    import { QuestionCircleOutline } from 'flowbite-svelte-icons';

    export let id: string;
    export let rangeKey: 'heads' | 'late_heads' | 'hearts' | 'tails';
    export let profile: any;

    const KPN_HELP = {
        title: 'Коэффициент паровой нагрузки (КПН)',
        description: 'Отношение общего объема генерируемого пара к скорости его отбора. Отражает реальную разделительную способность колонны.',
        footer: 'Чем выше число, тем больше флегмы возвращается в колонну и тем чище продукт.',
        ranges: {
            heads: 'Головы: 50.0 – 80.0 (макс. очистка)',
            late_heads: 'Подголовники: ~50.0 – 60.0',
            hearts: 'Тело: 2.0 – 5.0 (оптимальный баланс)',
            tails: 'Хвосты: < 1.0 (режим прямотока)'
        }
    };
</script>

<div class="flex items-center gap-2 cursor-help" id={id}>
    <AnalyticsValue label="Коэффициент паровой нагрузки" value={profile.analytics.refluxRatio[rangeKey]} />
    <QuestionCircleOutline class="w-4 h-4 text-gray-400" />
</div>

<Popover triggeredBy="#{id}" class="w-72 bg-white dark:bg-gray-800 text-sm font-light z-50">
    <div class="space-y-2 text-gray-900 dark:text-white">
        <h3 class="font-semibold">{KPN_HELP.title}</h3>
        <p>{KPN_HELP.description}</p>
        <ul class="list-disc pl-4 space-y-1">
            <li><strong>{KPN_HELP.ranges[rangeKey]}</strong></li>
        </ul>
        <p class="text-xs italic text-gray-500">{KPN_HELP.footer}</p>
    </div>
</Popover>