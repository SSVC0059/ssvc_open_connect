<script lang="ts" type="ts">
	let { pressure } = $props();
</script>

<div class="flex flex-col items-center justify-center text-center px-2">
	<div class="text-base md:text-3xl lg:text-2xl font-bold text-blue-600 dark:text-blue-300">
		{pressure} мм рт. ст.
	</div>
</div>
