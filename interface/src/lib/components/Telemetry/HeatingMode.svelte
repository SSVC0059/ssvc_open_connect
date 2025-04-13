<script lang="ts">
	import Flame from '$lib/assets/flame.png';
	import { slide } from 'svelte/transition';
	import { Popover } from 'flowbite-svelte';

	interface Props {
		heatingOn: boolean;
		overclockingOn: boolean;
	}

	let { heatingOn, overclockingOn }: Props = $props();
</script>

<div class="flex justify-center mt-10 mb-10">
	{#if heatingOn}
		{#if overclockingOn}
			{#each Array(3) as _, i}
				<img src={Flame} alt="Overclocking" class="object-contain w-20 h-10 flame-animation" />
				<Popover
					class="w-64 text-lg font-light text-gray-500 bg-white dark:text-gray-400 dark:border-gray-600 dark:bg-gray-800"
					title="Режим"
					transition={slide}
				>
					Разгон включен
				</Popover>
			{/each}
		{:else}
			<img src={Flame} alt="Heating" class="object-contain w-20 h-10 flame-animation" />
			<Popover
				class="w-64 text-sm font-light text-gray-500 bg-white dark:text-gray-400 dark:border-gray-600 dark:bg-gray-800"
				title="Режим"
				transition={slide}
			>
				Нагрев включен
			</Popover>
		{/if}
	{/if}
</div>

<style>
	@keyframes flamePulse {
		0% {
			opacity: 0.7;
			transform: scale(1);
		}
		50% {
			opacity: 1;
			transform: scale(1.1);
		}
		100% {
			opacity: 0.7;
			transform: scale(1);
		}
	}

	.flame-animation {
		animation: flamePulse 2s infinite;
	}
</style>
