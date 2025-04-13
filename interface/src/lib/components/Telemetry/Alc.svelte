<script lang="ts" type="ts">
	let { alcohol } = $props();
</script>

<div class="flex flex-col items-center justify-center text-center px-2">
	<div class="text-base md:text-3xl lg:text-4xl font-bold text-blue-600 dark:text-blue-300">
		{alcohol}% об.
	</div>
</div>
