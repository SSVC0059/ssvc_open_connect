<script lang="ts">
	import { modals } from 'svelte-modals';
	import Cancel from '~icons/tabler/x';
	import { fetchTelemetry, updateSetting } from '$lib/api/ssvcApi';
	import type { SsvcOpenConnectMessage } from '$lib/types/ssvc';
	import NumberInput from '$lib/components/NumberInput.svelte';
	import TimeInput from '$lib/components/TimeInput.svelte';

	let { isOpen }: Props = $props();

	let data = $state<SsvcOpenConnectMessage | null>();
	let telemetry = $derived(data?.telemetry);

	// Local state for bound values
	let tp1_target = $state(0);
	let tp2_target = $state(0);
	let hysteresis = $state(0);
	let open = $state(0);
	let period = $state(0);
	let decrement = $state(0);
	let countdown_seconds = $state(0);

	interface Props {
		isOpen: boolean;
	}

	$effect(() => {
		loadTelemetry();
	});

	$effect(() => {
		if (telemetry) {
			tp1_target = telemetry.tp1_target ?? 0;
			tp2_target = telemetry.tp2_target ?? 0;
			hysteresis = telemetry.hysteresis ?? 0; // Corrected path
			open = telemetry.open ?? 0;
			period = telemetry.period ?? 0;
			decrement = telemetry.decrement ?? 0;
			if (telemetry.countdown) {
				countdown_seconds = timeToSeconds(parseTimeString(telemetry.countdown));
			}
		}
	});

	// Функция для преобразования объекта времени в общее количество секунд
	function timeToSeconds(time: { h: number; m: number; s: number }): number {
		return time.h * 3600 + time.m * 60 + time.s;
	}

	// Функция для преобразования времени из формата "4:41:21" в объект {h, m, s}
	function parseTimeString(timeString: string): { h: number; m: number; s: number } {
		if (!timeString) return { h: 0, m: 0, s: 0 };

		const parts = timeString.split(':').map(part => parseInt(part, 10));

		if (parts.length === 3) {
			return { h: parts[0], m: parts[1], s: parts[2] };
		} else if (parts.length === 2) {
			return { h: 0, m: parts[0], s: parts[1] };
		}

		return { h: 0, m: 0, s: 0 };
	}

	async function loadTelemetry(): Promise<void> {
		data = await fetchTelemetry();
	}

	const close = () => {
		modals.close();
	};

	async function save() {
		if (!telemetry) return;

		const tasks = [];

		if (telemetry.tp1_target !== undefined && tp1_target !== telemetry.tp1_target) {
			tasks.push(updateSetting('s_temp', tp1_target));
		}
		if (telemetry.hysteresis !== undefined && hysteresis !== telemetry.hysteresis) {
			tasks.push(updateSetting('s_hyst', hysteresis));
		}
		if ((telemetry.open !== undefined && telemetry.period !== undefined) && (open !== telemetry.open || period !== telemetry.period)) {
			tasks.push(updateSetting('s_speed', [open, period]));
		}
		if (telemetry.decrement !== undefined && decrement !== telemetry.decrement) {
			tasks.push(updateSetting('s_decrement', decrement));
		}
		const original_countdown_seconds = telemetry.countdown ? timeToSeconds(parseTimeString(telemetry.countdown)) : undefined;
		if (original_countdown_seconds !== undefined && countdown_seconds !== original_countdown_seconds) {
			tasks.push(updateSetting('s_timer', countdown_seconds));
		}

		if (tasks.length > 0) {
			await Promise.all(tasks);
		}
		close();
	}

</script>

{#if isOpen}
	<div
		role="dialog"
		class="wizard-overlay-override"
	>
		<div class="wizard"	>
			<!-- Заголовок -->
			<div class="modal-header">
				<h2 class="modal-title">Корректировка оперативных параметров</h2>
			</div>

			<div class="divider m-0"></div>
			<div class="wizard-content">
				{#if telemetry}
					{#if telemetry.tp1_target !== undefined}
						<div class="settings-item">
							<label class="input-label" for="tp1_target">Температура отбора</label>
							<NumberInput
								bind:value={tp1_target}
								min={1}
								max={100}
								step={0.1}
								unit="°С"
							/>
						</div>
					{/if}

					{#if telemetry.tp2_target !== undefined}
						<div class="settings-item">
							<label class="input-label" for="tp1_control_temp">Целевая Температура ТД2</label>
							<NumberInput
								bind:value={tp2_target}
								min={1}
								max={100}
								step={0.1}
								unit="°С"
							/>
						</div>
					{/if}

					{#if telemetry.hysteresis !==0}
						<div class="settings-item">
							<label class="input-label" for="tp1_control_temp">Гистерезис</label>
							<NumberInput
								bind:value={hysteresis}
								min={1}
								max={100}
								step={0.1}
								unit="°С"
							/>
						</div>
					{/if}

					{#if telemetry.open !== undefined && telemetry.period !== undefined}
						<label class="valve-table-title" for="heads">
							Скорость отбора
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
											bind:value={open}
											unit="сек"
										/>
									</td>
									<td data-label="Период">
										<NumberInput
											step={1}
											min={0}
											bind:value={period}
											unit="сек"
										/>
									</td>
								</tr>
								</tbody>
							</table>
						</div>
					{/if}

					{#if telemetry.decrement !== undefined}
						<div class="settings-item">
							<label class="input-label" for="tp1_control_temp">Декремент</label>
							<NumberInput
								bind:value={decrement}
								min={1}
								max={100}
								step={0.1}
								unit="°С"
							/>
						</div>
					{/if}

					{#if telemetry.countdown !== undefined}
						<div class="settings-item">
							<label class="input-label" for="headsTimer">
								Таймер
							</label>
							<TimeInput
								bind:value={countdown_seconds}
								step={300}
								min={0}
								max={86399}
							/>
						</div>
					{/if}
				{/if}
			</div>

			<div class="wizard-nav">
				<button
					type="button"
					onclick={close}
					class="btn btn-back">
					<Cancel class="modal-icon" />
					Отмена
				</button>
				<button
					type="button"
					onclick={save}
					class="btn btn-success">
					Сохранить
				</button>
			</div>
		</div>
	</div>
{/if}
