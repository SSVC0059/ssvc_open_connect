<script lang="ts">
	import { createDefaultProfile, DistillationCycleModel } from '$lib/actions/DistillationCycleModel';
	import ChevronDown from '~icons/tabler/chevron-down';
	import { slide } from 'svelte/transition';
	import AnalyticsValue from './AnalyticsValue.svelte';
	import { getProfileContent } from '$lib/api/Profiles';
	import type { Profile } from '$lib/types/ssvc';
	import { normalizeProfile } from '$lib/utils/deepMerge';
	import KpnPopover from '$lib/components/profiles/KpnPopover.svelte'; // Опционально для иконки

	let {
		profileInfo,
		onSave,
		onCancel
	} = $props<{
		profileInfo: Profile;
		onSave: (profile: any) => void;
		onCancel: () => void;
	}>();

	const model = new DistillationCycleModel();

	const KPN_HELP = {
		title: 'Коэффициент паровой нагрузки (КПН)',
		description: 'Отношение общего объема генерируемого пара к скорости его отбора. Отражает реальную разделительную способность колонны.',
		footer: 'Чем выше число, тем больше флегмы возвращается в колонну и тем чище продукт.',
		ranges: {
			heads: 'Головы: 50.0 – 80.0 (макс. очистка)',
			late_heads: 'Подголовники: ~50.0 – 60.0',
			hearts: 'Тело: 2.0 – 5.0 (оптимальный баланс)',
			tails: '< 1.0 (режим прямотока)'
		}
	};

	// sourceProfile содержит все редактируемые поля формы
	let sourceProfile = $state<Profile>(createDefaultProfile());
	// profile является производным состоянием, которое автоматически пересчитывается
	let profile = $derived(model.calculateProcess(sourceProfile));

	let isLoading = $state(true);

	// Вычисляем сумму процентов всех фракций
	const totalFractionPercent = $derived.by(() => {
		let total = 0;
		// Головы всегда включены
		total += Number(sourceProfile.heads.percent) || 0;
		if (sourceProfile.late_heads.enabled) {
			total += Number(sourceProfile.late_heads.percent) || 0;
		}
		if (sourceProfile.tails.enabled) {
			total += Number(sourceProfile.tails.percent) || 0;
		}
		return total;
	});

	const isFractionSumInvalid = $derived(totalFractionPercent > 100);


	async function loadProfile(id: string) {
		isLoading = true;
		try {
			const rawProfile = await getProfileContent(id);
			// Загружаем данные в sourceProfile, что автоматически вызовет пересчет 'profile'
			sourceProfile = normalizeProfile(rawProfile);
		} catch (err) {
			console.error('Ошибка загрузки данных:', err);
		} finally {
			isLoading = false;
		}
	}

	// Загрузка профиля при инициализации
	$effect(() => {
		if (profileInfo.id) {
			loadProfile(profileInfo.id);
		} else {
			isLoading = false;
		}
	});

	function formatTime(seconds: number): string {
		if (isNaN(seconds) || seconds < 0) {
			return '00:00:00';
		}
		const h = Math.floor(seconds / 3600).toString().padStart(2, '0');
		const m = Math.floor((seconds % 3600) / 60).toString().padStart(2, '0');
		const s = Math.floor(seconds % 60).toString().padStart(2, '0');
		return `${h}:${m}:${s}`;
	}

	// Функции для управления взаимоисключающими полями для Голов
	function handleHeadsFlowInput(e: any) {
		if (e.target.value) {
			sourceProfile.heads.targetCycles = 0;
		}
	}

	function handleHeadsCyclesInput(e: any) {
		if (e.target.value) {
			sourceProfile.heads.targetFlowMlh = 0;
		}
	}

	// Функции для управления взаимоисключающими полями для Тела
	function handleBodyInitialSpeedInput(e: any) {
		if (e.target.value) {
			sourceProfile.hearts.targetFlowMlh = 0;
		}
	}

	function handleSave() {
		if (isFractionSumInvalid) {
			console.error('Сумма фракций превышает 100%');
			return;
		}
		// Это место для вашей логики обработки.
		// Вы можете изменить объект 'profile' перед отправкой.
		const finalProfile = { ...profile, id: profileInfo.id };
		console.log('Профиль для сохранения:', finalProfile);
		onSave(finalProfile);
	}

	// В секцию <script>
	function displayValue(val: number): string {
		return val === -1 ? '' : val.toString();
	}

	function handleOptionalInput(e: Event, field: 'release_timer' | 'release_speed' | 'heads_final') {
		const target = e.target as HTMLInputElement;
		const val = target.value;

		if (val === '') {
			sourceProfile.ssvcSettings[field] = -1;
		} else {
			sourceProfile.ssvcSettings[field] = Number(val);
		}
	}

