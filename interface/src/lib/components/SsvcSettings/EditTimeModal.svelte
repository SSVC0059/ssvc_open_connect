<script lang="ts">
	import Pencil from '~icons/tabler/pencil';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

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
	<div class="fixed inset-0 z-40 bg-black/50 backdrop-blur-sm fade-in"></div>
	<div
		role="dialog"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div
			class="bg-base-100 shadow-secondary/30 rounded-box pointer-events-auto flex max-h-full min-w-fit max-w-md flex-col justify-between p-4 shadow-lg"
		>
			<h2 class="text-base-content text-center text-2xl font-bold">Изменение времени</h2>
			<div class="divider my-2"></div>
			<div class="overflow-y-auto space-y-4 p-4">
				<div class="flex flex-col gap-4 items-center">
					<div class="flex gap-2">
						<div class="flex flex-col gap-2">
							<label class="font-medium text-center"
								>Часы
								<input
									type="number"
									bind:value={draftTime.h}
									step={hourStep}
									class="w-20 px-2 py-1 border rounded transition-colors {isInvalid.h ||
									validation.exceedsMax
										? 'bg-red-100 border-red-500'
										: ''}"
									min="0"
								/>
							</label>
						</div>
						<span class="text-2xl self-end">:</span>
						<div class="flex flex-col gap-2">
							<label class="font-medium text-center"
								>Минуты
								<input
									type="number"
									bind:value={draftTime.m}
									step={minuteStep}
									class="w-20 px-2 py-1 border rounded transition-colors {isInvalid.m ||
									validation.exceedsMax
										? 'bg-red-100 border-red-500'
										: ''}"
									min="0"
									max="59"
								/>
							</label>
						</div>
						<span class="text-2xl self-end">:</span>
						<div class="flex flex-col gap-2">
							<label class="font-medium text-center"
								>Секунды
								<input
									type="number"
									bind:value={draftTime.s}
									{step}
									class="w-20 px-2 py-1 border rounded transition-colors {isInvalid.s
										? 'bg-red-100 border-red-500'
										: ''}"
									min="0"
									max="59"
								/>
							</label>
						</div>
					</div>
					<div class="text-sm mt-2 {validation.notMultiple ? 'text-red-500' : 'text-gray-500'}">
						Значение должно быть кратно {formatStepMessage()}
					</div>
				</div>
			</div>
			<div class="divider my-2"></div>
			<div class="flex flex-wrap justify-end gap-2">
				<button
					class="btn btn-primary inline-flex flex-none items-center"
					onclick={() => {
						if (!hasErrors) {
							onSave(draftTime.h * 3600 + draftTime.m * 60 + draftTime.s);
							isEditing = false;
						}
					}}
					disabled={hasErrors}
				>
					<Check class="mr-2 h-5 w-5" />
					<span>Сохранить</span>
				</button>
				<button
					class="btn btn-warning text-warning-content inline-flex flex-none items-center"
					onclick={() => {
						isEditing = false;
					}}
				>
					<Cancel class="mr-2 h-5 w-5" />
					<span>Отменить</span>
				</button>
			</div>
		</div>
	</div>
{/if}

<div class="flex gap-2 items-center">
	<span class="font-mono whitespace-nowrap">
		{formatTime(parseTime(seconds).h)}:
		{formatTime(parseTime(seconds).m)}:
		{formatTime(parseTime(seconds).s)}
	</span>
	<button
		class="text-blue-500 hover:text-blue-700 flex-shrink-0"
		onclick={() => {
			draftTime = parseTime(seconds);
			isEditing = true;
		}}
	>
		<Pencil />
	</button>
</div>
