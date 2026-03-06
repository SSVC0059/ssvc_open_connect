<script lang="ts">
	import { onMount } from 'svelte';
	import WifiOff from '~icons/tabler/wifi-off';

	let online = false;
	let checked = false;

	onMount(() => {
		online = navigator.onLine;
		checked = true;
	});
</script>

{#if checked}
	{#if online}
		<div class="h-full w-full">
			<iframe
				src="https://ssvc0059.github.io/ssvc_open_connect/"
				class="h-full w-full"
				title="Документация"
			></iframe>
		</div>
	{:else}
		<div class="hero h-full bg-base-200">
			<div class="hero-content text-center">
				<div class="max-w-md">
					<WifiOff class="inline-block h-16 w-16" />
					<h1 class="mt-4 text-3xl font-bold">Нет подключения к интернету</h1>
					<p class="py-6">
						Страница недоступна из-за отсутствия подключения к интернету. Пожалуйста, проверьте
						ваше соединение и попробуйте снова.
					</p>
				</div>
			</div>
		</div>
	{/if}
{:else}
	<div class="flex h-full items-center justify-center">
		<span class="loading loading-lg"></span>
	</div>
{/if}
