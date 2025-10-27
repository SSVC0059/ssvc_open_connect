<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';
	import { handleInputChange, secondsToTimeString, getSamplingRate } from '$lib/components/StartWizard/wizardLogic';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();


</script>

<div class="settings-block">
	<h2 class="settings-title">Настройка отбора голов</h2>

	<div class="settings-group">
		<div class="settings-item">
			<label class="settings-label" for="startDelay">
				Отсрочка, сек
			</label>
			<input
				bind:value={settings.start_delay}
				class="settings-input"
				id="startDelay"
				max="18000"
				min="0"
				type="number"
			/>
		</div>

		<!-- Heads Timer (HH:MM) -->
		<div class="settings-item">
			<label class="settings-label" for="headsTimer">
				Таймер голов
			</label>

			<input
				class="settings-input"
				id="headsTimer"
				max="23:55"
				min="00:00"
				onchange={(event) => handleInputChange(settings, event)}
				step="300"
				type="time"
				value={secondsToTimeString(settings.heads_timer)}
			/>
		</div>

		<!-- Heads Timer (HH:MM) -->
		<div class="valve-card">
			<div class="settings-item">
				<label class="settings-label" for="heads">
					Настройки клапана голов
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
								bind:value={settings.heads[0]}
							/>
						</td>
						<td>
							<input
								type="number"
								step="1"
								min="1"
								class="input-cell"
								bind:value={settings.heads[1]}
							/>
						</td>
						<td>
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
				<p><strong>Пропускная способность клапана №2:</strong> {settings.valve_bw?.[1]} мл/час</p>
			</div>

		</div>

		<div class="settings-item">
			<label class="settings-label" for="heads_final">
				Снижение, сек
			</label>
			<input
				bind:value={settings.heads_final}
				class="settings-input"
				id="heads_final"
				max={settings.heads[0]}
				min="0"
				step="0.1"
				type="number"
			/>
		</div>

		<div class="settings-item">
			<label class="settings-label" for="release_speed">
				Сброс (время открытия / период)
			</label>

			<div class="flex flex-row gap-4">
				<input
					bind:value={settings.release_speed}
					class="settings-input"
					id="release_speed"
					type="number"
				/>
				<div class="flex flex-row gap-4">
					<input
						bind:value={settings.release_timer}
						class="settings-input"
						id="release_speed"
						type="number"
					/>
				</div>
			</div>
		</div>
	</div>
</div>
