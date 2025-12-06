<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();
</script>

<div class="settings-block">
		<h2 class="settings-title">Основные настройки</h2>
		<div class="settings-group">
			<div class="settings-item read-only">
				<input
					bind:checked={settings.hearts_temp_shift}
					class="settings-checkbox"
					id="heartsTempShift"
					type="checkbox"
				/>
				<label class="input-label" for="heartsTempShift"> Сдвиг температуры отбора </label>
			</div>

			<div class="settings-item read-only">
				<input
					bind:checked={settings.hearts_pause}
					class="settings-checkbox"
					id="heartsPause"
					type="checkbox"
				/>
				<label class="input-label" for="heartsPause"> Пауза на себя </label>
			</div>

			<div class="settings-item">

				<div class="settings-item settings-item-wrapper">
					<input
						bind:checked={settings.formula}
						class="settings-checkbox"
						id="formulaEnabled"
						type="checkbox"
					/>
					<label class="input-label" for="formulaEnabled">Формула</label>
				</div>

				<div class="formula-input-wrapper" class:active={settings.formula}>
					<input
						bind:value={settings.formula_start_temp}
						class="input-field"
						max="720"
						min="1"
						placeholder="Начальная температура"
						type="number"
					/>
				</div>
			</div>

			<div class="settings-item">
				<label class="input-label" for="tankPressure">
					Давление в кубе
				</label>
				<input
					bind:value={settings.tank_mmhg}
					class="input-field"
					id="tankPressure"
					max="720"
					min="1"
					type="number"
				/>
			</div>

			<div class="settings-item read-only">
				<label class="input-label" for="tp2Correction">
					Поправка ТД2, °С
				</label>
				<input
					bind:value={settings.tp2_shift}
					class="input-field"
					id="tp2Correction"
					max="100"
					min="1"
					step="0.1"
					type="number"
				/>
			</div>

			<div class="settings-item read-only">
				<label class="input-label" for="tp1_control_temp">
				Целевая температура в колонне, °С
				</label>
				<input
					bind:value={settings.tp1_control_temp}
					class="input-field"
					id="tp1_control_temp"
					max="100"
					min="1"
					step="0.1"
					type="number"
				/>
			</div>

			<div class="settings-item read-only">
				<input
					bind:checked={settings.tp_filter}
					class="settings-checkbox"
					id="tpFilter"
					type="checkbox"
				/>
				<label class="input-label" for="tpFilter"> Фильтр ТД2 </label>
			</div>
		</div>

</div>

<style>
	.settings-item-wrapper {
      border-bottom: 0;
	}

    .formula-input-wrapper {
        width: 100px;
        flex-shrink: 0;
        text-align: left;
        opacity: 0;
        pointer-events: none;
        transition: opacity 0.1s ease-in-out;
    }
    .formula-input-wrapper.active {
        opacity: 1;
        pointer-events: auto;
    }
</style>