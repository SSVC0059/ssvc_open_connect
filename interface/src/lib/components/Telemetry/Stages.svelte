<script lang="ts">
	import { Indicator } from 'flowbite-svelte';
	import { CheckCircleSolid } from 'flowbite-svelte-icons';
	import { rectificationStatusStore} from '$lib/stores/ssvcOpenConnect';
	import type { RectificationStatus } from '$lib/types/models';

	let rectificationStatus: RectificationStatus
	rectificationStatusStore.subscribe((value) => {
		if (value) {
			rectificationStatus = value;
		}
	});

	function getDescriptionStage(stage: string) {
		switch (stage) {
			case 'waiting':
				return "Дежурный режим";
			case 'tp1_waiting':
				return "Нагрев колонны";
			case 'delayed_start':
				return "Отложенный старт";
			case 'heads':
				return "Головы";
			case 'late_heads':
				return "Подголовники";
			case 'hearts':
				return "Тело";
			case 'tails':
				return "Хвосты";

		}
	}

	function getStageStatusStile(status: string) {
		switch (status) {
			case 'running':
				return {
					bgColor: 'bg-blue-200 dark:bg-blue-900',
					iconColor: 'text-blue-600 dark:text-blue-300',
					opacity: 'opacity-100',
					enabled: false,
				};
			case 'paused':
				return {
					bgColor: 'bg-yellow-200 dark:bg-yellow-900',
					iconColor: 'text-yellow-600 dark:text-yellow-300',
					opacity: 'opacity-100',
					enabled: false,
				};
			case 'finished':
				return {
					bgColor: 'bg-green-200 dark:bg-green-900',
					iconColor: 'text-green-600 dark:text-green-300',
					opacity: 'opacity-100',
					enabled: true,
				};
			case 'skipped':
				return {
					bgColor: 'bg-gray-200 dark:bg-gray-900',
					iconColor: 'text-gray-500 dark:text-gray-600',
					opacity: 'opacity-60',
					enabled: false,
				};
			default:
				return {
					bgColor: 'bg-gray-200 dark:bg-gray-900',
					iconColor: 'text-gray-500 dark:text-gray-600',
					opacity: 'opacity-100',
					enabled: false,
				};
		}
	}

</script>

{#if rectificationStatus && rectificationStatus.response && rectificationStatus.response.stages}
	<ul class="px-4 space-y-2">
		{#each Object.entries(rectificationStatus?.response?.stages) as [stage, status]}
			{#if stage != "error"}
				<li class={`flex items-center justify-between ${getStageStatusStile(status)?.opacity}`}>
					<div class="flex items-center">
						<div
							class={`flex items-center justify-center w-4 h-4 rounded-full shrink-0 ${getStageStatusStile(status)?.bgColor}`}
						>
							<Indicator
								size="xl"
								color="none"
								class={`z-10 ring-0 ring-white sm:ring-8 dark:ring-gray-900 shrink-0 ${getStageStatusStile(status)?.bgColor}`}
							>
								{#if status === 'finished'}
									<CheckCircleSolid	class={`w-6 h-6 ${getStageStatusStile("heads")?.iconColor}`}
									/>
								{:else if status === 'running'}
									<div class="flex items-center justify-center w-8 h-8 rounded-full bg-primary-200 dark:bg-primary-900 shrink-0">
										<div class="spinner"></div>
									</div>
								{/if}
							</Indicator>
						</div>
						<span
							class={`ml-3 text-gray-800 dark:text-gray-300 ${
						getStageStatusStile("heads")?.enabled ? '' : 'pointer-events-none'
						}`}>
						{getDescriptionStage(stage)}
					</span>
					</div>
				</li>
			{/if}

		{/each}
	</ul>
{/if}

<style>
    .spinner {
        width: 25px;
        height: 25px;
        border: 5px solid transparent;
        border-top: 5px solid #3498db;
        border-right: 5px solid rgba(52, 152, 219, 0.8); /* Полупрозрачный цвет */
        border-radius: 80%;
        animation: spin 1s linear infinite;
        box-shadow: 0 0 10px rgba(52, 152, 219, 0.4); /* Добавляем тень для глубины */
    }

    @keyframes spin {
        0% {
            transform: rotate(0deg);
        }
        100% {
            transform: rotate(360deg);
        }
    }
</style>