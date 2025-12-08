<!--{-->
<!--	id: 91,-->
<!--	title: 'Расчеты',-->
<!--	component: WizardStep3,-->
<!--	requiresInitialData: true,-->
<!--	initialData: {-->
<!--	volumeRaw: 10000,-->
<!--	strengthRaw: 40,-->
<!--	headsPercent: 3,-->
<!--	lateHeadsPercent: 5-->
<!--}-->
<!--},-->

<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';

	// 1. Объявляем пропсы (опциональные, с fallback-значениями через $state)
	// Объявляем пропсы с типами
	let {
		settings = $bindable(),
		// Значения по умолчанию через отдельные переменные $state
		volumeRaw: initialVolume = 10000,
		strengthRaw: initialStrength = 40,
		headsPercent: initialHeadsPercent = 3,
		lateHeadsPercent: initialLateHeadsPercent = 5
	} = $props<{
		settings: SsvcSettings;
		volumeRaw?: number;
		strengthRaw?: number;
		headsPercent?: number;
		lateHeadsPercent?: number;
	}>();

	// Создаем реактивные переменные
	let volumeRaw = $state(initialVolume);
	let strengthRaw = $state(initialStrength);
	let headsPercent = $state(initialHeadsPercent);
	let lateHeadsPercent = $state(initialLateHeadsPercent);
</script>

<div class="space-y-4">
	<h2 class="text-xl font-semibold text-gray-700">Калькулятор расчета отбора</h2>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_head"
			>Объем сырца, мл</label
		>
		<input
			bind:value={volumeRaw}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_head"
			type="number"
		/>
	</div>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_hearts"
			>Крепость сырца при 20°С, %</label
		>
		<input
			bind:value={strengthRaw}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_hearts"
			type="number"
		/>
	</div>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_tails"
			>Процент голов, %</label
		>
		<input
			bind:value={headsPercent}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_tails"
			type="number"
		/>
	</div>

	<div>
		<label class="block text-sm font-medium text-gray-700" for="valve_bw_tails"
			>Процент подголовников, %</label
		>
		<input
			bind:value={lateHeadsPercent}
			class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
			id="valve_bw_tails"
			type="number"
		/>
	</div>
</div>
