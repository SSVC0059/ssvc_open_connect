<svelte:options runes />

<script lang="ts">
	import { postXkc25Calibrate } from '$lib/api/ssvcApi';
	import type {
		Xkc25CalibrationStep,
		Xkc25SensorConfig,
		Xkc25SensorStatus,
		Xkc25StartGuardMode,
		Xkc25StatusResponse,
		Xkc25TriggerAction
	} from '$lib/types/ssvc';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import CircleCheck from '~icons/tabler/circle-check';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

	let {
		disabled = false,
		sensors = $bindable<Xkc25SensorConfig[]>([]),
		triggerAction = $bindable<Xkc25TriggerAction>(1),
		sharedTankIndex = $bindable(0),
		startGuardMode = $bindable<Xkc25StartGuardMode>(1),
		status = null,
		/** В редакторе есть несохранённые правки подключения: слоты прошивки ещё не совпадают с UI. */
		dirty = false,
		/** Идёт шаг калибровки: родитель на это время приостанавливает опрос статуса. */
		busy = $bindable(false),
		/** Последний опрос статуса не удался: живые значения на экране устарели. */
		stale = false,
		onStatusRefresh
	}: {
		disabled?: boolean;
		sensors: Xkc25SensorConfig[];
		triggerAction: Xkc25TriggerAction;
		sharedTankIndex: number;
		startGuardMode: Xkc25StartGuardMode;
		status?: Xkc25StatusResponse | null;
		dirty?: boolean;
		busy: boolean;
		stale?: boolean;
		onStatusRefresh?: () => void | Promise<void>;
	} = $props();

	const MAX_SENSORS = 3;
	/** Дефолтные пины платы: SSVC_XKC25_GPIO_1/2/3 в platformio.ini. */
	const DEFAULT_GPIOS = [4, 5, 6];
	const GPIO_PINS = Array.from({ length: 40 }, (_, pin) => pin);
	const ZONE_LABELS = ['Приёмная ёмкость 1', 'Приёмная ёмкость 2', 'Приёмная ёмкость 3'];

	/**
	 * Пины, занятые другими подсистемами платы (I²C, UART, 1-Wire, линии тревоги).
	 * Прошивка допускает любой пин 0..39, поэтому конфликт не блокируется, а помечается:
	 * схема подключения зависит от платы.
	 */
	const RESERVED_GPIOS: Record<number, string> = {
		8: 'линия тревоги',
		11: 'I²C SCL',
		12: 'I²C SDA',
		13: 'линия тревоги',
		15: '1-Wire',
		16: '1-Wire',
		17: 'UART TX',
		18: 'UART RX'
	};

	type WizardStep = 'without' | 'with' | 'verify';

	let wizardIndex = $state<number | null>(null);
	let wizardStep = $state<WizardStep>('without');
	let wizardTone = $state<'ok' | 'warn' | 'error' | ''>('');
	let wizardMessage = $state('');
	let wizardChecklist = $state<string[]>([]);

	const guardHint = $derived.by(() => {
		if (startGuardMode === 0) return 'Проверок нет: старт выполняется всегда.';
		if (startGuardMode === 1) return 'Старт разрешён, причина возвращается как предупреждение.';
		return 'Старт отклоняется, пока включён хотя бы один некалиброванный датчик. Если датчик неисправен — выключите его или выберите «предупреждать»/«не проверять».';
	});

	/** Пауза и остановка — взаимоисключающие режимы, поэтому это выбор, а не два переключателя. */
	const actionHint = $derived.by(() => {
		if (triggerAction === 0) return 'Датчик только сообщает о заполнении: отбор продолжается.';
		if (triggerAction === 1) return 'Отбор встаёт на паузу; когда сигнал снят, отбор возобновляется.';
		return 'Ректификация прерывается: процесс нужно запускать заново.';
	});

	/** Есть ли датчик, который физически переставляется между ёмкостями. */
	const hasSharedSensor = $derived(sensors.some((sensor) => sensor.sharedAcrossTanks === true));

	function reservedNote(pin: number): string | undefined {
		return RESERVED_GPIOS[pin];
	}

	/** Номер другого датчика на том же пине: конфликт виден ещё до сохранения. */
	function duplicateGpioOwner(index: number): number | null {
		const sensor = sensors[index];
		if (!sensor) return null;
		const duplicate = sensors.findIndex(
			(candidate, i) => i !== index && Number(candidate.gpio) === Number(sensor.gpio)
		);
		return duplicate === -1 ? null : duplicate + 1;
	}

	/**
	 * Живое состояние сопоставляется по пину, а не по порядку: после добавления или
	 * удаления датчика нумерация в редакторе и в прошивке расходится до сохранения.
	 */
	function statusFor(sensor: Xkc25SensorConfig): Xkc25SensorStatus | undefined {
		return status?.sensors?.find((candidate) => candidate.gpio === sensor.gpio);
	}

	function rawLevelText(rawLevel: number): string {
		if (rawLevel === 1) return 'высокий (1)';
		if (rawLevel === 0) return 'низкий (0)';
		return 'нет данных';
	}

	function nextFreeGpio(): number {
		const used = new Set(sensors.map((sensor) => sensor.gpio));
		const preferred = DEFAULT_GPIOS.find((pin) => !used.has(pin));
		if (preferred != null) return preferred;
		const free = GPIO_PINS.find((pin) => !used.has(pin));
		return free ?? DEFAULT_GPIOS[0];
	}

	function nextFreeZone(): number {
		const used = new Set(sensors.map((sensor) => sensor.zoneIndex));
		for (let zone = 0; zone < ZONE_LABELS.length; zone += 1) {
			if (!used.has(zone)) return zone;
		}
		return 0;
	}

	function addSensor() {
		if (sensors.length >= MAX_SENSORS) return;
		sensors = [
			...sensors,
			{
				enabled: true,
				gpio: nextFreeGpio(),
				activeLow: true,
				zoneIndex: nextFreeZone(),
				calibrated: false
			}
		];
	}

	function removeSensor(index: number) {
		sensors = sensors.filter((_, i) => i !== index);
		// Нумерация карточек сдвинулась: открытый мастер больше не относится к тому же датчику.
		closeWizard();
	}

	function confirmRemoveSensor(index: number) {
		const sensor = sensors[index];
		const details = sensor
			? ` — GPIO ${sensor.gpio}, ${ZONE_LABELS[sensor.zoneIndex] ?? 'зона не выбрана'}`
			: '';
		modals.open(ConfirmDialog, {
			title: `Удаление датчика ${index + 1}`,
			message: `Удалить датчик ${index + 1}${details}? Калибровка этого датчика будет потеряна.`,
			labels: {
				cancel: { label: 'Отмена', icon: Cancel },
				confirm: { label: 'Удалить', icon: Check }
			},
			onConfirm: () => {
				modals.close();
				removeSensor(index);
			}
		});
	}

	/**
	 * Изменение пина, зоны, полярности или включения делает прежнюю калибровку
	 * недостоверной (см. план 6.0.2), поэтому здесь она сбрасывается сразу —
	 * так же её снимет прошивка при сохранении.
	 */
	function updateSensor(index: number, patch: Partial<Xkc25SensorConfig>) {
		const current = sensors[index];
		if (!current) return;
		const wiringChanged =
			(patch.enabled !== undefined && patch.enabled !== current.enabled) ||
			(patch.gpio !== undefined && patch.gpio !== current.gpio) ||
			(patch.activeLow !== undefined && patch.activeLow !== current.activeLow) ||
			(patch.zoneIndex !== undefined && patch.zoneIndex !== current.zoneIndex);
		const next: Xkc25SensorConfig = { ...current, ...patch };
		if (wiringChanged) {
			next.calibrated = false;
			next.calibratedAt = 0;
		}
		sensors = sensors.map((sensor, i) => (i === index ? next : sensor));
	}

	function onGpioChange(index: number, rawValue: string) {
		const pin = Number(rawValue);
		if (!Number.isInteger(pin) || pin < 0 || pin > 39) return;
		updateSensor(index, { gpio: pin });
	}

	function onZoneChange(index: number, rawValue: string) {
		const zoneIndex = Number(rawValue);
		if (!Number.isInteger(zoneIndex) || zoneIndex < 0 || zoneIndex >= ZONE_LABELS.length) return;
		updateSensor(index, { zoneIndex });
	}

	function onGuardChange(rawValue: string) {
		const mode = Number(rawValue);
		if (mode !== 0 && mode !== 1 && mode !== 2) return;
		startGuardMode = mode;
	}

	/** Пауза и остановка взаимоисключающие: выбран ровно один режим реакции. */
	function onTriggerActionChange(rawValue: string) {
		const action = Number(rawValue);
		if (action !== 0 && action !== 1 && action !== 2) return;
		triggerAction = action;
	}

	function onSharedTankChange(rawValue: string) {
		const zoneIndex = Number(rawValue);
		if (!Number.isInteger(zoneIndex) || zoneIndex < 0 || zoneIndex >= ZONE_LABELS.length) return;
		sharedTankIndex = zoneIndex;
	}

	/**
	 * Переносит состояние датчика из ответа прошивки: после калибровки меняются
	 * вычисленная полярность и признак калибровки — их нужно сразу отразить в карточке.
	 */
	function applySensorFromStatus(index: number, live: Xkc25SensorStatus | undefined) {
		if (!live) return;
		const current = sensors[index];
		if (!current) return;
		sensors = sensors.map((sensor, i) =>
			i === index
				? {
						...sensor,
						activeLow: live.activeLow ?? sensor.activeLow,
						calibrated: live.calibrated ?? sensor.calibrated,
						calibratedAt: live.calibratedAt ?? sensor.calibratedAt,
						levelWithoutLiquid: live.levelWithoutLiquid ?? sensor.levelWithoutLiquid,
						levelWithLiquid: live.levelWithLiquid ?? sensor.levelWithLiquid
					}
				: sensor
		);
	}

	function openWizard(index: number) {
		wizardIndex = index;
		wizardStep = 'without';
		wizardTone = '';
		wizardMessage = '';
		wizardChecklist = [];
	}

	function goToWizardStep(step: WizardStep) {
		wizardStep = step;
		wizardTone = '';
		wizardMessage = '';
		wizardChecklist = [];
	}

	/** Возврат на предыдущий шаг: измерение предыдущего шага при этом не сбрасывается. */
	function wizardBack() {
		if (wizardStep === 'with') {
			goToWizardStep('without');
		} else if (wizardStep === 'verify') {
			goToWizardStep('with');
		}
	}

	function closeWizard() {
		wizardIndex = null;
		busy = false;
		wizardTone = '';
		wizardMessage = '';
		wizardChecklist = [];
	}

	async function runCalibrationStep(step: Xkc25CalibrationStep) {
		const index = wizardIndex;
		if (index == null || busy) return;
		if (dirty) {
			// Слоты прошивки ещё не совпадают с редактором: шаг ушёл бы не тому датчику.
			wizardTone = 'warn';
			wizardMessage =
				'Датчики изменены, но не сохранены. Сохраните настройки, перезагрузите контроллер и повторите калибровку.';
			return;
		}
		busy = true;
		wizardTone = '';
		wizardMessage = '';
		wizardChecklist = [];

		const outcome = await postXkc25Calibrate(
			index,
			step,
			step === 'verify_return' ? 5000 : undefined
		);

		busy = false;
		const body = outcome.body;
		if (!body) {
			wizardTone = 'error';
			wizardMessage = outcome.error
				? `Запрос не выполнен: ${outcome.error}`
				: `Контроллер ответил HTTP ${outcome.status} без тела ответа.`;
			return;
		}

		wizardMessage = body.message || body.error || body.code || 'Шаг калибровки отклонён.';
		wizardChecklist = body.checklist ?? [];
		// Прошивка могла уточнить полярность датчика — сразу показываем это в карточке.
		applySensorFromStatus(index, body.sensor);

		if (outcome.status !== 200) {
			wizardTone = 'error';
			return;
		}

		if (step === 'without_liquid') {
			wizardTone = 'ok';
			wizardStep = 'with';
		} else if (step === 'with_liquid') {
			if (body.completed) {
				wizardTone = 'ok';
				wizardStep = 'verify';
			} else {
				// Уровни совпали: датчик не отреагировал на изменение уровня жидкости.
				wizardTone = 'warn';
			}
		} else {
			wizardTone = body.code === 'verify_failed' ? 'warn' : 'ok';
		}

		await onStatusRefresh?.();
	}