</script>

{#if isLoading}
	<div class="loader">Загрузка профиля...</div>
{:else}
	<div class="editor-layout">
		<!-- ======================================================================= -->
		<!-- ОБЩИЕ ПАРАМЕТРЫ                                                         -->
		<!-- ======================================================================= -->
		<div class="card">
			<div class="card-header">
				<h3 class="card-title">Параметры процесса</h3>
			</div>
			<div class="settings-section">
				<div class="input-group">
					<span class="input-label">Мощность, кВт</span>
					<input class="input-field" type="number" bind:value={sourceProfile.powerKw} step="0.1" />
				</div>
				<div class="input-group">
					<span class="input-label">Объем заливки, л</span>
					<input class="input-field" type="number" bind:value={sourceProfile.volumeL} step="0.5" />
				</div>
				<div class="input-group">
					<span class="input-label">Крепость, %</span>
					<input class="input-field" type="number" bind:value={sourceProfile.strengthVol} />
				</div>
				<div class="input-group">
					<span class="input-label">Стабилизация, мин</span>
					<input class="input-field" type="number" bind:value={sourceProfile.stabilizationMin} />
				</div>
			</div>
		</div>

		<!-- ======================================================================= -->
		<!-- НАСТРОЙКИ ФРАКЦИЙ                                                       -->
		<!-- ======================================================================= -->
		<div class="card">
			<div class="card-header">
				<h3 class="card-title">Настройки отбора по фракциям</h3>
				{#if isFractionSumInvalid}
					<div class="fraction-warning">
						Сумма фракций (АС): {totalFractionPercent.toFixed(1)}% - не может быть больше 100%
					</div>
				{/if}
			</div>


			<!-- Головы -->
			<details class="fraction-details" open>
				<summary>
					<div class="summary-content" >
						<span>Головы</span>
						<div class="summary-controls">
							<ChevronDown class="chevron" />
						</div>
					</div>
				</summary>

				<div class="fraction-body" transition:slide|local>
					<!-- Левая колонка: Настройки -->
					<div class="settings-col">
						<h4 class="col-title">Параметры отбора</h4>
						<div class="input-group">
							<span class="input-label">% от АС</span>
							<input class="input-field" type="number" bind:value={sourceProfile.heads.percent} />
						</div>
						<div class="input-group">
							<span class="input-label">Клапан, мл/ч</span>
							<input class="input-field" type="number" bind:value={sourceProfile.ssvcSettings.valve_bw[0]} />
						</div>
						<div class="sub-settings-grid">
							<div class="sub-settings-group">
								<h5 class="sub-settings-title">Режим расчета (укажите одно)</h5>
								<div class="input-group">
									<span class="input-label">Количество переиспарений</span>
									<input class="input-field" type="number" placeholder="Авто (2)" bind:value={sourceProfile.heads.targetCycles} oninput={handleHeadsCyclesInput} step="0.1" />
								</div>
								<div class="input-group">
									<span class="input-label">Желаемая скорость, мл/ч</span>
									<input class="input-field" type="number" placeholder="Авто" bind:value={sourceProfile.heads.targetFlowMlh} oninput={handleHeadsFlowInput} />
								</div>
							</div>
							<div class="sub-settings-group">
								<h5 class="sub-settings-title">Расчет клапанов</h5>
								<div class="input-group">
									<span class="input-label">Период SSVC, сек</span>
									<div class="input-with-initial">
										<input
											class="input-field"
											type="number"
											value={sourceProfile.ssvcSettings.heads[1]}
											oninput={(e) => {
												sourceProfile.ssvcSettings.heads[1] = Number(e.currentTarget.value);
											}}
										/>
										<span class="initial-value">
												({profile.ssvcSettings.heads[1]})
										</span>
									</div>
								</div>
							</div>
							<div class="sub-settings-group">
								<h5 class="sub-settings-title">Сброс и снижение</h5>

								<div class="input-group" class:is-disabled={sourceProfile.ssvcSettings.release_timer === -1}>
									<span class="input-label">Продолжительность сброса (сек)</span>
									<input
											class="input-field"
											type="number"
											placeholder="Выкл"
											value={displayValue(sourceProfile.ssvcSettings.release_timer)}
											oninput={(e) => handleOptionalInput(e, 'release_timer')}
									/>
								</div>

								<div class="input-group" class:is-disabled={sourceProfile.ssvcSettings.release_speed === -1}>
									<span class="input-label">Время сброса (сек)</span>
									<input
											class="input-field"
											type="number"
											step="0.1"
											placeholder="Выкл"
											value={displayValue(sourceProfile.ssvcSettings.release_speed)}
											oninput={(e) => handleOptionalInput(e, 'release_speed')}
									/>
								</div>

								<div class="input-group" class:is-disabled={sourceProfile.ssvcSettings.heads_final === -1}>
									<span class="input-label">Время открытия в конце этапа (сек)</span>
									<input
											class="input-field"
											type="number"
											step="0.1"
											placeholder="Выкл"
											value={displayValue(sourceProfile.ssvcSettings.heads_final)}
											oninput={(e) => handleOptionalInput(e, 'heads_final')}
									/>
								</div>
							</div>
						</div>
					</div>

					<!-- Правая колонка: Расчет -->
					<div class="results-col">
						<h4 class="col-title">Расчетные значения</h4>
						<AnalyticsValue label="Объем голов" value={`${profile.analytics.fractions.headsMl.toFixed(0)} мл`} />
						{#if profile.analytics.fractions.releaseMl > 0}
							<AnalyticsValue
									label="Объем сброса"
									value={`${profile.analytics.fractions.releaseMl} мл`}
							/>
						{/if}
						<AnalyticsValue label="Скорость отбора" value={`${profile.analytics.flows.heads} мл/ч`} />
						<KpnPopover id="kpn-heads" rangeKey="heads" {profile} />
						{#if sourceProfile.ssvcSettings.release_timer > 0 && profile.analytics.flows.heads_release > 0}
							<AnalyticsValue
									label="Скорость сброса"
									value={`${profile.analytics.flows.heads_release} мл/ч`}
							/>
						{/if}

						{#if sourceProfile.ssvcSettings.heads_final > 0 && profile.analytics.flows.heads_final > 0}
							<AnalyticsValue
									label="Финальная скорость"
									value={`${profile.analytics.flows.heads_final} мл/ч`}
							/>
						{/if}
						<AnalyticsValue label="Флегмовое число" value={profile.analytics.phlegmatic.heads} />
						<AnalyticsValue label="Время этапа" value={formatTime(profile.analytics.timers.heads)} />
						<AnalyticsValue label="Время переиспарения" value={formatTime(profile.analytics.oneCycleTime)} />
						<AnalyticsValue label="Время" value={profile.ssvcSettings.heads[0]} />
						<AnalyticsValue label="Период" value={profile.ssvcSettings.heads[1]} />
					</div>
				</div>
			</details>

			<details class="fraction-details">
				<summary>
					<div class="summary-content" class:active={sourceProfile.late_heads.enabled}>
						<span>Подголовники</span>
						<div class="summary-controls">
							<input
									type="checkbox"
									bind:checked={sourceProfile.late_heads.enabled}
									onchange={() => { if (sourceProfile.late_heads.enabled) sourceProfile.tails.enabled = false }}
							>
							<ChevronDown class="chevron" />
						</div>
					</div>
				</summary>
				{#if sourceProfile.late_heads.enabled}
					<div class="fraction-body" transition:slide|local>
						<!-- Левая колонка: Настройки -->
						<div class="settings-col">
							<h4 class="col-title">Параметры отбора</h4>
							<div class="input-group">
								<span class="input-label">% от АС</span>
								<input class="input-field" type="number" bind:value={sourceProfile.late_heads.percent} />
							</div>
							<div class="input-group">
								<span class="input-label">Клапан, мл/ч</span>
								<input class="input-field" type="number" bind:value={sourceProfile.ssvcSettings.valve_bw[2]} />
							</div>
							<div class="sub-settings-grid">
								<div class="sub-settings-group">
									<h5 class="sub-settings-title">Режим расчета</h5>
									<div class="input-group">
										<span class="input-label">Желаемая скорость, мл/ч</span>
										<input class="input-field" type="number" placeholder="Авто" bind:value={sourceProfile.late_heads.targetFlowMlh} />
									</div>
								</div>
								<div class="sub-settings-group">
									<h5 class="sub-settings-title">Настройки SSVC (прошивка)</h5>
									<div class="input-group">
										<span class="input-label">Период SSVC, сек</span>
										<div class="input-with-initial">
											<input
												class="input-field"
												type="number"
												value={sourceProfile.ssvcSettings.late_heads[1]}
												oninput={(e) => sourceProfile.ssvcSettings.late_heads[1] = Number(e.currentTarget.value)}
											/>
											<span class="initial-value">
													({profile.ssvcSettings.late_heads[1]})
											</span>
										</div>

									</div>
								</div>
							</div>
						</div>

						<!-- Правая колонка: Расчет -->
						<div class="results-col">
							<h4 class="col-title">Расчетные значения</h4>
							<AnalyticsValue label="Объем" value={`${profile.analytics.fractions.lateHeadsMl.toFixed(0)} мл`} />
							<AnalyticsValue label="Скорость" value={`${profile.analytics.flows.late_heads} мл/ч`} />
							<KpnPopover id="kpn-late-heads" rangeKey="late_heads" {profile} />
							<AnalyticsValue label="Флегмовое число" value={profile.analytics.phlegmatic.late_heads} />
							<AnalyticsValue label="Время этапа" value={formatTime(profile.analytics.timers.late_heads)} />
							<AnalyticsValue label="Время" value={profile.ssvcSettings.late_heads[0]} />
							<AnalyticsValue label="Период" value={profile.ssvcSettings.late_heads[1]} />
						</div>
					</div>
				{/if}
			</details>

			<details class="fraction-details" open>
				<summary>
					<div class="summary-content active">
						<span>Тело</span>
						<ChevronDown class="chevron" />
					</div>
				</summary>
				<div class="fraction-body" transition:slide|local>
					<!-- Левая колонка: Настройки -->
					<div class="settings-col">
						<h4 class="col-title">Параметры отбора</h4>

						<div class="sub-settings-grid" style="border-top: none; padding-top: 0;">
							<div class="sub-settings-group">
								<h5 class="sub-settings-title">Завершение отбора</h5>
								<div class="input-group">
									<span class="input-label">Стоп по Т куба, °C</span>
									<input class="input-field" type="number" step="0.1" placeholder="Напр. 90.0" bind:value={sourceProfile.ssvcSettings.hearts_finish_temp} />
								</div>
								<div class="input-group" class:input-disabled={sourceProfile.ssvcSettings.hearts_finish_temp > 0}>
									<span class="input-label">% от АС</span>
									<input class="input-field" type="number" bind:value={sourceProfile.hearts.percent} disabled={sourceProfile.ssvcSettings.hearts_finish_temp > 0} />
								</div>
							</div>

							<div class="sub-settings-group">
								<h5 class="sub-settings-title">Снижение (Формула)</h5>
								<div class="formula-controls">
									<label class="checkbox-label">
										<input type="checkbox" bind:checked={sourceProfile.ssvcSettings.formula} />
										<span>Вкл</span>
									</label>
									<div class="formula-inputs" class:disabled={!sourceProfile.ssvcSettings.formula}>
										<div class="input-group">
											<span class="input-label">Гист.</span>
											<input class="input-field" type="number" step="0.01" bind:value={sourceProfile.ssvcSettings.hyst} />
										</div>
										<div class="input-group">
											<span class="input-label">Декр. %</span>
											<input class="input-field" type="number" bind:value={sourceProfile.ssvcSettings.decrement} />
										</div>
									</div>
								</div>
							</div>
						</div>

						<div class="sub-settings-grid">
							<div class="sub-settings-group">
								<h5 class="sub-settings-title">Скорость и оборудование</h5>
								<div class="input-group">
									<span class="input-label">Клапан, мл/ч</span>
									<input class="input-field" type="number" bind:value={sourceProfile.ssvcSettings.valve_bw[1]} />
								</div>
								<div class="input-group">
									<span class="input-label">Начальная, мл/ч</span>
									<input class="input-field" type="number" bind:value={sourceProfile.hearts.targetFlowMlh} placeholder="Авто" />
								</div>
							</div>

							<div class="sub-settings-group">
								<h5 class="sub-settings-title">Настройки SSVC</h5>
								<div class="input-group">
									<span class="input-label">Период, сек</span>
									<div class="input-with-initial">
										<input class="input-field" type="number" value={sourceProfile.ssvcSettings.hearts[1]} oninput={(e) => sourceProfile.ssvcSettings.hearts[1] = Number(e.currentTarget.value)} />
										<span class="initial-value">({profile.ssvcSettings.hearts[1]})</span>
									</div>
								</div>
							</div>
						</div>
					</div>

					<!-- Правая колонка: Расчет -->
					<div class="results-col">
						<h4 class="col-title">Расчетные значения</h4>

						<AnalyticsValue label="Объем" value={`${profile.analytics.fractions.heartsMl.toFixed(0)} мл`} />

						{#if sourceProfile.ssvcSettings.formula && sourceProfile.ssvcSettings.decrement > 0}
							<AnalyticsValue
									label="Скорость (Н/К/Ср)"
									value={`${profile.analytics.flows.hearts} / ${profile.analytics.flows.hearts_final} / ${profile.analytics.flows.hearts_avg} мл/ч`}
							/>
						{:else}
							<AnalyticsValue label="Скорость" value={`${profile.analytics.flows.hearts} мл/ч`} />
						{/if}
						<KpnPopover id="kpn-hearts" rangeKey="hearts" {profile} />
						<AnalyticsValue label="Флегмовое число" value={profile.analytics.phlegmatic.hearts} />
						<AnalyticsValue label="Время этапа" value={formatTime(profile.analytics.timers.hearts)} />
						<AnalyticsValue label="Открытие" value={`${profile.ssvcSettings.hearts[0]} сек`} />
						<AnalyticsValue label="Период" value={`${profile.ssvcSettings.hearts[1]} сек`} />
					</div>
				</div>
			</details>

			{#if sourceProfile.tails.enabled}
				<details class="fraction-details">
					<summary>
						<div class="summary-content" class:active={sourceProfile.tails.enabled}>
							<span>Хвосты</span>
							<div class="summary-controls">
								<input
										type="checkbox"
										bind:checked={sourceProfile.tails.enabled}
										onchange={() => { if (sourceProfile.tails.enabled) sourceProfile.late_heads.enabled = false }}
								>
								<ChevronDown class="chevron" />
							</div>
						</div>
					</summary>

						<div class="fraction-body" transition:slide|local>
							<!-- Левая колонка: Настройки -->
							<div class="settings-col">
								<h4 class="col-title">Параметры отбора</h4>
								<div class="input-group">
									<span class="input-label">% от АС</span>
									<input class="input-field" type="number" bind:value={sourceProfile.tails.percent} />
								</div>
								<div class="input-group">
									<span class="input-label">Клапан, мл/ч</span>
									<input class="input-field" type="number" bind:value={sourceProfile.ssvcSettings.valve_bw[2]} />
								</div>
								<div class="sub-settings-grid">
									<div class="sub-settings-group">
										<h5 class="sub-settings-title">Режим расчета</h5>
										<div class="input-group">
											<span class="input-label">Желаемая скорость, мл/ч</span>
											<input class="input-field" type="number" placeholder="Авто" bind:value={sourceProfile.tails.targetFlowMlh} />
										</div>
									</div>
									<div class="sub-settings-group">
										<h5 class="sub-settings-title">Настройки SSVC (прошивка)</h5>
										<div class="input-group">
											<span class="input-label">Период SSVC, сек</span>
											<div class="input-with-initial">
												<input
													class="input-field"
													type="number"
													value={sourceProfile.ssvcSettings.tails[1]}
													oninput={(e) => sourceProfile.ssvcSettings.tails[1] = Number(e.currentTarget.value)}
												/>
												<span class="initial-value">
														({profile.ssvcSettings.tails[1]})
												</span>
											</div>

										</div>
									</div>
								</div>
							</div>

							<!-- Правая колонка: Расчет -->
							<div class="results-col">
								<h4 class="col-title">Расчетные значения</h4>
								<AnalyticsValue label="Объем" value={`${profile.analytics.fractions.tailsMl.toFixed(0)} мл`} />
								<AnalyticsValue label="Скорость" value={`${profile.analytics.flows.tails} мл/ч`} />
								<KpnPopover id="kpn-tails" rangeKey="tails" {profile} />
								AnalyticsValue label="Флегмовое число" value={profile.analytics.phlegmatic.tails} />
								<AnalyticsValue label="Время этапа" value={formatTime(profile.analytics.timers.tails)} />
								<AnalyticsValue label="Время" value={profile.ssvcSettings.tails[0]} />
								<AnalyticsValue label="Период" value={profile.ssvcSettings.tails[1]} />
							</div>
						</div>
				</details>
			{/if}


		</div>
		<div class="card final-analytics-card">
			<h3 class="card-title">Итоговая аналитика процесса</h3>
			<div class="final-analytics-grid">
				<AnalyticsValue label="Общий АС" value={`${profile.analytics.totalAS.toFixed(0)} мл`} />
				<AnalyticsValue label="T° кипения" value={`${profile.analytics.boilingTemp.toFixed(2)} °C`} />
				<AnalyticsValue label="Время цикла" value={formatTime(profile.analytics.oneCycleTime)} />
				<AnalyticsValue label="Остаток в кубе" value={`${(profile.analytics.residueMl / 1000).toFixed(2)} л`} />
				<AnalyticsValue label="Остаточная крепость" value={`${profile.analytics.residualFortress} %`} />
				<AnalyticsValue label="Общее время" value={formatTime(profile.analytics.timers.total_process)} isLarge={true} />
			</div>
		</div>
	</div>

	<div class="actions-panel">
		<button class="btn-secondary" onclick={onCancel}>Отмена</button>
		<button class="btn-primary" onclick={handleSave} disabled={isFractionSumInvalid}>Сохранить профиль</button>
	</div>
{/if}

<style lang="scss">
	.editor-layout {
		display: flex;
		flex-direction: column;
		gap: 1rem;
	}

	.card {
		background-color: var(--white);
		border-radius: var(--border-radius);
		padding: 1.0rem;
		border: 1px solid var(--primary-300);
	}

	.card-header {
		display: flex;
		justify-content: space-between;
		align-items: center;
		margin-bottom: 1rem;
		flex-wrap: wrap;
	}

	.fraction-warning {
		color: var(--red-600);
		font-size: 0.9rem;
		font-weight: 600;
		background-color: var(--red-100);
		padding: 0.5rem 1rem;
		border-radius: var(--border-radius);
		border: 1px solid var(--red-300);
		margin-top: 0.5rem;
		width: 100%;
		text-align: center;
	}

	.card-title {
		font-size: 1.125rem;
		color: var(--primary-800);
		margin: 0;
	}

	.settings-section {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
		gap: 0.25rem;
	}

	.fraction-details {
		border-bottom: 2px solid var(--primary-300);
		padding-bottom: 1rem;
		margin-bottom: 1rem;

		&:last-of-type {
			border-bottom: none;
			margin-bottom: 0;
			padding-bottom: 0;
		}

		summary {
			list-style: none;
			cursor: pointer;
			padding: 0;
			font-weight: 600;
			display: block;
			transition: background-color 0.2s;

			&:hover {
				background-color: var(--primary-100);
			}

			&::-webkit-details-marker {
				display: none;
			}
		}

		.summary-content {
			display: flex;
			justify-content: space-between;
			align-items: center;
			padding: 0.75rem 0.25rem;
			border-radius: var(--border-radius);

			&.active {
				color: var(--primary-800);
			}
		}

		.summary-controls {
			display: flex;
			align-items: center;
			gap: 0.75rem;
		}

		.chevron {
			transition: transform 0.2s ease-in-out;
			color: var(--primary-500);
			font-size: 1.25rem;
		}

		&[open] > summary .chevron {
			transform: rotate(180deg);
		}
	}

	.fraction-body {
		background-color: var(--gray-50);
		border-top: 1px solid var(--primary-300);
		padding: 1rem;
		display: grid;
		grid-template-columns: 1fr;
		gap: 1.5rem;

		@media (min-width: 768px) {
			grid-template-columns: 2fr 1fr;
		}
	}

	.settings-col {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
	}

	.results-col {
		background-color: var(--gray-50);
		border: 1px dashed var(--primary-300);
		padding: 1rem;
		border-radius: var(--border-radius);
		display: grid;
		grid-template-columns: 1fr;
		gap: 0.75rem;
		align-self: start; /* Не растягиваться на всю высоту, если левая колонка выше */

		@media (min-width: 1024px) {
			grid-template-columns: 1fr 1fr;
			gap: 0.5rem 1rem;

			& > .col-title {
				grid-column: 1 / -1;
			}
		}
	}

	.col-title {
		font-size: 1rem;
		font-weight: 600;
		color: var(--primary-700);
		margin: 0 0 0.25rem;
		border-bottom: 1px solid var(--primary-300);
		padding-bottom: 0.5rem;
	}

	.sub-settings-grid {
		display: grid;
		grid-template-columns: 1fr;
		gap: 1.5rem;
		margin-top: 0.5rem;
		border-top: 1px solid var(--primary-300);
		padding-top: 1rem;

		@media (min-width: 768px) {
			grid-template-columns: 1fr 1fr;
		}
	}

	.sub-settings-group {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
	}

	.sub-settings-title {
		font-size: 0.8rem;
		font-weight: 600;
		color: var(--primary-500);
		text-transform: uppercase;
		margin: 0;
	}

	/* Стили для полей ввода */
	.input-field {
		&:disabled {
			background-color: var(--gray-200); /* Явный серый фон */
			color: var(--gray-500);            /* Блеклый текст */
			border-color: var(--gray-300);     /* Нейтральная рамка */
			cursor: not-allowed;               /* Курсор запрета */
			opacity: 0.7;
		}
	}

	/* Стиль для всей группы, когда поле отключено */
	.input-disabled {
		.input-label {
			color: var(--gray-400); /* Текст подписи тоже бледным */
		}
	}

	.input-with-initial {
		display: flex;
		align-items: center;
		gap: 0.5rem;

		.input-field {
			flex-grow: 1;
		}

		.initial-value {
			font-size: 0.8rem;
			color: var(--primary-500);
			white-space: nowrap;
		}
	}

	/* Компактные контролы формулы */
	.formula-controls {
		display: flex;
		align-items: center;
		gap: 0.75rem;
		margin-top: 0.25rem;
		height: 100%;
		padding: 0.25rem 0;
	}

	.formula-inputs {
		display: flex;
		gap: 0.5rem;
		align-items: flex-end;
		transition: opacity 0.2s;

		&.disabled {
			opacity: 0.3;
			pointer-events: none;
			filter: grayscale(1); /* Визуальный эффект отключения */
		}
	}

	.checkbox-label {
		display: flex;
		align-items: center;
		gap: 0.3rem;
		font-size: 0.85rem;
		font-weight: 600;
		cursor: pointer;
		user-select: none;

		input[type="checkbox"] {
			width: 16px;
			height: 16px;
		}
	}

	.final-analytics-card {
		border-color: var(--blue-300);
		background-color: var(--primary-100);
	}

	.final-analytics-grid {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
		gap: 1rem;
	}

	.actions-panel {
		display: flex;
		justify-content: flex-end;
		gap: 1rem;
		padding: 1.5rem 1rem;
		background-color: rgba(255, 255, 255, 0.8);
		backdrop-filter: blur(4px);
		border-top: 1px solid var(--primary-300);
		position: sticky;
		bottom: 0;
		z-index: 10;
	}

	.btn-primary,
	.btn-secondary {
		padding: 0.6rem 1.5rem;
		border-radius: var(--border-radius);
		font-weight: 600;
		cursor: pointer;
		border: 1px solid transparent;
		font-size: 0.9rem;
	}

	.btn-primary {
		background-color: var(--primary-500);
		color: var(--white);
		border-color: var(--primary-500);

		&:hover {
			background-color: var(--primary-600);
		}

		&:disabled {
			background-color: var(--gray-400);
			border-color: var(--gray-400);
			cursor: not-allowed;
		}
	}

	.btn-secondary {
		background-color: var(--white);
		color: var(--primary-700);
		border-color: var(--primary-300);

		&:hover {
			background-color: var(--primary-100);
		}
	}
</style>