<script lang="ts">
	import { Label } from 'flowbite-svelte';
	import { Alert } from 'flowbite-svelte';

	export let stops: number

</script>

<div class="items-center flex justify-center px-4 gap-2" >
	<Label class="text-lg block">Количество стопов - </Label>
	<p class="text-2xl text-center font-semibold text-indigo-500">
		{stops}
	</p>
</div>



