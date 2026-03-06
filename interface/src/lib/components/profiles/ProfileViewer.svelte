<script lang="ts">
	import type { Profile } from '$lib/types/ssvc';
	import AnalyticsValue from './AnalyticsValue.svelte';
	import { getProfileContent } from '$lib/api/Profiles';

	let { profileInfo } = $props<{ profileInfo: Profile }>();
	let isLoading = $state(true);
	let error: Error | null = $state(null);
	let profile: Profile | null = $state(null);

	function formatTime(seconds: number): string {
		if (isNaN(seconds) || seconds < 0) {
			return '00:00:00';
		}
		const h = Math.floor(seconds / 3600)
			.toString()
			.padStart(2, '0');
		const m = Math.floor((seconds % 3600) / 60)
			.toString()
			.padStart(2, '0');
		const s = Math.floor(seconds % 60)
			.toString()
			.padStart(2, '0');
		return `${h}:${m}:${s}`;
	}

	async function loadProfile(id: string) {
		isLoading = true;
		error = null;
		try {
			const data = await getProfileContent(id);
			// Проверяем наличие ssvcSettings, так как именно там лежат параметры
			profile = data;
			isLoading = true;
		} catch (err) {
			error = err as Error;
			console.error('Ошибка загрузки данных:', err);
		} finally {
			isLoading = false;
		}
	}

	$effect(() => {
		if (profileInfo.id) {
			loadProfile(profileInfo.id);
		} else {
			isLoading = false;
		}
	});

</script>


