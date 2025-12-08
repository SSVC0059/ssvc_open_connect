<script lang="ts">
	import Pencil from '~icons/tabler/pencil';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

	import Portal from 'svelte-portal';

	let {
		seconds,
		onSave,
		max,
		step = 1
	} = $props<{
		seconds: number;
		onSave: (seconds: number) => void;
		max?: number;
		step?: number;
	}>();

	let isEditing = $state(false);
	let draftTime = $state({ h: 0, m: 0, s: 0 });
	// Вычисляемые свойства
	let showSeconds = $state(true);
	let hourStep = $state(1);
	let minuteStep = $state(1);

	$effect(() => {
		// Обновляем вычисляемые свойства при изменении step
		showSeconds = step < 60 || step % 60 !== 0;
		hourStep = Math.floor(step / 3600) || 1;
		minuteStep = Math.floor((step % 3600) / 60) || (step >= 60 ? 1 : 0);
	});

	// Парсинг времени
	const parseTime = (totalSeconds: number) => {
		return {
			h: Math.floor(totalSeconds / 3600),
			m: Math.floor((totalSeconds % 3600) / 60),
			s: showSeconds ? totalSeconds % 60 : 0
		};
	};

	// Валидация введенных значений
	const isInvalid = $derived({
		h: draftTime.h < 0,
		m: draftTime.m < 0 || draftTime.m > 59,
		s: draftTime.s < 0 || draftTime.s > 59
	});

	// Форматирование времени с ведущими нулями
	const formatTime = (value: number) => value.toString().padStart(2, '0');

	// Проверка на кратность и максимальное значение
	const validateTime = (time: { h: number; m: number; s: number }) => {
		const totalSeconds = time.h * 3600 + time.m * 60 + time.s;
		return {
			exceedsMax: max !== undefined && totalSeconds > max,
			notMultiple: totalSeconds % step !== 0
		};
	};

	const validation = $derived(validateTime(draftTime));
	const hasErrors = $derived(
		isInvalid.h || isInvalid.m || isInvalid.s || validation.exceedsMax || validation.notMultiple
	);

	// Обработчик изменения полей с учетом кратности
	function handleTimeChange(field: 'h' | 'm' | 's', value: number) {
		draftTime[field] = value;
		const total = draftTime.h * 3600 + draftTime.m * 60 + draftTime.s;
		if (step && total % step !== 0) {
			// Корректируем до ближайшего кратного значения
			const adjusted = Math.round(total / step) * step;
			draftTime = parseTime(adjusted);
		}
	}

	// Форматирование шага для сообщения
	const formatStepMessage = () => {
		if (step >= 3600) {
			const hours = Math.floor(step / 3600);
			const minutes = Math.floor((step % 3600) / 60);
			return `${step} секундам (${hours}ч ${minutes}м)`;
		} else if (step >= 60) {
			return `${step} секундам (${Math.floor(step / 60)}м)`;
		}
		return `${step} секундам`;
	};

	let bodyOverflow = '';
	$effect(() => {
		if (isEditing) {
			bodyOverflow = document.body.style.overflow;
			document.body.style.overflow = 'hidden';
		} else {
			document.body.style.overflow = bodyOverflow;
		}
	});
</script>

{#if isEditing}
	<Portal>
		<div class="modal-overlay fade-in"></div>
		<div
			role="dialog"
			class="modal-dialog"
			transition:fly={{ y: 50 }}
			use:focusTrap
		>
			<div class="modal-content">
				<h2 class="modal-title">Изменение времени</h2>
				<div class="modal-divider"></div>
				<div class="modal-body">
					<div class="time-input-container">
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
									/>
								</label>
							</div>
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
							<div class="time-input-group">
								<label class="time-input-label">
									Секунды
									<input
										type="number"
										bind:value={draftTime.s}
										{step}
										class="time-input {isInvalid.s ? 'input-error' : ''}"
										min="0"
										max="59"
									/>
								</label>
							</div>
						</div>
						<div class="validation-message {validation.notMultiple ? 'validation-error' : 'validation-info'}">
							Значение должно быть кратно {formatStepMessage()}
						</div>
					</div>
				</div>
				<div class="modal-divider"></div>
				<div class="modal-actions">
					<button
						class="btn btn-primary"
						onclick={() => {
							if (!hasErrors) {
								onSave(draftTime.h * 3600 + draftTime.m * 60 + draftTime.s);
								isEditing = false;
							}
						}}
						disabled={hasErrors}
					>
						<Check class="btn-icon" />
						<span>Сохранить</span>
					</button>
					<button
						class="btn btn-warning"
						onclick={() => {
							isEditing = false;
						}}
					>
						<Cancel class="btn-icon" />
						<span>Отменить</span>
					</button>
				</div>
			</div>
		</div>
	</Portal>
{/if}

<div class="values-container">
	<span class="value-text">
		{formatTime(parseTime(seconds).h)}:
		{formatTime(parseTime(seconds).m)}:
		{formatTime(parseTime(seconds).s)}
	</span>
	<button
		class="edit-button"
		onclick={() => {
			draftTime = parseTime(seconds);
			isEditing = true;
		}}
	>
		<Pencil />
	</button>
</div>
