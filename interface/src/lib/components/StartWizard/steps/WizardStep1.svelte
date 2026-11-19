<script lang="ts">
	import type { OcApiFeature, SsvcSettings } from '$lib/types/ssvc';
	import NumberInput from '$lib/components/NumberInput.svelte';
	import { getInfo, infoCacheVersion } from '$lib/api/ssvcApi';
	import {
		PREDEC_VALUES,
		formulaModeHint,
		minHysteresisFor,
		predecLabel,
		predecState
	} from '$lib/actions/rectificationRules';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();

	// Режим «Авто 92+» появился в API 1.9. Возможность запрашиваем у контроллера:
	// при отсутствии метаданных считаем её недоступной, иначе на API 1.7/1.8 поле
	// молча отбросит очередь, а визард покажет сохранение.
	type ApiCheckState = 'checking' | 'ok' | 'failed';
	let apiFeatures = $state<OcApiFeature[]>([]);
	// Состояние проверки /rest/oc/info: отличаем «ещё проверяем» и «не удалось»
	// от подтверждённого отсутствия возможности в API.
	let apiCheckState = $state<ApiCheckState>('checking');

	$effect(() => {
		// Перечитываем при сбросе кеша /rest/oc/info (переподключение): контроллер
		// мог быть заменён или перепрошит, и набор возможностей API изменился.
		void $infoCacheVersion;
		let cancelled = false;
		// Пока ответа нет, контролы прошлого подключения не показываем.
		apiFeatures = [];
		apiCheckState = 'checking';
		void (async () => {
			try {
				const info = await getInfo();
				if (!cancelled) {
					apiFeatures = info?.oc.api_features ?? [];
					apiCheckState = 'ok';
				}
			} catch {
				if (!cancelled) {
					apiFeatures = [];
					apiCheckState = 'failed';
				}
			}
		})();
		return () => {
			cancelled = true;
		};
	});

	const formulaAuto92Available = $derived(
		apiFeatures.find((feature) => feature.name === 'formula_auto92')?.available ?? false
	);
	// Предекремент появился в API 1.9 (SET predec=0/7/13). При отсутствии
	// возможности не показываем рабочий контрол: на 1.7/1.8 SET predec молча
	// отбросится очередью, а мастер покажет сохранение.
	const predecAvailable = $derived(
		apiFeatures.find((feature) => feature.name === 'predec')?.available ?? false
	);

	const capabilityTitle = $derived(
		apiCheckState === 'checking'
			? 'Проверка возможностей API…'
			: apiCheckState === 'failed'
				? 'Не удалось проверить возможности API'
				: 'Требуется API 1.9'
	);

	/** 0,12 → «0,12»: подписи и предупреждения в русской десятичной записи. */
	const formatNumber = (value: number): string => value.toFixed(2).replace('.', ',');

	const predecMinHyst = $derived(minHysteresisFor(settings.predec));
	const hysteresisTooSmall = $derived(
		predecState(settings.predec, settings.hyst) === 'hysteresis-too-small'
	);
	const auto92WithoutPredec = $derived(settings.formula === 2 && settings.predec === 0);

	// Правки — только по явному нажатию и только одного поля: мастер сохранит
	// настройки при переходе дальше, молча менять параметры нельзя.
	function raiseHysteresis() {
		if (predecMinHyst !== null) settings.hyst = predecMinHyst;
	}

	function enablePredec() {
		settings.predec = 7;
	}
</script>

