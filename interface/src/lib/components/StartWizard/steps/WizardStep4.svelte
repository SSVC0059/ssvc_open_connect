<!--{-->
<!--	id: 92,-->
<!--	title: 'Расчеты',-->
<!--	component: WizardStep4,-->
<!--	requiresInitialData: true,-->
<!--	get initialData() {-->
<!--	return {-->
<!--	headDischargeSpeed: settings-->
<!--	? calculateSamplingRate(settings.heads[0], settings.heads[1], settings.valve_bw[0])-->
<!--	: 0 // или другое значение по умолчанию-->
<!--};-->
<!--}-->
<!--}-->

<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';
	import { handleInputChange, secondsToTimeString } from '$lib/components/StartWizard/wizardLogic';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>

<div class="settings-block">
	<h2 class="settings-title">Настройка отбора подголовников</h2>

	<div>
		<label class="block text-lg font-medium text-gray-700 mb-2" for="late_heads">
			Настройки клапана подголовников (время открытия / период)
		</label>

		<div class="flex flex-row gap-4">
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
	<div class="flex flex-row gap-4">
		<label class="block text-lg font-medium text-gray-700 mb-2" for="late_heads_timer">
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
