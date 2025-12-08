<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { getSamplingRate } from '$lib/components/StartWizard/wizardLogic';
	import NumberInput from '$lib/components/NumberInput.svelte';
	import TimeInput from '$lib/components/TimeInput.svelte';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>

<div class="settings-block">
	<h2 class="settings-title">Настройка отбора голов</h2>
	<div class="settings-group">
		<div class="settings-item">
			<label class="input-label" for="startDelay">
				Отсрочка, сек
			</label>
			<NumberInput
				bind:value={settings.start_delay}
				min={0}
				max={18000}
				unit="сек"
			/>
		</div>

		<!-- Heads Timer (HH:MM) -->
		<div class="settings-item">
			<label class="input-label" for="headsTimer">
				Таймер голов
			</label>
			<TimeInput
				bind:value={settings.heads_timer}
				step={300}
				min={0}
				max={86399}
			/>
		</div>

		<label class="valve-table-title" for="heads">
			Настройки клапана голов
		</label>
		<div class="settings-item">
			<table class="valve-table">
				<thead>
				<tr>
					<th>Открытие</th>
					<th>Период</th>
					<th>Скорость мл/ч</th>
				</tr>
				</thead>
				<tbody>
				<tr>
					<td data-label="Открытие">
						<NumberInput
							step={0.1}
							min={0}
							bind:value={settings.heads[0]}
							unit="сек"
						/>
					</td>
					<td data-label="Период">
						<NumberInput
							step={1}
							min={0}
							bind:value={settings.heads[1]}
							unit="сек"
						/>
					</td>
					<td data-label="Скорость мл/ч">
						<input
							type="text"
							readonly
							class="input-readonly"
							value={getSamplingRate(settings.heads[0], settings.heads[1], settings).toFixed(1)}
						/>
					</td>
				</tr>
				</tbody>
			</table>
		</div>
		<div class="valve-info">
			<p><strong>Пропускная способность клапана:</strong> {settings.valve_bw?.[0]} мл/час</p>
		</div>

		<div class="settings-item">
			<label class="input-label" for="heads_final">
				Снижение, сек
			</label>
			<NumberInput
				bind:value={settings.heads_final}
				max={settings.heads[0]}
				min={0}
				step={0.1}
				unit="сек"
			/>
		</div>

		<label class="valve-table-title" for="heads">
			Сброс
		</label>
		<div class="settings-item">
			<table class="valve-table">
				<thead>
				<tr>
					<th>Скорость сброса</th>
					<th>Время сброса</th>
				</tr>
				</thead>
				<tbody>
				<tr>
					<td data-label="Скорость сброса">
						<NumberInput
							step={0.1}
							min={0}
							max={99.9}
							bind:value={settings.release_speed}
							unit="сек"
						/>
					</td>
					<td data-label="Время сброса">
						<NumberInput
							step={1}
							min={0}
							bind:value={settings.release_timer}
							unit="сек"
						/>
					</td>
				</tr>
				</tbody>
			</table>
		</div>
	</div>
</div>
