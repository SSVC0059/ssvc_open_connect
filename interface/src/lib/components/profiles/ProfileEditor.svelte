<script lang="ts">
	import type { FractionCalcResult, Profile, SsvcSettings } from '$lib/types/ssvc';
	import Info from '~icons/tabler/info-circle';
	import { getInfo } from '$lib/api/ssvcApi';
	import { getProfileContent } from '$lib/api/Profiles';
	import {
		calculateHeadsRampStage,
		createDefaultUIConfig,
		getTotalAAMl,
		suggestHeadsSettings
	} from '$lib/actions/profiles';

	let {
		profile,
		onSave,
		onCancel
	} = $props<{
		profile: Profile;
		onSave: (profile: any) => void;
		onCancel: () => void;
	}>();

	let profileData: Profile | null = $state(null);
	let isTailSupported: boolean = $state(false);
	let releaseEnabled: boolean = $state(false);

	let totalAAml: number = $state(0);
	let headsCalcResult: FractionCalcResult | null = $state(null);

	// --- Состояние для модального окна ---
	let showSuggestionModal = $state(false);
	let suggestionParams = $state({
		initialRate: 150,
		finalRate: 50,
		releaseVolume: 50
	});

	function isObject(item: any): item is Record<string, any> {
		return (item && typeof item === 'object' && !Array.isArray(item));
	}

	function deepMerge(target: any, source: any): any {
		const output = { ...target };
		if (isObject(target) && isObject(source)) {
			Object.keys(source).forEach(key => {
				if (isObject(source[key]) && key in target && isObject(target[key])) {
					output[key] = deepMerge(target[key], source[key]);
				} else {
					output[key] = source[key];
				}
			});
		}
		return output;
	}

	$effect(() => {
		(async () => {
			try {
				const [info, data] = await Promise.all([getInfo(), getProfileContent(profile.id)]);
				if (data) {
					const defaultConfig = createDefaultUIConfig();
					profileData = deepMerge(defaultConfig, data);

					// Инициализируем состояние чекбокса на основе загруженных данных
					if (profileData.ssvcSettings.release_timer > 0 || profileData.ssvcSettings.release_speed > 0 || profileData.ssvcSettings.heads_final > 0) {
						releaseEnabled = true;
					}
				}
				if (info) {
					isTailSupported = info.ssvc.mode != 'late_heads';
				}
				calculate();
			} catch (error) {
				console.error('Failed to load profile data:', error);
			}
		})();
	});

	function calculate() {
		if (profileData) {
			totalAAml = getTotalAAMl(profileData.rawVolumeLiters, profileData.rawStrength, profileData.rawTemperature);
			headsCalcResult = calculateHeadsRampStage(totalAAml, profileData, releaseEnabled);
		}
	}

	function handleSuggest() {
		if (!profileData) return;
		const suggested = suggestHeadsSettings(
			suggestionParams.initialRate,
			suggestionParams.finalRate,
			suggestionParams.releaseVolume,
			profileData
		);
		profileData.ssvcSettings = deepMerge(profileData.ssvcSettings, suggested);
		calculate(); // Пересчитываем итоговые значения
		showSuggestionModal = false;
	}

	function useNodeVolume() {
		if (profileData) {
			suggestionParams.releaseVolume = profileData.collectionNodeVolumeMl;
		}
	}

	function handleSave() {
		if (profileData) {
			onSave(profileData);
		}
	}
</script>

