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
	import { getSamplingRate, handleInputChange, secondsToTimeString } from '$lib/components/StartWizard/wizardLogic';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>

<div class="settings-block">
	<h2 class="settings-title">Настройка отбора тела</h2>

	<div class="valve-card">

		<div class="settings-item">
			<label class="settings-label settings-label-center" for="heads">
				Настройки клапана тела
			</label>

			<table class="valve-table">
				<thead>
				<tr>
					<th>Открытие</th>
					<th>Период</th>
					<th>Скорость</th>
				</tr>
				</thead>
				<tbody>
				<tr>
					<td>
						<input
							type="number"
							step="0.1"
							min="0"
							class="input-cell"
							bind:value={settings.hearts[0]}
						/>
					</td>
					<td>
						<input
							type="number"
							step="1"
							min="1"
							class="input-cell"
							bind:value={settings.hearts[1]}
						/>
					</td>
					<td>
						<input
							type="text"
							readonly
							class="w-full px-2 py-1 text-sm bg-gray-100 border border-gray-300 rounded"
							value={getSamplingRate(settings.hearts[0], settings.hearts[1], settings).toFixed(1)}
						/>
					</td>
				</tr>
				</tbody>
			</table>

		</div>
	</div>


	<div class="settings-item">
		<label class="settings-label" for="heads">
			Время отбора подголовников, с.
		</label>
		<input
			class="settings-input"
			id="headsTimer"
			max="23:55"
			min="00:00"
			onchange={(event) => handleInputChange(settings, event)}
			step="300"
			type="time"
			value={secondsToTimeString(settings.late_heads_timer)}
		/>
	</div>

	<div class="settings-item">
		<label class="settings-label" for="tp2Correction">
			Гистерезис, °С
		</label>
		<input
			bind:value={settings.hyst}
			class="settings-input"
			id="tp2Correction"
			max="50"
			min="0"
			step="0.01"
			type="number"
		/>
	</div>

	<div class="settings-item">
		<label class="settings-label" for="decrement">
			Декремент
		</label>
		<input
			bind:value={settings.decrement}
			class="settings-input"
			id="decrement"
			max="100"
			min="0"
			type="number"
		/>
	</div>

	<div class="flex flex-row gap-4">
		<label class="settings-label" for="start_delay">
			Отложенный пуск, с.
		</label>
		<input
			class="settings-input"
			id="start_delay"
			max="05:00"
			min="00:00"
			onchange={(event) => handleInputChange(settings, event)}
			step="1"
			type="time"
			value={secondsToTimeString(settings.start_delay)}
		/>
	</div>

	<div>
		<label class="settings-label" for="hearts_finish_temp">
			Стоп при, °С
		</label>
		<input
			bind:value={settings.hearts_finish_temp}
			class="settings-input"
			id="hearts_finish_temp"
			max="110"
			min="0"
			step="0.1"
			type="number"
		/>
	</div>

</div>

<style>
    .settings-label-center {
        text-align: center;
        display: block;
    }
</style>
