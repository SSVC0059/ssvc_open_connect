<script lang="ts">
	import { onMount } from 'svelte';
	import Sun from '~icons/tabler/sun';
	import Moon from '~icons/tabler/moon';

	type Theme = 'corporate' | 'business';

	let theme: Theme = 'corporate';

	onMount(() => {
		theme = document.documentElement.getAttribute('data-theme') === 'business' ? 'business' : 'corporate';
	});

	function toggleTheme() {
		theme = theme === 'business' ? 'corporate' : 'business';
		document.documentElement.setAttribute('data-theme', theme);
		try {
			localStorage.setItem('ssvc-theme', theme);
		} catch {
			// localStorage может быть недоступен (приватный режим) — тема просто не запомнится.
		}
	}
</script>

<button
	type="button"
	class="btn btn-ghost btn-circle btn-sm"
	aria-label={theme === 'business' ? 'Включить светлую тему' : 'Включить тёмную тему'}
	title={theme === 'business' ? 'Светлая тема' : 'Тёмная тема'}
	onclick={toggleTheme}
>
	{#if theme === 'business'}
		<Sun class="h-5 w-5" />
	{:else}
		<Moon class="h-5 w-5" />
	{/if}
</button>
