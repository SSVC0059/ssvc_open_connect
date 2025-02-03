<script lang="ts">
	import { Popover } from 'flowbite-svelte';
	import { slide } from 'svelte/transition';

	export let n: string;
	export let vo: number  = 0; // Время открытия клапана в процентах
	export let o: number = 0; // Время открытия в секундах
	export let p: number = 0; // Периодичность открытия
	export let s: number = 0; // скорость отбора клапана
	export let f: string = "" ; // Отобранный объем
	export let c: boolean = false;
</script>

<div class="flex flex-col items-center justify-center p-2 ">
	<div class="flex text-center">
		<p class="text-lg font-semibold text-blue-600">{n}</p>
	</div>
	{#if c}
		<div class="text-center">
			<p class="text-lg font-semibold text-green-500">Этап завершен</p>
		</div>
		<div  class="mt-2 flex text-center">
			<p class="text-sm font-medium text-gray-700 ">Отобрано: {f ? f : "-"} мл</p>
			<p class="text-lg font-bold text-gray-500"></p>
		</div>
	{:else}
		<div class="flex items-center justify-center gap-2 text-xl">
		<span
			class="bg-blue-500 text-white py-1 px-3 rounded-full shadow">
			{#if o}
				{o.toFixed(2)} / {p}
				{:else}
				Закрыт
			{/if}
		</span>
			<Popover class="w-64 text-sm font-light text-gray-500 bg-white dark:text-gray-400 dark:border-gray-600 dark:bg-gray-800"
							 title="Параметры клапана" transition={slide} placement = 'left'>
				<div class="flex justify-between my-1">
					<p class="mb-2 text-sm font-normal leading-none text-gray-900 dark:text-white">
						Время открытия:
					</p>
					<p class="mb-1 text-base font-semibold leading-none text-gray-900 dark:text-white">
						{o} сек
					</p>
				</div>
				<div class="flex justify-between">
					<p class="mb-3 text-sm font-normal leading-none text-gray-900 dark:text-white">
						Периодичность:
					</p>
					<p class="mb-1 text-base font-semibold leading-none text-gray-900 dark:text-white">
						{p} сек
					</p>
				</div>
				<div class="justify-between">
					Расчетные параметры клапана.
				</div>
				<div class="flex justify-between">
					<p class="mb-3 text-sm font-normal leading-none text-gray-900 dark:text-white">
						Cкорость отбора:
					</p>
					<p class="mb-1 text-sm font-semibold leading-none text-gray-900 dark:text-white">
						{s} мл/ч
					</p>
					<div class="flex justify-between">
						<p class="mb-3 text-sm font-normal leading-none text-gray-900 dark:text-white">
							Пропускная способность:
						</p>
						<p class="mb-1 text-sm font-semibold leading-none text-gray-900 dark:text-white">
							{vo} %
						</p>
					</div>
				</div>
			</Popover>
		</div>
		<div class=" flex justify-center mt-2">
			<p class="text-lg font-medium text-gray-700 dark:text-white">Отобрано:</p>
			<p class="text-lg font-bold text-green-500">{f ? f : "-"} мл</p>
		</div>
	{/if}
</div>


<style>
    div {
        font-family: 'Arial', sans-serif;
    }
</style>
