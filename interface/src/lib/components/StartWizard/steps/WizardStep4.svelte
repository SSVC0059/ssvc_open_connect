<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { handleInputChange, secondsToTimeString } from '$lib/components/StartWizard/wizardLogic';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>

<div class="settings-block">
	<h2 class="settings-title">Настройка отбора подголовников</h2>

	<div>
		<label class="input-label" for="late_heads">
			Настройки клапана подголовников (время открытия / период)
		</label>

		<div class="settings-item">
			<input
				bind:value={settings.late_heads[0]}
				class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				id="late_heads"
				type="number"
			/>
			<input
				bind:value={settings.late_heads[1]}
				class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				id="heads"
				type="number"
			/>
		</div>

	</div>
	<div class="settings-item">
		<label class="input-label" for="late_heads_timer">
			Время отбора подголовников, с.
		</label>
		<input
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="late_heads_timer"
			max="23:55"
			min="00:00"
			onchange={(event) => handleInputChange(settings, event)}
			step="300"
			type="time"
			value={secondsToTimeString(settings.late_heads_timer)}
		/>
	</div>

</div>

<style>
	input:read-only {
		background-color: #efeeee; /* bg-gray-50 */
		cursor: default;
	}
</style>
