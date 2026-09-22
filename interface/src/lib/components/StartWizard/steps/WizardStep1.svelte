<script lang="ts">
	import type { OcApiFeature, SsvcSettings } from '$lib/types/ssvc';
	import NumberInput from '$lib/components/NumberInput.svelte';
	import { getInfo } from '$lib/api/ssvcApi';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();

	// Режим «Авто 92+» появился в API 1.9. Возможность запрашиваем у контроллера:
	// при отсутствии метаданных считаем её недоступной, иначе на API 1.7/1.8 поле
	// молча отбросит очередь, а визард покажет сохранение.
	let apiFeatures = $state<OcApiFeature[]>([]);

	$effect(() => {
		let cancelled = false;
		void (async () => {
			const info = await getInfo();
			if (!cancelled) {
				apiFeatures = info?.oc.api_features ?? [];
			}
		})();
		return () => {
			cancelled = true;
		};
	});

	const formulaAuto92Available = $derived(
		apiFeatures.find((feature) => feature.name === 'formula_auto92')?.available ?? false
	);
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
				<label class="input-label" for="formulaMode">Формула</label>
				<select
					bind:value={settings.formula}
					class="settings-checkbox"
					id="formulaMode"
				>
					<option value={0}>Выкл</option>
					<option value={1}>Вкл</option>
					<option
						value={2}
						disabled={!formulaAuto92Available}
						title={formulaAuto92Available ? undefined : 'Требуется API 1.9'}
					>
						Авто 92+
					</option>
				</select>
			</div>

			<div class="formula-input-wrapper" class:active={settings.formula > 0}>
				<NumberInput
					bind:value={settings.formula_start_temp}
					min={1}
					max={720}
					unit="°С"
					placeholder="Начальная температура"
				/>
			</div>
		</div>

		<div class="settings-item">
			<label class="input-label" for="tankPressure"> Давление в кубе </label>
			<NumberInput
				bind:value={settings.tank_mmhg}
				min={1}
				max={720}
				unit="мм рт. ст."
			/>
		</div>

		<div class="settings-item read-only">
			<label class="input-label" for="tp2Correction"> Поправка ТД2 </label>
			<NumberInput
				bind:value={settings.tp2_shift}
				min={1}
				max={100}
				unit="°С"
			/>
		</div>

		<div class="settings-item read-only">
			<label class="input-label" for="tp1_control_temp"> Целевая температура в колонне </label>
			<NumberInput
				bind:value={settings.tp1_control_temp}
				min={1}
				max={100}
				step={0.1}
				unit="°С"
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

<style lang="scss">
	.settings-item-wrapper {
		border-bottom: 0;
	}

	.formula-input-wrapper {
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
