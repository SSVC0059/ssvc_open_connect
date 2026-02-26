<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { handleInputChange, secondsToTimeString } from '$lib/components/StartWizard/wizardLogic';
	import NumberInput from '$lib/components/NumberInput.svelte';
	import TimeInput from '$lib/components/TimeInput.svelte';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>

<div class="settings-block">

	<h2 class="settings-title">Настройка отбора подголовников</h2>
	<label class="valve-table-title" for="heads">
		Настройки клапана подголовников
	</label>
	<div class="settings-item">
		<table class="valve-table">
			<thead>
			<tr>
				<th>Время включения</th>
				<th>Период</th>
			</tr>
			</thead>
			<tbody>
			<tr>
				<td data-label="Время включения">
					<NumberInput
						step={0.1}
						min={0}
						max={99.9}
						bind:value={settings.late_heads[0]}
						unit="сек"
					/>
				</td>
				<td data-label="Период">
					<NumberInput
						step={1}
						min={0}
						bind:value={settings.late_heads[1]}
						unit="сек"
					/>
				</td>
			</tr>
			</tbody>
		</table>
	</div>


	<div class="settings-group">
		<div class="settings-item">
			<label class="input-label" for="late_heads_timer">
				Время отбора подголовников, с.
			</label>
			<TimeInput
				bind:value={settings.late_heads_timer}
				step={300}
				min={0}
				max={86399}
			/>
		</div>
	</div>
</div>

