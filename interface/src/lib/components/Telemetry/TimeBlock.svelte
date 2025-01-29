<script lang="ts">
	export let name: string = "";
	export let timeString: string = "-"
</script>

<div class="flex flex-col items-center gap-2 p-1 rounded-lg">
	<p class="text-base font-medium text-gray-600">
		{name || "Default Name"}
	</p>
	<span class="text-base font-semibold text-indigo-500">
		{timeString}
	</span>
</div>

<style>
    div {
        font-family: 'Arial', sans-serif;
    }
</style>
