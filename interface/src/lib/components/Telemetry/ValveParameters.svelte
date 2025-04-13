<script lang="ts">
	import { Popover } from 'flowbite-svelte';
	import { slide } from 'svelte/transition';

	interface Props {
		n: string;
		vo?: number; // Время открытия клапана в процентах
		o?: number; // Время открытия в секундах
		p?: number; // Периодичность открытия
		s?: number; // скорость отбора клапана
		f?: number; // Отобранный объем
		c?: boolean;
	}

	let { n, vo = 0, o = 0, p = 0, s = 0, f = '', c = false }: Props = $props();
</script>

<div
	class="flex flex-col items-center justify-center p-2 font-sans text-sm md:text-base lg:text-lg"
>
	<!-- Название клапана -->
	<div class="text-center">
		<p class="font-semibold text-blue-600">{n}</p>
	</div>

	<!-- Этап завершен -->
	{#if c}
		<p class="mt-1 font-semibold text-green-500 text-center">Этап завершен</p>
	{:else}
		<!-- Параметры клапана -->
		<div class="flex items-center justify-center gap-2 text-base lg:text-xl mt-2">
			<span class="bg-blue-500 text-white py-1 px-3 rounded-full shadow">
				{#if o}
					{o.toFixed(2)} / {p}
				{:else}
					Закрыт
				{/if}
			</span>

			<!-- Всплывающая подсказка -->
			<Popover
				class="w-64 text-sm font-light text-gray-500 bg-white dark:text-gray-400 dark:border-gray-600 dark:bg-gray-800"
				title="Параметры клапана"
				transition={slide}
				placement="left"
			>
				<div class="flex justify-between my-1">
					<p class="font-normal text-gray-900 dark:text-white">Время открытия:</p>
					<p class="font-semibold text-gray-900 dark:text-white">{o} сек</p>
				</div>
				<div class="flex justify-between mb-2">
					<p class="font-normal text-gray-900 dark:text-white">Периодичность:</p>
					<p class="font-semibold text-gray-900 dark:text-white">{p} сек</p>
				</div>
				<div class="flex justify-between mb-2">
					<p class="font-normal text-gray-900 dark:text-white">Cкорость отбора:</p>
					<p class="font-semibold text-gray-900 dark:text-white">{s} мл/ч</p>
				</div>
				<div class="flex justify-between">
					<p class="font-normal text-gray-900 dark:text-white">Пропускная способность:</p>
					<p class="font-semibold text-gray-900 dark:text-white">{vo / 100}%</p>
				</div>
			</Popover>
		</div>
	{/if}

	<!-- Общий блок Отобрано -->
	<div class="flex justify-center mt-2 gap-2 text-center">
		<p class="font-medium text-gray-700 dark:text-white">Отобрано:</p>
		<p class="font-bold text-green-500">{f ? f : '-'} мл</p>
	</div>
</div>
