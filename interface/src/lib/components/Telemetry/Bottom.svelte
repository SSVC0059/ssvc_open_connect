<script lang="ts">
	import type { RectStatus, telemetry as TelemetryType } from '$lib/types/models';
	import { getEventDescription } from '$lib/utils/ssvcHelper.ts';
	import { Popover } from 'flowbite-svelte';
	import { slide } from 'svelte/transition';

	// Новый синтаксис для пропсов в Svelte 5
	const { telemetry, status, getStageDescription } = $props<{
		telemetry: TelemetryType;
		status?: RectStatus;
		getStageDescription: (type: string) => string;
	}>();

	// Вычисляемые значения
	const eventDescription = $derived(telemetry.event ? getEventDescription(telemetry.event) : '');
</script>

{#if telemetry}
	<div
		class="relative h-full p-4 rounded-xl flex items-center justify-center overflow-hidden border border-gray-200 dark:border-gray-700"
	>
		<!-- Десктопные элементы времени (оставляем как было) -->
		{#if telemetry.countdown}
			<div
				id="countdown"
				class="absolute top-3 left-3 px-4 py-2 rounded-xl text-center hidden lg:block"
			>
				<p
					class="text-xs sm:text-sm md:text-base text-gray-700 dark:text-gray-300 mb-1 font-medium"
				>
					Осталось
				</p>
				<p
					class="text-lg sm:text-xl md:text-2xl font-bold text-gray-900 dark:text-white tracking-wide"
				>
					{telemetry.countdown}
				</p>
				<Popover
					placement="right"
					class="w-64 text-sm font-light text-gray-700 bg-white dark:text-gray-300 dark:border-gray-600 dark:bg-gray-800"
					transition={slide}
					triggeredBy="#countdown"
				>
					Время до окончания этапа
				</Popover>
			</div>
		{/if}

		{#if telemetry.time}
			<div
				id="time"
				class="absolute top-3 right-3 px-4 py-2 rounded-xl text-center hidden lg:block"
			>
				<p
					class="text-xs sm:text-sm md:text-base text-gray-700 dark:text-gray-300 mb-1 font-medium"
				>
					Прошло
				</p>
				<p
					class="text-lg sm:text-xl md:text-2xl font-bold text-gray-900 dark:text-white tracking-wide"
				>
					{telemetry.time}
				</p>
				<Popover
					placement="left"
					class="w-64 text-sm font-light text-gray-700 bg-white dark:text-gray-300 dark:border-gray-600 dark:bg-gray-800"
					transition={slide}
					triggeredBy="#time"
				>
					Пройденное время этапа
				</Popover>
			</div>
		{/if}

		<!-- Основной контент с ограничениями ширины -->
		<div class="w-full max-w-[90%] mx-auto">
			<!-- Добавляем ограничение по ширине -->
			<div class="text-center px-4 py-4 sm:px-6 sm:py-4 rounded-xl animate-fade-in">
				<div
					class="text-lg sm:text-xl md:text-2xl lg:text-3xl font-semibold text-blue-700 dark:text-blue-400 tracking-wide"
				>
					{getStageDescription(telemetry.type)}
				</div>

				{#if telemetry.event}
					<div
						class="text-sm sm:text-lg md:text-xl font-medium text-red-700 dark:text-red-300 text-center break-words mt-2 mx-auto max-w-[85%]"
					>
						{eventDescription}
					</div>
				{/if}
			</div>

			<!-- Мобильные временные метки -->
			<div class="flex lg:hidden justify-between w-full absolute bottom-3 left-0 right-0 px-4">
				<span class="text-sm font-medium text-gray-600 dark:text-gray-300 min-w-[30%] text-left">
					{#if telemetry.countdown}{telemetry.countdown}{/if}
				</span>
				<span class="text-sm font-medium text-gray-600 dark:text-gray-300 min-w-[30%] text-right">
					{#if telemetry.time}{telemetry.time}{/if}
				</span>
			</div>
		</div>
	</div>
{/if}

<style>
	.animate-slide-in {
		animation: slide-in 0.3s ease-out forwards;
	}

	@keyframes slide-in {
		from {
			transform: translateY(-10px);
			opacity: 0;
		}
		to {
			transform: translateY(0);
			opacity: 1;
		}
	}

	@media (max-width: 640px) {
		.text-xl {
			font-size: 1.1rem;
		}

		.text-lg {
			font-size: 1rem;
		}

		.text-base {
			font-size: 0.9rem;
		}

		.px-6 {
			padding-left: 1rem;
			padding-right: 1rem;
		}

		.py-4 {
			padding-top: 0.75rem;
			padding-bottom: 0.75rem;
		}
	}
</style>
