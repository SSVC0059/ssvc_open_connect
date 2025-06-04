<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();
</script>

<div class="space-y-4">
	<h2 class="text-xl font-semibold text-gray-700">General Settings</h2>

	<div class="space-y-5">
		<div class="flex items-center space-x-4">
			<input
				bind:checked={settings.hearts_temp_shift}
				class="h-5 w-5 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
				id="heartsTempShift"
				type="checkbox"
			/>
			<label class="text-lg text-gray-700" for="heartsTempShift"> Сдвиг температуры отбора </label>
		</div>

		<div class="flex items-center space-x-4">
			<input
				bind:checked={settings.hearts_pause}
				class="h-5 w-5 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
				id="heartsPause"
				type="checkbox"
			/>
			<label class="text-lg text-gray-700" for="heartsPause"> Пауза на себя </label>
		</div>

		<div class="flex items-center space-x-4">
			<input
				bind:checked={settings.tp_filter}
				class="h-5 w-5 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
				id="tpFilter"
				type="checkbox"
			/>
			<label class="text-lg text-gray-700" for="tpFilter"> Фильтр ТД2 </label>
		</div>
	</div>

	<!-- Formula Section -->
	<div class="flex items-center">
		<!-- Чекбокс и лейбл -->
		<div class="flex items-center mr-4">
			<!-- Добавлен фиксированный отступ -->
			<input
				bind:checked={settings.formula}
				class="h-5 w-5 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
				id="formulaEnabled"
				type="checkbox"
			/>
			<label class="ml-2 text-lg text-gray-700" for="formulaEnabled">Формула</label>
		</div>

		<!-- Поле ввода (заранее зарезервировано место) -->
		<div
			class="min-w-[300px] transition-opacity duration-100 {settings.formula
				? 'opacity-100'
				: 'opacity-0 pointer-events-none'}"
		>
			<input
				bind:value={settings.formula_start_temp}
				class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				max="720"
				min="1"
				placeholder="Начальная температура"
				type="number"
			/>
		</div>
	</div>

	<!-- Input Fields -->
	<div class="space-y-5">
		<div>
			<label class="block text-lg font-medium text-gray-700 mb-2" for="tankPressure">
				Давление в кубе
			</label>
			<input
				bind:value={settings.tank_mmhg}
				class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				id="tankPressure"
				max="720"
				min="1"
				type="number"
			/>
		</div>

		<div>
			<label class="block text-lg font-medium text-gray-700 mb-2" for="tp2Correction">
				Поправка ТД2, °С
			</label>
			<input
				bind:value={settings.tp2_shift}
				class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				id="tp2Correction"
				max="720"
				min="1"
				step="0.1"
				type="number"
			/>
		</div>
	</div>
</div>