{#if isLoading}
	<div class="message-box">Загрузка данных профиля...</div>
{:else if error}
	<div class="message-box error-box">
		Ошибка при загрузке профиля: {error.message}
	</div>
{:else if profile}
	<div class="viewer-layout">
		<!-- ======================================================================= -->
		<!-- ОБЩИЕ ПАРАМЕТРЫ                                                         -->
		<!-- ======================================================================= -->
		{#if profile.powerKw != null || profile.volumeL != null || profile.strengthVol != null || profile.stabilizationMin != null}
			<div class="card">
				<h3 class="card-title">Общие параметры</h3>
				<div class="params-grid">
					{#if profile.powerKw != null}
						<AnalyticsValue label="Мощность" value={`${profile.powerKw} кВт`} />
					{/if}
					{#if profile.volumeL != null}
						<AnalyticsValue label="Объем заливки" value={`${profile.volumeL} л`} />
					{/if}
					{#if profile.strengthVol != null}
						<AnalyticsValue label="Крепость" value={`${profile.strengthVol} %`} />
					{/if}
					{#if profile.stabilizationMin != null}
						<AnalyticsValue label="Стабилизация" value={`${profile.stabilizationMin} мин`} />
					{/if}
				</div>
			</div>
		{/if}

		{#if profile.analytics}
			<!-- ======================================================================= -->
			<!-- ИТОГОВАЯ АНАЛИТИКА                                                      -->
			<!-- ======================================================================= -->
			<div class="card summary-card">
				<h3 class="card-title">Итоговые параметры процесса</h3>
				<div class="summary-grid">
					<AnalyticsValue
						label="Общее время"
						value={formatTime(profile.analytics.timers.total_process)}
						isLarge={true}
					/>
					<AnalyticsValue
						label="Общий АС"
						value={`${profile.analytics.totalAS.toFixed(0)} мл`}
						isLarge={true}
					/>
					<AnalyticsValue
						label="T° кипения"
						value={`${profile.analytics.boilingTemp.toFixed(2)} °C`}
					/>
					<AnalyticsValue label="Время цикла" value={profile.analytics.oneCycleTime} />
					<AnalyticsValue
						label="Остаток в кубе"
						value={`${(profile.analytics.residueMl / 1000).toFixed(2)} л`}
					/>
				</div>
			</div>

			<!-- ======================================================================= -->
			<!-- РАЗБИВКА ПО ФРАКЦИЯМ                                                    -->
			<!-- ======================================================================= -->
			<div class="card">
				<h3 class="card-title">Разбивка по фракциям</h3>
				<div class="fractions-grid">
					<!-- Головы -->
					{#if profile.heads.enabled}
						<div class="fraction-card">
							<h4 class="fraction-title">Головы</h4>
							<AnalyticsValue
								label="Объем"
								value={`${profile.analytics.fractions.headsMl.toFixed(0)} мл`}
							/>
							<AnalyticsValue label="Скорость" value={`${profile.analytics.flows.heads} мл/ч`} />
							<AnalyticsValue
								label="ФЧ"
								value={`${profile.analytics.refluxRatio.heads.toFixed(1)}`}
							/>
							<AnalyticsValue
								label="Время этапа"
								value={formatTime(profile.analytics.timers.heads)}
							/>
							<AnalyticsValue
								label="Время открытия клапана, c"
								value={profile.ssvcSettings.heads[0]}
							/>
							<AnalyticsValue
								label="Период, c"
								value={profile.ssvcSettings.heads[1]}
							/>
						</div>
					{/if}

					<!-- Подголовники -->
					{#if profile.late_heads.enabled}
						<div class="fraction-card">
							<h4 class="fraction-title">Подголовники</h4>
							<AnalyticsValue
								label="Объем"
								value={`${profile.analytics.fractions.lateHeadsMl.toFixed(0)} мл`}
							/>
							<AnalyticsValue
								label="Скорость"
								value={`${profile.analytics.flows.late_heads} мл/ч`}
							/>
							<AnalyticsValue
								label="ФЧ"
								value={`${profile.analytics.refluxRatio.late_heads.toFixed(1)}`}
							/>
							<AnalyticsValue
								label="Время этапа"
								value={formatTime(profile.analytics.timers.late_heads)}
							/>
							<AnalyticsValue
								label="Время открытия клапана, c"
								value={profile.ssvcSettings.late_heads[0]}
							/>
							<AnalyticsValue
								label="Период, c"
								value={profile.ssvcSettings.late_heads[1]}
							/>
						</div>
					{/if}

					<!-- Тело -->
					{#if profile.hearts.enabled}
						<div class="fraction-card">
							<h4 class="fraction-title">Тело</h4>
							<AnalyticsValue
								label="Объем"
								value={`${profile.analytics.fractions.heartsMl.toFixed(0)} мл`}
							/>
							<AnalyticsValue label="Скорость" value={`${profile.analytics.flows.hearts} мл/ч`} />
							<AnalyticsValue
								label="ФЧ"
								value={`${profile.analytics.refluxRatio.hearts.toFixed(1)}`}
							/>
							<AnalyticsValue
								label="Время этапа"
								value={formatTime(profile.analytics.timers.hearts)}
							/>
							<AnalyticsValue
								label="Время открытия клапана, c"
								value={profile.ssvcSettings.hearts[0]}
							/>
							<AnalyticsValue
								label="Период, c"
								value={profile.ssvcSettings.hearts[1]}
							/>
						</div>
					{/if}

					<!-- Хвосты -->
					{#if profile.tails.enabled}
						<div class="fraction-card">
							<h4 class="fraction-title">Хвосты</h4>
							<AnalyticsValue
								label="Объем"
								value={`${profile.analytics.fractions.tailsMl.toFixed(0)} мл`}
							/>
							<AnalyticsValue label="Скорость" value={`${profile.analytics.flows.tails} мл/ч`} />
							<AnalyticsValue
								label="ФЧ"
								value={`${profile.analytics.refluxRatio.tails.toFixed(1)}`}
							/>
							<AnalyticsValue
								label="Время этапа"
								value={formatTime(profile.analytics.timers.tails)}
							/>
							<AnalyticsValue
								label="Время открытия клапана, c"
								value={profile.ssvcSettings.tails[0]}
							/>
							<AnalyticsValue
								label="Период, c"
								value={profile.ssvcSettings.tails[1]}
							/>
						</div>
					{/if}
				</div>
			</div>
		{:else}
			<div class="message-box">Расчетные данные для этого профиля отсутствуют.</div>
		{/if}
	</div>
{:else}
	<div class="message-box">Выберите профиль для просмотра.</div>
{/if}

<style lang="scss">
	.viewer-layout {
		display: flex;
		flex-direction: column;
		gap: 1rem;
	}

	.card {
		background-color: var(--white);
		border-radius: var(--border-radius);
		padding: 1.25rem;
		border: 1px solid var(--primary-300);
	}

	.card-title {
		font-size: 1.125rem;
		color: var(--primary-800);
		margin: 0 0 1rem 0;
		padding-bottom: 0.75rem;
		border-bottom: 1px solid var(--primary-200);
	}

	.summary-card {
		background-color: var(--primary-100);
		border-color: var(--blue-300);
	}

	.summary-grid,
	.params-grid {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
		gap: 1rem;
	}

	.fractions-grid {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
		gap: 1rem;
	}

	.fraction-card {
		background-color: var(--gray-50);
		border: 1px solid var(--primary-200);
		padding: 1rem;
		border-radius: var(--border-radius);
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
	}

	.fraction-title {
		font-size: 1rem;
		font-weight: 600;
		color: var(--primary-700);
		margin: 0;
		text-align: center;
	}

	.message-box {
		padding: 2rem;
		text-align: center;
		background-color: var(--gray-50);
		border-radius: var(--border-radius);
		color: var(--gray-600);
	}

	.error-box {
		background-color: var(--red-50);
		color: var(--red-700);
		border: 1px solid var(--red-200);
	}
</style>
