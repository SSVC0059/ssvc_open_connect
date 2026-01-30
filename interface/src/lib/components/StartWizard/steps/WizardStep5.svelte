<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { getSamplingRate, handleInputChange, secondsToTimeString } from '$lib/components/StartWizard/wizardLogic';
	import NumberInput from '$lib/components/NumberInput.svelte';
	import TimeInput from '$lib/components/TimeInput.svelte';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>

<div class="settings-block">
	<h2 class="settings-title">Настройка отбора тела</h2>
		<div class="settings-item">
		<label class="input-label" for="tp2Correction">
			Гистерезис, °С
		</label>
		<NumberInput
			bind:value={settings.hyst}
			max={50}
			min={0}
			step={0.01}
		/>
	</div>

	<div class="settings-item">
		<label class="input-label" for="decrement">
			Декремент
		</label>
		<NumberInput
			bind:value={settings.decrement}
			max={100}
			min={0}
			unit="°С"
		/>
	</div>

	<div class="settings-item">
		<label class="input-label" for="start_delay">
			Отложенный пуск, с.
		</label>
		<TimeInput
			bind:value={settings.start_delay}
			step={1}
			min={0}
			max={18000}
		/>
	</div>

	<div class="settings-item">
		<label class="input-label" for="hearts_finish_temp">
			Стоп при, °С
		</label>
		<NumberInput
			bind:value={settings.hearts_finish_temp}
			max={110}
			min={0}
			step={0.1}
			unit="°С"
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
					<td data-label="Открытие">
						<NumberInput
							step={0.1}
							min={0}
							bind:value={settings.hearts[0]}
							unit="сек"
						/>
					</td>
					<td data-label="Период">
						<NumberInput
							bind:value={settings.hearts[1]}
							step={1}
							min={1}
							unit="сек"
						/>
					</td>
					<td data-label="Скорость">
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