<div class="settings-block">
	<h2 class="settings-title">Основные настройки</h2>
	<div class="settings-group">
		<div class="settings-item read-only">
			<input
				bind:checked={settings.hearts_temp_shift}
				class="settings-checkbox"
				id="heartsTempShift"
				type="checkbox"
			/>
			<label class="input-label" for="heartsTempShift"> Сдвиг температуры отбора </label>
		</div>

		<div class="settings-item read-only">
			<input
				bind:checked={settings.hearts_pause}
				class="settings-checkbox"
				id="heartsPause"
				type="checkbox"
			/>
			<label class="input-label" for="heartsPause"> Пауза на себя </label>
		</div>

		<div class="formula-block">
			<div class="settings-item">
				<div class="settings-item settings-item-wrapper">
					<label class="input-label" for="formulaMode">Формула</label>
					<select
						bind:value={settings.formula}
						class="settings-checkbox"
						id="formulaMode"
					>
						<option value={0}>Выкл</option>
						<option value={1}>Вкл</option>
						<option
							value={2}
							disabled={!formulaAuto92Available}
							title={formulaAuto92Available ? undefined : capabilityTitle}
						>
							Авто 92+
						</option>
					</select>
				</div>

				<div class="formula-input-wrapper" class:active={settings.formula > 0}>
					{#if settings.formula === 2}
						<NumberInput
							value={92}
							disabled
							min={84}
							max={100}
							unit="°С"
							placeholder="Начальная температура"
						/>
					{:else}
						<NumberInput
							bind:value={settings.formula_start_temp}
							min={84}
							max={100}
							unit="°С"
							placeholder="Начальная температура"
						/>
					{/if}
				</div>
			</div>
			<!-- Пояснение режима: без него «Авто 92+» выглядит как обычная формула. -->
			<p class="formula-hint">{formulaModeHint(settings.formula)}</p>
			{#if apiCheckState === 'failed'}
				<p class="formula-hint">
					Не удалось проверить возможности API — закройте и откройте мастер, чтобы
					повторить.
				</p>
			{/if}
		</div>

		<div class="settings-item">
			<label class="input-label" for="predecMode">Предекремент</label>
			<select
				bind:value={settings.predec}
				class="settings-checkbox"
				id="predecMode"
				disabled={!predecAvailable}
				title={predecAvailable ? undefined : capabilityTitle}
			>
				{#each PREDEC_VALUES as value}
					<option {value}>{predecLabel(value)}</option>
				{/each}
			</select>
		</div>

		<!-- Предупреждения не блокируют переход: пользователь может запустить процесс
		     и осознанно, но не должен узнать о проблеме уже после запуска. -->
		{#if auto92WithoutPredec || hysteresisTooSmall}
			<div class="formula-warning" role="status">
				{#if auto92WithoutPredec}
					<p>
						«Авто 92+» не сработает: режим включается после первого предекремента, а
						предекремент выключен. Включите предекремент ({predecLabel(7)} или
						{predecLabel(13)}) или выберите обычную формулу.
					</p>
					{#if predecAvailable}
						<button type="button" class="formula-warning-action" onclick={enablePredec}>
							Включить предекремент {predecLabel(7)}
						</button>
					{/if}
				{/if}
				{#if hysteresisTooSmall && predecMinHyst !== null}
					<p>
						Предекремент {predecLabel(settings.predec)} при гистерезисе
						{formatNumber(settings.hyst)} °C не сработает — контроллер отключит его. Нужен
						гистерезис не меньше {formatNumber(predecMinHyst)} °C.
					</p>
					{#if predecAvailable}
						<button type="button" class="formula-warning-action" onclick={raiseHysteresis}>
							Поднять гистерезис до {formatNumber(predecMinHyst)} °C
						</button>
					{/if}
				{/if}
			</div>
		{/if}

		<div class="settings-item">
			<label class="input-label" for="tankPressure"> Давление в кубе </label>
			<NumberInput
				bind:value={settings.tank_mmhg}
				min={1}
				max={720}
				unit="мм рт. ст."
			/>
		</div>

		<div class="settings-item read-only">
			<label class="input-label" for="tp2Correction"> Поправка ТД2 </label>
			<NumberInput
				bind:value={settings.tp2_shift}
				min={1}
				max={100}
				unit="°С"
			/>
		</div>

		<div class="settings-item read-only">
			<label class="input-label" for="tp1_control_temp"> Целевая температура в колонне </label>
			<NumberInput
				bind:value={settings.tp1_control_temp}
				min={1}
				max={100}
				step={0.1}
				unit="°С"
			/>
		</div>

		<div class="settings-item read-only">
			<input
				bind:checked={settings.tp_filter}
				class="settings-checkbox"
				id="tpFilter"
				type="checkbox"
			/>
			<label class="input-label" for="tpFilter"> Фильтр ТД2 </label>
		</div>
	</div>
</div>

<style lang="scss">
	.settings-item-wrapper {
		border-bottom: 0;
	}

	.formula-block {
		display: flex;
		flex-direction: column;
		gap: 0.25rem;
	}

	.formula-hint {
		margin: 0;
		font-size: 0.8125rem;
		line-height: 1.35;
		opacity: 0.8;
	}

	.formula-warning {
		margin: 0.25rem 0;
		padding: 0.5rem 0.75rem;
		border-left: 3px solid var(--yellow-500);
		border-radius: 0.5rem;
		background: var(--yellow-100);
		color: var(--yellow-800);

		p {
			margin: 0;
			font-size: 0.8125rem;
			line-height: 1.35;
		}

		.formula-warning-action {
			margin-top: 0.35rem;
			padding: 0.25rem 0.6rem;
			border: 1px solid var(--yellow-600);
			border-radius: 0.375rem;
			background: transparent;
			color: inherit;
			font-size: 0.8rem;
			cursor: pointer;
		}

		.formula-warning-action:hover {
			background: color-mix(in srgb, var(--yellow-500) 20%, transparent);
		}

		@media (prefers-color-scheme: dark) {
			background: rgba(245, 158, 11, 0.16);
			color: var(--text-color);
		}
	}

	.formula-input-wrapper {
		flex-shrink: 0;
		text-align: left;
		opacity: 0;
		pointer-events: none;
		transition: opacity 0.1s ease-in-out;
	}
	.formula-input-wrapper.active {
		opacity: 1;
		pointer-events: auto;
	}
</style>
