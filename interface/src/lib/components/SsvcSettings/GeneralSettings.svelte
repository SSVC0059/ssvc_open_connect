<script lang="ts">
	import EditNumbersModal from '$lib/components/SsvcSettings/EditNumbersModal.svelte';
	import EditTimeModal from '$lib/components/SsvcSettings/EditTimeModal.svelte';
	import ProcessProtectionPanel from '$lib/components/SsvcSettings/ProcessProtectionPanel.svelte';
	import Toggle from '$lib/components/Toggle.svelte';
	import {
		PREDEC_VALUES,
		formulaModeHint,
		formulaModeLabel,
		formulaStartTempForMode,
		predecLabel
	} from '$lib/actions/rectificationRules';

	import type { OcApiFeature, SsvcSettings } from '$lib/types/ssvc';

	const {
		settings,
		onSave,
		apiFeatures = [],
		isSupportRelease,
		tp1Target = null
	} = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: unknown) => void;
		apiFeatures?: OcApiFeature[];
		/**
		 * Признак ветки прошивки: «Сброс и снижение» доступно только у прошивки
		 * с подголовниками. undefined — старая прошивка openConnect, поле не отдаёт.
		 */
		isSupportRelease?: boolean;
		/** Текущая целевая ТД1 из телеметрии: панель защиты показывает реальные пороги. */
		tp1Target?: number | null;
	}>();

	// Секция «Сброс и снижение»: явный признак ветки прошивки, а не значение
	// release_timer (ноль — легитимное значение таймера и не должен скрывать
	// секцию). Фолбэк на старое поведение — для прошивок без нового поля.
	const releaseSectionVisible = $derived(
		isSupportRelease === undefined ? Boolean(settings.release_timer) : true
	);
	const releaseSectionEnabled = $derived(isSupportRelease !== false);

	//
	// Отсутствие метаданных трактуется как «возможности нет»: включать режим 1.9
	// по умолчанию нельзя, иначе на контроллере 1.7/1.8 поле молча отбросит
	// очередь, а интерфейс покажет успех.
	const formulaAuto92Available = $derived(
		apiFeatures.find((feature: OcApiFeature) => feature.name === 'formula_auto92')?.available ?? false
	);
	const formulaStartTempAvailable = $derived(
		apiFeatures.find((feature: OcApiFeature) => feature.name === 'formula')?.available ?? true
	);
	const predecAvailable = $derived(
		apiFeatures.find((feature: OcApiFeature) => feature.name === 'predec')?.available ?? false
	);

</script>

