<script lang="ts">
	import { modals } from 'svelte-modals';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Portal from 'svelte-portal';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import type { SsvcOpenConnectMessage } from '$lib/types/ssvc';
	import type { SensorsData } from '$lib/types/Sensors';
	import { fetchTelemetry, updateSetting } from '$lib/api/ssvcApi';
	import { notifications } from '$lib/components/toasts/notifications';

	// Принимаем оба колбэка
	const { onConfirm, onCancel } = $props<{
		onConfirm?: () => void;
		onCancel?: () => void;
	}>();

	let data = $state<SsvcOpenConnectMessage | null>();
	let telemetry = $derived(data?.telemetry)

	async function loadTelemetry(): Promise<void>{
		data = await fetchTelemetry();
	}

	let isEditing = $state(false);
	let draftTime = $state({ h: 0, m: 0, s: 0 });
	let hourStep = $state(1);
	let minuteStep = $state(1);

	// Валидация введенных значений
	const isInvalid = $derived({
		h: draftTime.h < 0,
		m: draftTime.m < 0 || draftTime.m > 59,
		s: draftTime.s < 0 || draftTime.s > 59
	});

	// Проверка на кратность и максимальное значение
	const validateTime = (time: { h: number; m: number; s: number }) => {
		const totalSeconds = time.h * 3600 + time.m * 60 + time.s;
		return {
			exceedsMax: totalSeconds > 86400,
			notMultiple: totalSeconds % 1 !== 0
		};
	};

	const validation = $derived(validateTime(draftTime));

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

	// Функция для преобразования объекта времени в общее количество секунд
	function timeToSeconds(time: { h: number; m: number; s: number }): number {
		return time.h * 3600 + time.m * 60 + time.s;
	}

	// Заглушка для REST API
	async function saveChanges(field: string, value: any) {
		console.log(field, JSON.stringify(value));
		let result = await updateSetting(field, value);
		if (result) {
			notifications.success('Настройка сохранена', 5000);
		} else {
			notifications.error('Ошибка сохранения настроек', 5000);
		}
	}

	// Функция для сохранения таймера
	async function handleSaveTimer() {
		const totalSeconds = timeToSeconds(draftTime);
		// Проверка валидности
		if (isInvalid.h || isInvalid.m || isInvalid.s || validation.exceedsMax) {
			alert('Пожалуйста, исправьте ошибки ввода времени');
			return;
		}

		if (totalSeconds > 86400) {
			alert('Максимальное значение таймера - 86400 секунд (24 часа)');
			return;
		}
		await saveChanges("s_timer", totalSeconds);
	}

	$effect(() => {
		loadTelemetry()
	})

	$effect(() => {
		if (telemetry?.countdown) {
			// При получении данных преобразуем строку времени в объект
			draftTime = parseTimeString(telemetry.countdown);
		}
	})

	$effect(() => {
		if (!isEditing) return;

		const originalOverflow = document.body.style.overflow;
		document.body.style.overflow = 'hidden';

		return () => {
			document.body.style.overflow = originalOverflow;
		};
	});

	function handleSave() {
		isEditing = false;
		if (onConfirm) {
			onConfirm(); // Вызываем колбэк подтверждения
		} else {
			modals.close(); // Fallback
		}
	}

	// Обработчик отмены - исправлено
	function handleCancel() {
		isEditing = false;
		if (onCancel) {
			onCancel(); // Вызываем колбэк отмены
		} else {
			modals.close(); // Fallback
		}
	}
</script>

