<script lang="ts" type="ts">
	import LetsIconsPressure from '~icons/lets-icons/pressure';

	let { pressure } = $props();
</script>

<div class="flex flex-col items-center justify-center text-center px-2">
	<div
		class="inline-flex items-center gap-1 text-base md:text-3xl lg:text-2xl font-bold text-blue-600 dark:text-blue-300 whitespace-nowrap"
	>
		{pressure}
		<LetsIconsPressure class="inline-block" />
	</div>
</div>
