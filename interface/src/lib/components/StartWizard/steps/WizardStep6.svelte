<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { getSamplingRate } from '$lib/components/StartWizard/wizardLogic';
	import ApiVersionGuard from '$lib/components/ApiVersionGuard.svelte';
	import { requireVersion } from '$lib/actions/versioning';
	import NumberInput from '$lib/components/NumberInput.svelte';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();

</script>
	<ApiVersionGuard requiredVersion={1.6} message="Функционал настройки параллельного отбора доступен в текущей версии API"></ApiVersionGuard>
	<div class="settings-block" use:requireVersion={1.6}>
		<div class="valve-card" >
			<h2 class="valve-table-title">Параллельный отбор клапаном №1</h2>
			<table class="valve-table">
				<thead>
				<tr class="table-header">
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
							bind:value={settings.parallel_v1[0]}
							unit="Сек"
						/>
					</td>
					<td data-label="Период">
						<NumberInput
							step= {1}
							min={1}
							bind:value={settings.parallel_v1[1]}
							unit="Сек"
						/>
					</td>
					<td data-label="Скорость">
						<input
							type="text"
							readonly
							class="input-readonly"
							value={getSamplingRate(settings.parallel_v1[0], settings.parallel_v1[1], settings).toFixed(1)}
						/>
					</td>
				</tr>
				</tbody>
			</table>
			<!--			</div>-->
			<div class="valve-info">
				<p><strong>Пропускная способность клапана №1:</strong> {settings.valve_bw?.[0]} мл/час</p>
			</div>
		</div>

		<div class="valve-card">
			<h2 class="valve-table-title">Параллельный отбор клапаном №3</h2>
			{#if settings.parallel_v3?.length}
				<table class="valve-table">
					<thead>
					<tr class="table-header">
						<th>Температура</th>
						<th>Открытие</th>
						<th>Период</th>
						<th>Скорость</th>
					</tr>
					</thead>
					<tbody>
					{#each settings.parallel_v3 as [temp, openTime, period], i}
						<tr>
							<td data-label="Температура">
								<NumberInput
									step={0.1}
									min={0}
									max={100}
									bind:value={settings.parallel_v3[i][0]}
									unit="°С"
								/>
							</td>
							<td data-label="Открытие">
								<NumberInput
									step={0.1}
									min={0}
									bind:value={settings.parallel_v3[i][1]}
									unit="Сек"
								/>
							</td>
							<td data-label="Период">
								<NumberInput
									step={1}
									min={1}
									bind:value={settings.parallel_v3[i][2]}
									unit="Сек"
								/>
							</td>
							<td data-label="Скорость">
								<input
									type="text"
									readonly
									class="input-readonly"
									value={getSamplingRate(openTime, period, settings).toFixed(1)}
								/>
							</td>
						</tr>
					{/each}
					</tbody>
				</table>

				<div class="valve-info">
					<p><strong>Пропускная способность клапана №3:</strong> {settings.valve_bw?.[2]} мл/час</p>

				</div>
			{/if}
		</div>
	</div>
