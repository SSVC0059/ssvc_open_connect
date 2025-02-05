<script lang="ts">
	import {
		getStageDescription,
	} from '$lib/utils/ssvcHelper';

	export let type: string;
	export let event: string = "";
</script>

<div class="flex flex-col items-center justify-center gap-x-4 p-4 rounded-lg">
	<p class="text-center text-xl sm:text-2xl font-bold text-blue-700 dark:text-white tracking-wide">
		{getStageDescription(type)}
	</p>
	<p class="text-center text-xl sm:text-2xl font-bold text-red-500 tracking-wide">
		{event}
	</p>
</div>

