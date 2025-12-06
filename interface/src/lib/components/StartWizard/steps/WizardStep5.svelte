<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { getSamplingRate, handleInputChange, secondsToTimeString } from '$lib/components/StartWizard/wizardLogic';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>

<div class="settings-block">
	<h2 class="settings-title">Настройка отбора тела</h2>
	<div class="settings-item">
		<label class="input-label" for="heads">
			Время отбора подголовников, с.
		</label>
		<input
			class="input-field"
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
		<label class="input-label" for="tp2Correction">
			Гистерезис, °С
		</label>
		<input
			bind:value={settings.hyst}
			class="input-field"
			id="tp2Correction"
			max="50"
			min="0"
			step="0.01"
			type="number"
		/>
	</div>

	<div class="settings-item">
		<label class="input-label" for="decrement">
			Декремент
		</label>
		<input
			bind:value={settings.decrement}
			class="input-field"
			id="decrement"
			max="100"
			min="0"
			type="number"
		/>
	</div>

	<div class="settings-item">
		<label class="input-label" for="start_delay">
			Отложенный пуск, с.
		</label>
		<input
			class="input-field time-input-wide"
			id="start_delay"
			max="05:00"
			min="00:00"
			onchange={(event) => handleInputChange(settings, event)}
			step="1"
			type="time"
			value={secondsToTimeString(settings.start_delay)}
		/>
	</div>

	<div class="settings-item">
		<label class="input-label" for="hearts_finish_temp">
			Стоп при, °С
		</label>
		<input
			bind:value={settings.hearts_finish_temp}
			class="input-field"
			id="hearts_finish_temp"
			max="110"
			min="0"
			step="0.1"
			type="number"
		/>
	</div>

	<div class="valve-card">
		<label class="valve-table-title" for="hearts">
			Настройки клапана тела
		</label>
		<div class="settings-item">
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
							class="input-readonly"
							value={getSamplingRate(settings.hearts[0], settings.hearts[1], settings).toFixed(1)}
						/>
					</td>
				</tr>
				</tbody>
			</table>
		</div>
		<div class="valve-info">
			<p><strong>Пропускная способность клапана:</strong> {settings.valve_bw?.[1]} мл/час</p>
		</div>
	</div>

</div>

<style>
    .time-input-wide {
        width: 130px; /* Увеличиваем ширину с 100px до 130px, чтобы вместить время с секундами (ЧЧ:ММ:СС) */
    }
</style>