{#if profileData}
	<div class="profile-editor-grid">
		<!-- Панель 1: Параметры сырца -->
		<div class="card">
			<div class="settings-section">
				<h3 class="card-title">Параметры сырца</h3>
				<div class="input-group">
					<span class="input-label">Объем, л</span>
					<input class="input-field" type="number" bind:value={profileData.rawVolumeLiters} step="0.1" oninput={calculate}/>
				</div>
				<div class="input-group">
					<span class="input-label">Крепость, %</span>
					<input class="input-field" type="number" bind:value={profileData.rawStrength} oninput={calculate}/>
				</div>
				<div class="input-group">
					<span class="input-label">Температура, °C</span>
					<input class="input-field" type="number" bind:value={profileData.rawTemperature} oninput={calculate}/>
				</div>
				<div class="input-group">
					<span class="input-label">Целевая крепость, %</span>
					<input class="input-field" type="number" bind:value={profileData.targetStrength} />
				</div>
			</div>
		</div>

		<!-- Панель 2: Параметры отбора -->
		<div class="card">
			<div class="settings-section">
				<h3 class="card-title">Параметры отбора</h3>
				<div class="input-group">
					<span class="input-label">Головы (общие), % от АС</span>
					<input class="input-field" type="number" bind:value={profileData.headsTotalPercent} oninput={calculate} />
				</div>
				<div class="input-group">
					<span class="input-label">Объем узла отбора, мл</span>
					<input class="input-field" type="number" bind:value={profileData.collectionNodeVolumeMl} oninput={calculate} />
				</div>
				{#if !isTailSupported}
					<div class="input-group">
						<span class="input-label">Подголовники, % от АС</span>
						<input class="input-field" type="number" bind:value={profileData.lateHeadsPercent} oninput={calculate} />
					</div>
				{/if}
				<div class="input-group">
					<span class="input-label">Тело, % от АС</span>
					<input class="input-field" type="number" bind:value={profileData.heartsPercent} oninput={calculate} />
				</div>
				{#if isTailSupported}
					<div class="input-group">
						<span class="input-label">Хвосты, % от АС</span>
						<input class="input-field" type="number" bind:value={profileData.tailsPercent} oninput={calculate} />
					</div>
				{/if}
			</div>
		</div>

		<!-- Панель 3: Отбор голов -->
		<div class="card">
			<div class="settings-section">
				<h3 class="card-title">Отбор голов</h3>
				<div class="calc-result">
					<span>Расчетный объем: <strong>{headsCalcResult?.ml ?? 0} мл</strong></span>
					<span>Расчетное время: <strong>{headsCalcResult?.durationMin ?? 0} мин</strong></span>
				</div>
				<div class="input-group-toggle">
					<label for="release-toggle">Режим "Сброс и снижение"</label>
					<input type="checkbox" id="release-toggle" bind:checked={releaseEnabled} onchange={calculate} />
				</div>

				{#if releaseEnabled}
					<div class="input-group">
						<span class="input-label">Время сброса, с</span>
						<input class="input-field" type="number" bind:value={profileData.ssvcSettings.release_timer} oninput={calculate} />
					</div>
					<div class="input-group">
						<span class="input-label">Скорость сброса, с</span>
						<input class="input-field" type="number" step="0.1" bind:value={profileData.ssvcSettings.release_speed} oninput={calculate} />
					</div>
					<div class="input-group">
						<span class="input-label">Начальная скорость, [open, period]</span>
						<input class="input-field" type="number" step="0.1" bind:value={profileData.ssvcSettings.heads[0]} oninput={calculate} />
						<input class="input-field" type="number" bind:value={profileData.ssvcSettings.heads[1]} oninput={calculate} />
					</div>
					<div class="input-group">
						<span class="input-label">Конечная скорость, с</span>
						<input class="input-field" type="number" step="0.1" bind:value={profileData.ssvcSettings.heads_final} oninput={calculate} />
					</div>
				{:else}
					<div class="input-group">
						<span class="input-label">Скорость отбора, [open, period]</span>
						<input class="input-field" type="number" step="0.1" bind:value={profileData.ssvcSettings.heads[0]} oninput={calculate} />
						<input class="input-field" type="number" bind:value={profileData.ssvcSettings.heads[1]} oninput={calculate} />
					</div>
				{/if}

				<div class="editor-actions">
					<button class="btn btn-primary" onclick={() => showSuggestionModal = true}>Предложить настройки</button>
				</div>
			</div>
		</div>
	</div>

	<div class="settings-section">
		<div class="alert alert-info">
			<Info class="icon" />
			{#if !isTailSupported}
				<span>Режим отбора <strong>подголовников</strong> (без отбора хвостов).</span>
			{:else}
				<span>Режим отбора <strong>хвостов</strong>.</span>
			{/if}
		</div>
	</div>

	<div class="editor-actions">
		<button class="btn btn-success" onclick={handleSave}>Сохранить</button>
		<button class="btn btn-back" onclick={onCancel}>Отмена</button>
	</div>
{/if}

<!-- Модальное окно для предложения настроек -->
{#if showSuggestionModal}
	<div class="modal-backdrop" onclick={() => showSuggestionModal = false}>
		<div class="modal">
			<h3 class="modal-title">Расчет параметров отбора</h3>
			<div class="input-group">
				<span class="input-label">Начальная скорость, мл/ч</span>
				<input class="input-field" type="number" bind:value={suggestionParams.initialRate} />
			</div>
			<div class="input-group">
				<span class="input-label">Конечная скорость, мл/ч</span>
				<input class="input-field" type="number" bind:value={suggestionParams.finalRate} />
			</div>
			<div class="input-group">
				<span class="input-label">Объем сброса, мл</span>
				<input class="input-field" type="number" bind:value={suggestionParams.releaseVolume} />
				<button class="btn btn-sm" onclick={useNodeVolume}>Узел отбора</button>
			</div>
			<div class="modal-actions">
				<button class="btn btn-success" onclick={handleSuggest}>Применить</button>
				<button class="btn btn-back" onclick={() => showSuggestionModal = false}>Отмена</button>
			</div>
		</div>
	</div>
{/if}

<style lang="scss">
	.profile-editor-grid {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
		gap: 1.5rem;
		margin-bottom: 1.5rem;
	}

	.editor-actions {
		margin-top: 1.5rem;
		display: flex;
		gap: 0.5rem;
	}

	.card {
		background-color: var(--surface-color);
		border-radius: var(--border-radius-lg);
		padding: 1.5rem;
		box-shadow: var(--shadow-sm);
		border: 1px solid var(--border-color);
		height: fit-content;
	}

	.card-title {
		font-size: 1.125rem;
		font-weight: 600;
		color: var(--primary-800);
		margin-bottom: 1rem;
	}

	.alert {
		display: flex;
		align-items: center;
		padding: 0.75rem 1rem;
		border-radius: var(--border-radius);
		font-size: 0.9rem;

		.icon {
			flex-shrink: 0;
			margin-right: 0.75rem;
			font-size: 1.25rem;
		}
	}

	.alert-info {
		background-color: var(--primary-100);
		color: var(--primary-700);
		border: 1px solid var(--primary-200);
	}

	.calc-result {
		display: flex;
		justify-content: space-between;
		background-color: var(--surface-alt-color);
		padding: 0.75rem 1rem;
		border-radius: var(--border-radius);
		margin-bottom: 1rem;
		font-size: 0.9rem;
	}

	.input-group-toggle {
		display: flex;
		justify-content: space-between;
		align-items: center;
		margin-bottom: 1rem;
		padding: 0.5rem 0;
	}

	/* Стили для модального окна */
	.modal-backdrop {
		position: fixed;
		top: 0;
		left: 0;
		width: 100%;
		height: 100%;
		background-color: rgba(0, 0, 0, 0.6);
		display: flex;
		justify-content: center;
		align-items: center;
		z-index: 100;
	}

	.modal {
		background-color: var(--surface-color);
		padding: 2rem;
		border-radius: var(--border-radius-lg);
		box-shadow: var(--shadow-lg);
		width: 100%;
		max-width: 450px;
	}

	.modal-title {
		font-size: 1.25rem;
		font-weight: 600;
		margin-bottom: 1.5rem;
	}

	.modal-actions {
		margin-top: 1.5rem;
		display: flex;
		gap: 0.5rem;
		justify-content: flex-end;
	}

	.btn-sm {
		padding: 0.25rem 0.75rem;
		font-size: 0.8rem;
		margin-left: 0.5rem;
		flex-shrink: 0;
	}
</style>
