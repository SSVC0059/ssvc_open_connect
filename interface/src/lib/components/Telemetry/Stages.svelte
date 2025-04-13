<script lang="ts">
	import type { SsvcOpenConnectMessage } from '$lib/types/models';
	import { Breadcrumb, BreadcrumbItem, Button, Indicator, Popover } from 'flowbite-svelte';
	import { CheckCircleSolid } from 'flowbite-svelte-icons';
	import { slide } from 'svelte/transition';

	const { data } = $props<{
		data: SsvcOpenConnectMessage;
	}>();

	function getDescriptionStage(stage: string) {
		switch (stage) {
			case 'waiting':
				return 'Дежурный режим';
			case 'tp1_waiting':
				return 'Нагрев колонны';
			case 'delayed_start':
				return 'Отложенный старт';
			case 'heads':
				return 'Головы';
			case 'late_heads':
				return 'Подголовники';
			case 'hearts':
				return 'Тело';
			case 'tails':
				return 'Хвосты';
			default:
				return '';
		}
	}

	function getStageStatusClass(status: string) {
		switch (status) {
			case 'running':
				return 'bg-blue-500 text-white'; // Оживленный
			case 'paused':
				return 'bg-yellow-500 text-white'; // Приостановленный
			case 'finished':
				return 'bg-green-500 text-white'; // Завершенный
			case 'skipped':
				return 'bg-gray-500 text-white'; // Пропущенный
			default:
				return 'bg-gray-200 text-black'; // Неизвестный
		}
	}

	// Список этапов
	const stages = ['waiting', 'tp1_waiting', 'delayed_start', 'heads', 'late_heads', 'hearts'];

	// Функция для получения текущего этапа, предыдущего и следующего
	function getNeighbourStages(currentStage: string) {
		const currentIndex = stages.indexOf(currentStage);
		return {
			previous: stages[currentIndex - 1] || null,
			current: currentStage,
			next: stages[currentIndex + 1] || null
		};
	}

	// Находим текущий этап
	let currentStage = $state('waiting'); // Изначально 'heads

	// Значения, которые нужно отслеживать
	let previous = $state<string | null>(null);
	let current = $state<string | null>(null);
	let next = $state<string | null>(null);

	$effect(() => {
		const result = getNeighbourStages(data.status?.stage);
		previous = result.previous;
		current = result.current;
		next = result.next;
	});

	// Функция для вычисления процента завершенных этапов
	function getProgressPercentage() {
		const completedStages = Object.values(data.status.stages).filter(
			(status) => status === 'finished'
		).length;
		const totalStages = stages.length;
		return (completedStages / totalStages) * 100;
	}
</script>

<!-- Основной блок -->
{#if data.status}
	<div class="flex flex-col md:flex-row items-center justify-center space-x-4 p-4">
		<!-- Предыдущий этап -->
		{#if previous}
			<div class="flex items-center justify-center space-x-2 opacity-50 leading-relaxed">
				<span class="text-gray-500 text-sm">{getDescriptionStage(previous)}</span>
			</div>
		{/if}

		<!-- Текущий этап -->
		{#if current}
			<div class="flex items-center justify-center space-x-2 leading-relaxed">
				<span class="text-blue-500 text-2xl font-bold text-center"
					>{getDescriptionStage(current)}</span
				>
				<div
					class="w-6 h-6 border-t-2 border-blue-500 border-solid rounded-full animate-spin ml-3"
				></div>
			</div>
		{/if}

		<!-- Следующий этап -->
		{#if next}
			<div class="flex items-center justify-center space-x-2 opacity-50 leading-relaxed">
				<span class="text-gray-500 text-sm">{getDescriptionStage(next)}</span>
			</div>
		{/if}
	</div>

	<!-- Завершённые этапы, отображаем в виде кнопок с Popover -->
	{#each Object.entries(data?.status?.stages) as [stage, status]}
		{#if status !== 'running' && status !== 'error'}
			<Popover trigger="hover" class="text-sm hidden md:block" transition={slide}>
				<ol class="flex items-center space-x-4">
					{#each stages as stage, index}
						<li class="relative w-full mb-6">
							<div class="flex items-center">
								<!-- Индикатор для текущего или завершённого этапа -->
								<Indicator
									size="xl"
									color="none"
									class={`z-10 ring-0 ring-white sm:ring-8 dark:ring-gray-900 shrink-0 ${getStageStatusClass(data.status.stages[stage])}`}
								>
									{#if data.status.stages[stage] === 'finished'}
										<CheckCircleSolid class="w-6 h-6 text-white" />
									{:else if data.status.stages[stage] === 'running'}
										<div
											class="w-6 h-6 border-t-2 border-blue-600 border-solid rounded-full animate-spin"
										></div>
									{/if}
								</Indicator>

								<!-- Соединительные линии между этапами -->
								{#if index < stages.length - 1}
									<div class="flex w-full bg-gray-200 h-0.5 dark:bg-gray-700"></div>
								{/if}
							</div>

							<!-- Описание этапа -->
							<div class="mt-3 text-center">
								<h3 class="font-medium text-gray-900 dark:text-white">
									{getDescriptionStage(stage)}
								</h3>
							</div>
						</li>
					{/each}
				</ol>

				<div class="space-y-2 p-4">
					<div class="w-full bg-gray-200 rounded-full h-2.5 mb-4 dark:bg-gray-700">
						<div
							class="bg-green-500 h-2.5 rounded-full"
							style="width: {getProgressPercentage()}%"
						></div>
					</div>
				</div>
			</Popover>
		{/if}
	{/each}
{/if}

<style>
	.animate-pulse {
		animation: pulse 1.5s infinite;
	}

	@keyframes pulse {
		0% {
			transform: scale(1);
		}
		50% {
			transform: scale(1.05);
		}
		100% {
			transform: scale(1);
		}
	}

	/* Межстрочный интервал для текста */
	.leading-relaxed {
		line-height: 1.75; /* Можно настроить этот параметр для нужного интервала */
	}
</style>
