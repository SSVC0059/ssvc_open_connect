<script lang="ts">
	import Pause from '~icons/tabler/player-pause';
	import { slide } from 'svelte/transition';
	import { Popover } from 'flowbite-svelte';
</script>

<div class="w-40 h-40 flex items-center justify-center animate-pulse">
	<Pause class="w-full h-full text-red-500" />
</div>
<Popover
	class="w-64 text-sm font-light text-gray-500 bg-white dark:text-gray-400 dark:border-gray-600 dark:bg-gray-800"
	title="Пауза"
	transition={slide}
>
	Отбор временно преостановлен
</Popover>

<style>
	@keyframes pulse {
		0%,
		100% {
			opacity: 0.8;
			transform: scale(1);
		}
		50% {
			opacity: 0.5;
			transform: scale(1.1);
		}
	}
	.animate-pulse {
		animation: pulse 1.5s infinite;
	}
</style>
