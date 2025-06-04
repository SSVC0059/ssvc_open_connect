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

	// 1. Объявляем пропсы (опциональные, с fallback-значениями через $state)
	// Объявляем пропсы с типами
	let {
		settings = $bindable(),
		// Значения по умолчанию через отдельные переменные $state
		headDischargeSpeed
	} = $props<{
		settings: SsvcSettings;
		headDischargeSpeed?: number;
	}>();

	const headVolume = $derived(Math.round(headDischargeSpeed * (settings.release_speed / 3600)));
</script>

<div class="space-y-4">
	<h2 class="text-xl font-semibold text-gray-700">Параметры работы клапанов</h2>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_head"
			>Головы, время сброса, с</label
		>
		<input
			bind:value={settings.release_speed}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_head"
			type="number"
		/>
	</div>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_hearts"
			>Головы, сброс со скоростью, мл/ч</label
		>
		<input
			bind:value={headDischargeSpeed}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_hearts"
			type="number"
		/>
	</div>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_hearts"
			>Головы, Объем, мл</label
		>
		<input
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_hearts"
			readonly
			value={headVolume}
		/>
	</div>
</div>

<style>
	input:read-only {
		background-color: #efeeee; /* bg-gray-50 */
		cursor: default;
	}
</style>
