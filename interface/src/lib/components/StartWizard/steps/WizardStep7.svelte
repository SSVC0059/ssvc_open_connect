<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { getSamplingRate, } from '$lib/components/StartWizard/wizardLogic';
	import NumberInput from '$lib/components/NumberInput.svelte';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>


<div class="settings-block">
	<h2 class="settings-title">Настройка отбора хвостов</h2>

	<div class="valve-card">
		<label class="valve-table-title" for="heads">
			Настройки клапана (время открытия / период)
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
							bind:value={settings.tails[0]}
							unit="Сек"
						/>
					</td>
					<td data-label="Период">
						<NumberInput
							step={1}
							min={1}
							bind:value={settings.tails[1]}
							unit="Сек"
						/>
					</td>
					<td data-label="Скорость">
						<input
							type="text"
							readonly
							class="input-readonly"
							value={getSamplingRate(settings.tails[0], settings.tails[1], settings).toFixed(1)}
						/>
					</td>
				</tr>
				</tbody>
			</table>

		</div>
	</div>

	<div class="settings-item">
		<label class="input-label" for="tails_temp">
			Стоп при температуре, °С
		</label>
		<NumberInput
			bind:value={settings.tails_temp}
			max={50}
			min={0}
			step={0.01}
			unit="°С"
		/>
	</div>

</div>