<Portal>
	<div class="modal-overlay fade-in"></div>
	<div
		role="dialog"
		class="modal-dialog"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div class="modal-content">
			<h2 class="modal-title">
				Настройки таймера
			</h2>

			<div class="modal-body">
				{#if telemetry}
					<!-- Отладочная информация -->
					<!-- <pre>{JSON.stringify(telemetry, null, 2)}</pre> -->

					{#if telemetry.tp1_target}
						<div class="input-group">
							<span class="input-label">Целевая температура, °C</span>
							<div class="input-wrapper">
								<input
									id='s_temp'
									type="number"
									value={telemetry.tp1_target}
									class="input-field"
									step=0.1
									min="0"
									max="110.0"
								/>
							</div>
						</div>
					{/if}
					{#if telemetry.tp2_target}
						<div class="input-group">
							<span class="input-label">Целевая температура, °C</span>
							<div class="input-wrapper">
								<input
									id='hysteresis'
									type="number"
									value={telemetry.tp2_target}
									class="input-field"
									step=0.1
									min="0"
									max="110.0"
								/>
							</div>
						</div>
					{/if}
					{#if telemetry.common.hysteresis}
						<div class="input-group">
							<span class="input-label">Гистерезис, °C</span>
							<div class="input-wrapper">
								<input
									id='hysteresis'
									type="number"
									value={telemetry.common.hysteresis}
									class="input-field"
									step=0.1
									min="0"
									max="110.0"
								/>
							</div>
						</div>
					{/if}
					{#if telemetry.decrement}
						<div class="input-group">
							<span class="input-label">Декремент, °C</span>
							<div class="input-wrapper">
								<input
									id='hearts'
									type="number"
									value={telemetry.period}
									class="input-field"
									step=0.1
									min="0"
									max="110.0"
								/>
							</div>
						</div>
					{/if}
					{#if telemetry.open}
						<div class="input-group">
							<span class="input-label">Время открытия клапана, Сек</span>
							<div class="input-wrapper">
								<input
									id='hearts'
									type="number"
									value={telemetry.open}
									class="input-field"
									step=0.1
									min="0"
									max="110.0"
								/>
							</div>
						</div>
					{/if}
					{#if telemetry.period}
						<div class="input-group">
							<span class="input-label">Период открытия клапана, Сек</span>
							<div class="input-wrapper">
								<input
									id='hearts'
									type="number"
									value={telemetry.period}
									class="input-field"
									step=0.1
									min="0"
									max="110.0"
								/>
							</div>
						</div>
					{/if}
					{#if telemetry.countdown}
						<div class="input-group">
							<span class="input-label">Таймер</span>
							<div class="time-inputs">
								<div class="time-input-group">
									<label class="time-input-label">
										Часы
										<input
											type="number"
											bind:value={draftTime.h}
											step={hourStep}
											class="time-input {isInvalid.h || validation.exceedsMax ? 'input-error' : ''}"
											min="0"
											max="23"
										/>
									</label>
								</div>
								<span class="time-separator">:</span>
								<div class="time-input-group">
									<label class="time-input-label">
										Минуты
										<input
											type="number"
											bind:value={draftTime.m}
											step={minuteStep}
											class="time-input {isInvalid.m || validation.exceedsMax ? 'input-error' : ''}"
											min="0"
											max="59"
										/>
									</label>
								</div>
								<span class="time-separator">:</span>
								<div class="time-input-group">
									<label class="time-input-label">
										Секунды
										<input
											type="number"
											bind:value={draftTime.s}
											class="time-input {isInvalid.s || validation.exceedsMax ? 'input-error' : ''}"
											min="0"
											max="59"
										/>
									</label>
								</div>
								<button
									class="btn btn-primary save-timer-btn"
									onclick={handleSaveTimer}
									disabled={isInvalid.h || isInvalid.m || isInvalid.s || validation.exceedsMax}
								>
									<Check class="btn-icon" />
									<span>Сохранить</span>
								</button>
							</div>
							<div class="time-info">
								<span>Текущее значение: {telemetry.countdown}</span>
								<span>Всего секунд: {timeToSeconds(draftTime)}</span>
								{#if validation.exceedsMax}
									<span class="error-text">Превышено максимальное значение (86400 секунд)</span>
								{/if}
							</div>
						</div>
					{/if}
				{/if}
			</div>
			<div class="modal-divider"></div>
			<div class="modal-actions">
				<button
					class="btn btn-primary"
					onclick={handleSave}
				>
					<Check class="btn-icon" />
					<span>Сохранить все</span>
				</button>
				<button
					class="btn btn-warning"
					onclick={handleCancel}
				>
					<Cancel class="btn-icon" />
					<span>Отменить</span>
				</button>
			</div>
		</div>
	</div>
</Portal>

<style lang="scss">
  .time-info {
    margin-top: 10px;
    display: flex;
    flex-direction: row;
		justify-content: space-between;
    gap: 5px;
    font-size: 0.9em;
    color: #666;
  }

  .error-text {
    color: #dc2626;
    font-weight: 500;
  }

  .save-timer-btn {
    margin-top: 10px;
    width: 20%;

    &:disabled {
      opacity: 0.6;
      cursor: not-allowed;
    }
  }

  .input-error {
    border-color: #dc2626 !important;
    background-color: #fef2f2;
  }
</style>