{#if settings}
	<ProcessProtectionPanel {settings} {onSave} {tp1Target} />

	<div class="settings-grid">
		<div class="settings-panel">
			<div class="settings-section">
				<h3 class="settings-section-title">Параметры отбора</h3>

				<div class="settings-item">
					<span class="input-label">Гистерезис при отборе тела</span>
					<EditNumbersModal
						onSave={(newValues) => {
								onSave('hyst', newValues[0].value);
							}}
						values={[
								{
									name: 'Гистерезис',
									value: settings.hyst,
									unit: '°С',
									step: 0.01,
									precision: 1,
									min: 0,
									max: 50
								}
							]}
					/>
				</div>

				<div class="settings-item">
					<span class="input-label">Предекремент</span>
					<span class="settings-value">
						<select
							class="settings-select"
							aria-label="Предекремент"
							disabled={!predecAvailable}
							title={predecAvailable ? '' : 'Требуется API 1.9'}
							value={settings.predec}
							onchange={(e) => onSave('predec', Number(e.currentTarget.value))}
						>
							{#each PREDEC_VALUES as value}
								<option {value}>{predecLabel(value)}</option>
							{/each}
						</select>
					</span>
				</div>

				<div class="settings-item">
					<span class="input-label">Декремент при отборе тела</span>
					<EditNumbersModal
						onSave={(newValues) => {
								onSave('decrement', newValues[0].value);
							}}
						values={[
								{
									name: 'Декремент при отборе тела',
									value: settings.decrement,
									unit: '%',
									min: 0,
									max: 100
								}
							]}
					/>
				</div>

				<div class="settings-item">
					<span class="input-label">Время отбора голов</span>
					<EditTimeModal
						max={36000}
						onSave={(v) => onSave('heads_timer', v)}
						seconds={settings.heads_timer}
						step={300}
					/>
				</div>

				<div class="settings-item">
					<span class="input-label">Таймер фиксации температуры отбора тела</span>
					<EditNumbersModal
						onSave={(newValues) => {
								onSave('hearts_timer', newValues[0].value);
							}}
						values={[
								{
									name: 'Таймер фиксации температуры отбора тела',
									value: settings.hearts_timer,
									unit: 'сек',
									min: 0,
									max: 30
								}
							]}
					/>
				</div>
			</div>

			<div class="settings-section">
				<h3 class="settings-section-title">Настройки реле и Signal</h3>
				<div class="settings-item">
					<span class="input-label">Реле инвертировано</span>
					<Toggle checked={settings.relay_inverted} disabled />
				</div>

				<div class="settings-item">
					<span class="input-label">Автовключение реле</span>
					<Toggle checked={settings.relay_autostart} disabled />
				</div>

				<div class="settings-item">
					<span class="input-label">Контроль ТД1 для выхода SIGNAL</span>
					<Toggle checked={settings.signal_tp1_control} disabled />
				</div>

				<div class="settings-item">
					<span class="input-label">Инвертировать SIGNAL</span>
					<Toggle checked={settings.signal_inverted} disabled />
				</div>

				<div class="settings-item">
					<span class="input-label">Целевая температура на ТД1 при контроле ТД1 для выхода SIGNAL</span>
					<span class="settings-value">{settings.tp1_control_temp} °С</span>
				</div>

				<div class="settings-item">
					<span class="input-label">Старт по ТД1 при контроле ТД1 для выхода SIGNAL.</span>
					<Toggle checked={settings.tp1_control_start} disabled />
				</div>

			</div>

			<div class="settings-section">
				<h3 class="settings-section-title">Системные настройки</h3>

				<div class="settings-item">
					<span class="input-label">Звук</span>
					<Toggle checked={settings.sound} disabled />
				</div>

				<div class="settings-item">
					<span class="input-label">Барометр</span>
					<Toggle checked={settings.pressure} disabled />
				</div>

				<div class="settings-item">
					<span class="input-label">Отложенный пуск</span>
					<button class="settings-action">
						<span>00:00</span>
						<i data-feather="edit-2" class="text-xs"></i>
					</button>
				</div>

				<div class="settings-item">
					<span class="input-label">Поправка давления в кубе</span>
					<EditNumbersModal
						onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('tank_mmhg', newValues[0].value);
							}}
						values={[
								{
									name: 'Поправка давления в кубе',
									value: settings.tank_mmhg,
									unit: 'мм рт. ст.',
									min: 0,
									max: 50
								}
							]}
					/>
				</div>

				<div class="settings-item">
					<span class="input-label">Поправка ТД2</span>
					<span class="settings-value">{settings.tp2_shift} °С</span>
				</div>

				<div class="settings-item">
					<span class="input-label">Фильтр ТД</span>
					<span class="settings-value">0</span>
				</div>
			</div>
		</div>

		<div class="settings-panel">
			<div class="settings-section">
				<h3 class="settings-section-title">Температурные настройки</h3>

				<div class="settings-item">
					<span class="input-label">Температура ТД2 завершения отбора хвостов</span>
					<EditNumbersModal
						values={[
									{
										name: 'Температура ТД2 завершения отбора хвостов',
										value: settings.tails_temp ?? 0,
										unit: '°С',
										min: 0,
										max: 110.0,
										step: 0.1
									}
								]}
						onSave={(newValues) => {
									onSave('tails_temp', newValues[0].value);
								}}
					/>
				</div>

				<div class="settings-item">
					<span class="input-label">Температура ТД2 завершения отбора тела</span>
					<EditNumbersModal
						values={[
									{
										name: 'Температура ТД2 завершения отбора тела',
										value: settings.hearts_finish_temp,
										unit: '°С',
										min: 0,
										max: 110.0,
										step: 0.1
									}
								]}
						onSave={(newValues) => {
									onSave('hearts_finish_temp', newValues[0].value);
								}}
					/>
				</div>

				<div class="settings-item">
					<span class="input-label">Сдвиг температуры отбора тела на 0.07°С</span>
					<Toggle checked={settings.hearts_temp_shift} disabled />
				</div>

				<div class="settings-item">
					<span class="input-label">Пауза "на себя"</span>
					<Toggle checked={settings.hearts_pause} disabled />
				</div>

				<div class="settings-item settings-item--with-hint">
					<div class="settings-row">
						<span class="input-label">Формула</span>
						<span class="settings-value">
							<select
								class="settings-select"
								aria-label="Формула"
								value={settings.formula}
								onchange={(e) => onSave('formula', Number(e.currentTarget.value))}
							>
								{#each [0, 1, 2] as mode}
									<option
										value={mode}
										disabled={mode === 2 && !formulaAuto92Available}
										title={mode === 2 && !formulaAuto92Available ? 'Требуется API 1.9' : ''}
									>
										{formulaModeLabel(mode)}
									</option>
								{/each}
							</select>
						</span>
					</div>
					<span class="settings-hint">{formulaModeHint(settings.formula)}</span>
				</div>

				{#if !formulaStartTempAvailable}
					<div class="settings-item settings-item--with-hint">
						<div class="settings-row">
							<span class="input-label">Температура начала формулы</span>
							<span class="settings-value settings-value--disabled">
								{settings.formula_start_temp} °С
							</span>
						</div>
						<span class="settings-hint">требуется API 1.9</span>
					</div>
				{:else if settings.formula === 2}
					<div class="settings-item settings-item--with-hint">
						<div class="settings-row">
							<span class="input-label">Температура начала формулы</span>
							<span class="settings-value settings-value--disabled">
								{formulaStartTempForMode(2, settings.formula_start_temp)} °С
							</span>
						</div>
						<span class="settings-hint">В «Авто 92+» начало формулы — 92 °C</span>
					</div>
				{:else}
					<div class="settings-item">
						<span class="input-label">Температура начала формулы</span>
						<EditNumbersModal
							onSave={(newValues) => {
									onSave('formula_start_temp', newValues[0].value);
								}}
							values={[
									{
										name: 'Температура начала формулы',
										value: settings.formula_start_temp,
										unit: '°С',
										min: 84.0,
										max: 100.0,
										step: 0.1
									}
								]}
						/>
					</div>
				{/if}

				<div class="settings-item">
					<span class="input-label">Лимит стабилизации</span>
					<Toggle checked={settings.tp1_control_start} disabled />
				</div>

				<div class="settings-item">
					<span class="input-label">Завершить этап при превышении лимита стабилизации</span>
					<Toggle checked={settings.stab_limit_finish} disabled />
				</div>
			</div>

			{#if releaseSectionVisible}
				<div class="settings-section">
					<h3 class="settings-section-title">Сброс и снижение</h3>

					{#if !releaseSectionEnabled}
						<p class="settings-hint">
							Доступно только для прошивки с подголовниками (late_heads).
						</p>
					{/if}

					<div class="settings-item">
						<span class="input-label">Время сброса</span>
						{#if releaseSectionEnabled}
							<EditNumbersModal
								onSave={(newValues) => {
									// Используйте полный тип объекта как в компоненте
									onSave('release_timer', newValues[0].value);
								}}
								values={[
									{
										name: 'Время сброса.',
										value: settings.release_timer,
										unit: 'сек',
										min: 0,
										max: 1200
									}
								]}
							/>
						{:else}
							<span class="settings-value settings-value--disabled">
								{settings.release_timer} сек
								<span class="settings-hint">требуется прошивка с подголовниками</span>
							</span>
						{/if}
					</div>
					<div class="settings-item">
						<span class="input-label">Скорость сброса</span>
						{#if releaseSectionEnabled}
							<EditNumbersModal
								onSave={(newValues) => {
									// Используйте полный тип объекта как в компоненте
									onSave('release_speed', newValues[0].value);
								}}
								values={[
									{
										name: 'Скорость сброса (время открытого клапана). Значение должно быть меньше периода этапа отбора голов.',
										value: settings.release_speed,
										unit: 'сек',
										step: 0.1
									}
								]}
							/>
						{:else}
							<span class="settings-value settings-value--disabled">
								{settings.release_speed} сек
							</span>
						{/if}
					</div>

					<div class="settings-item">
						<span class="input-label">Скорость отбора к окончанию голов</span>
						{#if releaseSectionEnabled}
							<EditNumbersModal
								onSave={(newValues) => {
									// Используйте полный тип объекта как в компоненте
									onSave('heads_final', newValues[0].value);
								}}
								values={[
									{
										name: 'Скорость отбора к окончанию голов',
										value: settings.heads_final,
										unit: 'сек',
										min: 0,
										max: 99.9,
										step: 0.1
									}
								]}
							/>
						{:else}
							<span class="settings-value settings-value--disabled">
								{settings.heads_final} сек
							</span>
						{/if}
					</div>
				</div>
			{/if}

		</div>
	</div>
{/if}

<style lang="scss">
	/* Строка с пояснением под ней: подпись и значение остаются в общей сетке,
	   пояснение идёт отдельной строкой на всю ширину, а не второй колонкой. */
	.settings-item--with-hint {
		flex-direction: column;
		align-items: stretch;
		gap: 0.3rem;
	}

	.settings-row {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.75rem;
		width: 100%;
	}

	.settings-row > .input-label {
		flex-grow: 1;
		min-width: 0;
	}

	.settings-hint {
		display: block;
		font-size: 0.8125rem;
		line-height: 1.35;
		opacity: 0.8;
	}
</style>

