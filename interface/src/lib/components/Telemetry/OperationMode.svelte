<script lang="ts">
	import { ssvcTelemetryStore } from '$lib/stores/ssvcOpenConnect';
	import { getStageDescription } from '$lib/utils/ssvcHelper'; // Импортируем хранилище из твоего файла store.ts

	let currentMode: string

	ssvcTelemetryStore.subscribe((value) => {
		if (value) {
			currentMode = getStageDescription(value.type)
		}
	});
</script>

{#if currentMode}
	<div >
		<div class="text-center">
			<p class="text-lg font-medium">Текущее состояние:</p>
			<p class="text-2xl transition-colors duration-500">{currentMode}</p>
		</div>
	</div>
{/if}