</script>

<div class="xkc-section">
	<div class="editor-step">
		<div class="step-title">1. Датчики уровня</div>
		<p class="step-hint">
			XKC-25-NPN — бесконтактный ёмкостный датчик: крепится на внешнюю стенку неметаллической
			ёмкости (толщина стенки или зазор до 20 мм), питание 5–12 В, выход OUT — на выбранный GPIO.
			До трёх датчиков, каждый на своей приёмной ёмкости.
		</p>

		{#if dirty}
			<p class="xkc-warn">
				<AlertTriangle class="h-4 w-4" aria-hidden="true" />
				<span>
					Настройки датчиков изменены, но не сохранены: калибровка привязана к сохранённой
					конфигурации. Сохраните настройки и перезагрузите контроллер.
				</span>
			</p>
		{/if}

		{#if sensors.length === 0}
			<p class="xkc-empty">Датчики не добавлены.</p>
		{:else}
			<ul class="xkc-sensor-list">
				{#each sensors as sensor, idx (idx)}
					{@const live = statusFor(sensor)}
					{@const reserved = reservedNote(sensor.gpio)}
					{@const pinTakenByOther = duplicateGpioOwner(idx)}
					<li class="xkc-sensor-card">
						<div class="xkc-sensor-head">
							<span class="xkc-sensor-title">Датчик {idx + 1}</span>
							<div class="xkc-sensor-badges">
								<span class="badge badge-sm {sensor.enabled ? 'badge-success' : 'badge-ghost'}">
									{sensor.enabled ? 'Включён' : 'Выключен'}
								</span>
								<span class="badge badge-sm {sensor.calibrated ? 'badge-success' : 'badge-warning'}">
									{sensor.calibrated ? 'Откалиброван' : 'Не откалиброван'}
								</span>
							</div>
						</div>

						<div class="xkc-sensor-grid">
							<label class="form-control">
								<span class="label-text">Пин выхода OUT</span>
								<select
									class="select select-bordered select-sm w-full"
									value={sensor.gpio}
									onchange={(e) => onGpioChange(idx, e.currentTarget.value)}
									disabled={disabled || busy}
									aria-label="GPIO датчика {idx + 1}"
								>
									{#each GPIO_PINS as pin (pin)}
										<option value={pin}>
											GPIO {pin}{RESERVED_GPIOS[pin] ? ` — ${RESERVED_GPIOS[pin]}` : ''}
										</option>
									{/each}
								</select>
							</label>

							<label class="form-control">
								<span class="label-text">Приёмная ёмкость</span>
								<select
									class="select select-bordered select-sm w-full"
									value={sensor.sharedAcrossTanks ? sharedTankIndex : sensor.zoneIndex}
									onchange={(e) => onZoneChange(idx, e.currentTarget.value)}
									disabled={disabled || busy || sensor.sharedAcrossTanks === true}
									aria-label="Приёмная ёмкость датчика {idx + 1}"
								>
									{#each ZONE_LABELS as label, zoneIndex (zoneIndex)}
										<option value={zoneIndex}>{label}</option>
									{/each}
								</select>
								{#if sensor.sharedAcrossTanks}
									<span class="label-text-alt">
										Датчик переносной: ёмкость задаёт общая настройка «Датчик сейчас на ёмкости».
									</span>
								{/if}
							</label>
						</div>

						<label class="label-cb">
							<input
								type="checkbox"
								class="toggle toggle-primary toggle-sm"
								checked={sensor.enabled}
								onchange={(e) => updateSensor(idx, { enabled: e.currentTarget.checked })}
								disabled={disabled || busy}
								aria-label="Датчик {idx + 1}: подключён и активен"
							/>
							<span>Датчик подключён и активен</span>
						</label>

						<label class="label-cb">
							<input
								type="checkbox"
								class="toggle toggle-primary toggle-sm"
								checked={sensor.sharedAcrossTanks === true}
								onchange={(e) => updateSensor(idx, { sharedAcrossTanks: e.currentTarget.checked })}
								disabled={disabled || busy}
								aria-label="Датчик {idx + 1}: один прибор используется на всех ёмкостях"
							/>
							<span>Один датчик на все ёмкости (переставляется вручную)</span>
						</label>
						{#if sensor.sharedAcrossTanks}
							<p class="step-hint">
								Датчик переставляется с ёмкости на ёмкость по фракциям: в общих опциях
								укажите, на какой ёмкости он стоит сейчас. Калибровка проходится один раз и
								при перестановке не сбрасывается.
							</p>
						{/if}

						<label class="label-cb">
							<input
								type="checkbox"
								class="toggle toggle-primary toggle-sm"
								checked={sensor.activeLow}
								onchange={(e) => updateSensor(idx, { activeLow: e.currentTarget.checked })}
								disabled={disabled || busy}
								aria-label="Датчик {idx + 1}: жидкость определяется низким уровнем на пине"
							/>
							<span>«Жидкость есть» = низкий уровень на пине (LOW)</span>
						</label>
						<p class="step-hint">
							Полярность определяет калибровка. Ручное переключение полярности, а также смена пина,
							зоны или включения сбрасывают калибровку датчика — после таких правок пройдите
							калибровку заново.
						</p>

						{#if live && live.live}
							<div class="xkc-live">
								<span class="xkc-live-item">
									Сигнал на пине: <strong>{rawLevelText(live.rawLevel)}</strong>
								</span>
								<span class="xkc-live-item" class:filled={live.filled}>
									{live.filled ? 'Жидкость напротив датчика есть' : 'Жидкости напротив датчика нет'}
								</span>
								{#if !live.calibrated}
									<span class="xkc-live-hint">
										Датчик не откалиброван: показан сырой уровень пина.
									</span>
								{/if}
								{#if stale}
									<span class="xkc-live-hint">
										Связь с контроллером потеряна: значения могли устареть.
									</span>
								{/if}
							</div>
						{:else if status}
							<p class="xkc-live-hint">
								{sensor.enabled
									? 'Данные недоступны: сохраните настройки и перезагрузите контроллер.'
									: 'Датчик выключен.'}
							</p>
						{/if}

						{#if !sensor.calibrated}
							<p class="xkc-warn">
								<AlertTriangle class="h-4 w-4" aria-hidden="true" />
								<span>Работа датчика не проверена: калибровка не пройдена.</span>
							</p>
						{/if}
						{#if reserved}
							<p class="xkc-warn xkc-warn-soft">
								<AlertTriangle class="h-4 w-4" aria-hidden="true" />
								<span>GPIO {sensor.gpio} зарезервирован ({reserved}). Проверьте, что пин свободен.</span>
							</p>
						{/if}
						{#if pinTakenByOther}
							<p class="xkc-warn xkc-warn-soft">
								<AlertTriangle class="h-4 w-4" aria-hidden="true" />
								<span>
									GPIO {sensor.gpio} уже занят датчиком {pinTakenByOther}. У каждого датчика должен
									быть свой пин.
								</span>
							</p>
						{/if}

						<div class="xkc-sensor-actions">
							<button
								type="button"
								class="btn btn-sm btn-outline"
								onclick={() => openWizard(idx)}
								disabled={disabled || dirty || busy || !sensor.enabled}
							>
								Калибровать
							</button>
							<button
								type="button"
								class="btn btn-sm btn-ghost"
								onclick={() => confirmRemoveSensor(idx)}
								disabled={disabled || busy}
							>
								Удалить
							</button>
						</div>
						{#if !sensor.enabled}
							<p class="step-hint">Включите датчик, чтобы пройти калибровку.</p>
						{/if}

						{#if wizardIndex === idx}
							<div class="xkc-wizard" aria-busy={busy}>
								<div class="xkc-wizard-head">
									<span class="xkc-wizard-title">Калибровка датчика {idx + 1}</span>
									<button type="button" class="btn btn-xs btn-ghost" onclick={closeWizard}>
										Закрыть
									</button>
								</div>

								<ol class="xkc-wizard-steps" aria-label="Шаги калибровки датчика">
									<li
										class:done={wizardStep !== 'without'}
										class:active={wizardStep === 'without'}
										aria-current={wizardStep === 'without' ? 'step' : undefined}
									>
										1. Без жидкости
									</li>
									<li
										class:done={wizardStep === 'verify'}
										class:active={wizardStep === 'with'}
										aria-current={wizardStep === 'with' ? 'step' : undefined}
									>
										2. С жидкостью
									</li>
									<li
										class:active={wizardStep === 'verify'}
										aria-current={wizardStep === 'verify' ? 'step' : undefined}
									>
										3. Проверка возврата
									</li>
								</ol>

								{#if wizardStep === 'without'}
									<p class="step-hint">
										Опустите уровень жидкости ниже метки датчика и нажмите кнопку: прошивка снимет
										серию чтений и запомнит состояние без жидкости.
									</p>
									<button
										type="button"
										class="btn btn-sm btn-primary"
										onclick={() => runCalibrationStep('without_liquid')}
										disabled={busy}
									>
										{#if busy}<span class="loading loading-spinner loading-xs" aria-hidden="true"></span>{/if}
										Запомнить состояние без жидкости
									</button>
								{:else if wizardStep === 'with'}
									<p class="step-hint">
										Поднимите уровень жидкости выше метки датчика, подождите до 500 мс (время
										реакции датчика) и нажмите кнопку.
									</p>
									<button
										type="button"
										class="btn btn-sm btn-primary"
										onclick={() => runCalibrationStep('with_liquid')}
										disabled={busy}
									>
										{#if busy}<span class="loading loading-spinner loading-xs" aria-hidden="true"></span>{/if}
										Запомнить состояние с жидкостью
									</button>
									<button
										type="button"
										class="btn btn-sm btn-ghost"
										onclick={wizardBack}
										disabled={busy}
									>
										Назад
									</button>
								{:else}
									<p class="step-hint">
										Калибровка сохранена. Рекомендуется проверить возврат: опустите уровень ниже
										метки датчика и нажмите кнопку.
									</p>
									<div class="xkc-wizard-actions">
										<button
											type="button"
											class="btn btn-sm btn-outline"
											onclick={() => runCalibrationStep('verify_return')}
											disabled={busy}
										>
											{#if busy}<span class="loading loading-spinner loading-xs" aria-hidden="true"></span>{/if}
											Проверить возврат
										</button>
										<button type="button" class="btn btn-sm btn-ghost" onclick={closeWizard}>
											Готово
										</button>
										<button
											type="button"
											class="btn btn-sm btn-ghost"
											onclick={wizardBack}
											disabled={busy}
										>
											Назад
										</button>
									</div>
								{/if}

								{#if wizardMessage}
									<div
										class="alert py-2 {wizardTone === 'ok' ? 'alert-success' : ''}{wizardTone === 'warn' ? 'alert-warning' : ''}{wizardTone === 'error' ? 'alert-error' : ''}{wizardTone === '' ? 'alert-info' : ''}"
										role="status"
										aria-live="polite"
									>
										<span>
											{#if wizardTone === 'ok'}
												<CircleCheck class="mr-1 inline h-4 w-4" aria-hidden="true" />
											{/if}
											{wizardMessage}
										</span>
									</div>
								{/if}

								{#if wizardChecklist.length > 0}
									<div class="xkc-checklist">
										<span class="xkc-checklist-title">Проверьте подключение:</span>
										<ul>
											{#each wizardChecklist as item}
												<li>{item}</li>
											{/each}
										</ul>
									</div>
								{/if}
							</div>
						{/if}
					</li>
				{/each}
			</ul>
		{/if}

		<div class="xkc-add-row">
			<button
				type="button"
				class="btn btn-outline btn-sm"
				onclick={addSensor}
				disabled={disabled || busy || sensors.length >= MAX_SENSORS}
			>
				Добавить датчик
			</button>
			<span class="step-hint">Добавлено {sensors.length} из {MAX_SENSORS}</span>
		</div>
	</div>

	<div class="editor-step">
		<div class="step-title">2. Общие опции</div>
		<label class="form-control w-full max-w-md">
			<span class="label-text">Что делать при срабатывании датчика</span>
			<select
				class="select select-bordered select-sm w-full"
				value={triggerAction}
				onchange={(e) => onTriggerActionChange(e.currentTarget.value)}
				disabled={disabled}
			>
				<option value={0}>Только уведомление</option>
				<option value={1}>Поставить отбор на паузу</option>
				<option value={2}>Прервать ректификацию</option>
			</select>
			<span class="label-text-alt">{actionHint}</span>
		</label>

		{#if hasSharedSensor}
			<label class="form-control w-full max-w-md">
				<span class="label-text">Переносной датчик сейчас стоит на ёмкости</span>
				<select
					class="select select-bordered select-sm w-full"
					value={sharedTankIndex}
					onchange={(e) => onSharedTankChange(e.currentTarget.value)}
					disabled={disabled}
				>
					{#each ZONE_LABELS as label, zoneIndex (zoneIndex)}
						<option value={zoneIndex}>{label}</option>
					{/each}
				</select>
				<span class="label-text-alt">
					Переключите, когда физически переставили датчик: зона сменится без перезагрузки,
					калибровка сохранится.
				</span>
			</label>
		{/if}

		<label class="form-control w-full max-w-md">
			<span class="label-text">Проверка перед стартом, если датчик не откалиброван</span>
			<select
				class="select select-bordered select-sm w-full"
				value={startGuardMode}
				onchange={(e) => onGuardChange(e.currentTarget.value)}
				disabled={disabled}
			>
				<option value={0}>Не проверять (off)</option>
				<option value={1}>Предупреждать, но запускать (warn)</option>
				<option value={2}>Блокировать старт (block)</option>
			</select>
			<span class="label-text-alt">{guardHint}</span>
		</label>

		<p class="step-hint">
			Защита действует на команды через веб-UI, MQTT и ботов. Старт физической кнопкой SSVC-0059
			она предотвратить не может — придёт только уведомление.
		</p>
	</div>

	{#if status && (status.blockStart || status.warnings.length > 0)}
		<div class="editor-step" role="status" aria-live="polite">
			<div class="step-title">3. Состояние проверки старта</div>
			{#if status.blockStart}
				<div class="alert alert-error py-2">
					<span>
						Старт будет отклонён (режим block):
						{status.blockReasons.join('; ')}
					</span>
				</div>
			{/if}
			{#if status.warnings.length > 0}
				<div class="alert alert-warning py-2">
					<ul class="xkc-warning-list">
						{#each status.warnings as warning}
							<li>{warning}</li>
						{/each}
					</ul>
				</div>
			{/if}
		</div>
	{/if}
</div>

<style lang="scss">
	.xkc-section {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
	}

	.editor-step {
		display: flex;
		flex-direction: column;
		gap: 0.6rem;
		border-top: 1px solid hsl(var(--b3));
		padding-top: 0.75rem;
	}

	.editor-step:first-child {
		padding-top: 0;
		border-top: 0;
	}

	.step-title {
		font-size: 0.92rem;
		font-weight: 600;
	}

	.step-hint {
		font-size: 0.86rem;
		color: hsl(var(--bc) / 0.85);
	}

	.label-cb {
		display: inline-flex;
		align-items: center;
		gap: 0.6rem;
		font-size: 0.95rem;
	}

	.xkc-empty {
		font-size: 0.9rem;
		color: hsl(var(--bc) / 0.75);
	}

	.xkc-sensor-list {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
		list-style: none;
		margin: 0;
		padding: 0;
	}

	.xkc-add-row {
		display: flex;
		align-items: center;
		gap: 0.6rem;
		flex-wrap: wrap;
	}

	.xkc-sensor-card {
		display: flex;
		flex-direction: column;
		gap: 0.6rem;
		padding: 0.75rem;
		border: 1px solid hsl(var(--b3));
		border-radius: 0.6rem;
	}

	.xkc-sensor-head {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.5rem;
		flex-wrap: wrap;
	}

	.xkc-sensor-title {
		font-size: 0.95rem;
		font-weight: 600;
	}

	.xkc-sensor-badges {
		display: inline-flex;
		align-items: center;
		gap: 0.3rem;
		flex-wrap: wrap;
	}

	.xkc-sensor-grid {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(min(220px, 100%), 1fr));
		gap: 0.6rem;
	}

	.xkc-sensor-actions {
		display: flex;
		align-items: center;
		gap: 0.35rem;
		flex-wrap: wrap;
	}

	.xkc-live {
		display: flex;
		align-items: center;
		gap: 0.6rem;
		flex-wrap: wrap;
		font-size: 0.86rem;
		padding: 0.4rem 0.6rem;
		border-radius: 0.5rem;
		background: hsl(var(--b2));
	}

	.xkc-live-item.filled {
		font-weight: 600;
		color: hsl(var(--er));
	}

	.xkc-live-hint {
		color: hsl(var(--bc) / 0.7);
	}

	.xkc-warn {
		display: flex;
		align-items: center;
		gap: 0.35rem;
		font-size: 0.86rem;
		color: hsl(var(--wa));
	}

	.xkc-warn-soft {
		color: hsl(var(--bc) / 0.7);
	}

	.xkc-wizard {
		display: flex;
		flex-direction: column;
		gap: 0.6rem;
		padding: 0.75rem;
		border: 1px dashed hsl(var(--b3));
		border-radius: 0.6rem;
		background: hsl(var(--b2) / 0.4);
	}

	.xkc-wizard-head {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.5rem;
	}

	.xkc-wizard-title {
		font-size: 0.92rem;
		font-weight: 600;
	}

	.xkc-wizard-steps {
		display: flex;
		align-items: center;
		gap: 0.5rem;
		flex-wrap: wrap;
		font-size: 0.82rem;
		list-style: none;
		margin: 0;
		padding: 0;
	}

	.xkc-wizard-steps li {
		padding: 0.15rem 0.5rem;
		border-radius: 999px;
		background: hsl(var(--b3));
		color: hsl(var(--bc) / 0.75);
	}

	.xkc-wizard-steps li.active {
		background: hsl(var(--p) / 0.2);
		color: hsl(var(--bc));
		font-weight: 600;
	}

	.xkc-wizard-steps li.done {
		background: hsl(var(--su) / 0.2);
		color: hsl(var(--bc));
	}

	.xkc-wizard-actions {
		display: flex;
		align-items: center;
		gap: 0.35rem;
		flex-wrap: wrap;
	}

	.xkc-checklist {
		font-size: 0.84rem;
		color: hsl(var(--bc) / 0.85);
	}

	.xkc-checklist-title {
		font-weight: 600;
	}

	.xkc-checklist ul {
		margin-top: 0.25rem;
		padding-left: 1.1rem;
		list-style: disc;
	}

	.xkc-warning-list {
		padding-left: 1.1rem;
		list-style: disc;
	}
</style>
