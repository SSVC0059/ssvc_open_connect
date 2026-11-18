<script lang="ts">
	import { page } from '$app/stores';
	import { browser } from '$app/environment';
	import { tick } from 'svelte';
	import type { OcApiFeature, SsvcSettings } from '$lib/types/ssvc';
	import {
		fetchSettings,
		fetchTelemetry,
		getInfo,
		updateSetting,
		infoCacheVersion
	} from '$lib/api/ssvcApi';
	import { notifications } from '$lib/components/toasts/notifications';
	import { getActiveProfile, updateProfileContent } from '$lib/api/Profiles';
	import GeneralSettings from '$lib/components/SsvcSettings/GeneralSettings.svelte';
	import ValveBandwidth from '$lib/components/SsvcSettings/ValveBandwidth.svelte';
	import SpeedSettings from '$lib/components/SsvcSettings/SpeedSettings.svelte';
	import ParallelValve from '$lib/components/SsvcSettings/ParallelValve.svelte';
	import Reload from '~icons/tabler/reload';
	import { goto } from '$app/navigation';
	import SettingsMobileScroll from '$lib/components/settings-layout/SettingsMobileScroll.svelte';

	const MOBILE_MQ = '(max-width: 767px)';

	let ssvcSettings = $state<SsvcSettings | null>();
	// Возможности API подключённого контроллера: по ним поля, появившиеся в
	// новых версиях API, показываются неактивными с пояснением.
	let apiFeatures = $state<OcApiFeature[]>([]);
	// Признак ветки прошивки: «Сброс и снижение» есть только у прошивки
	// с подголовниками. undefined — старая прошивка openConnect, поля нет.
	let isSupportRelease = $state<boolean | undefined>(undefined);
	// Текущая целевая ТД1 из телеметрии: по ней панель «Защита от стопа»
	// показывает реальные пороги. null — данных нет, панель покажет формулу.
	let tp1Target = $state<number | null>(null);

	let activeTab = $state(0);

	const loadSsvcSettings = async () => {
		try {
			ssvcSettings = await fetchSettings();
		} catch (err) {
			if (err instanceof Error) {
				console.error(err);
			}
		}
		// Панель «Защита от стопа» показывает пороги по текущей Т отбора:
		// перечитываем её вместе с настройками, в том числе по кнопке «Обновить».
		await loadTp1Target();
	};

	const loadApiFeatures = async () => {
		const info = await getInfo();
		apiFeatures = info?.oc.api_features ?? [];
		isSupportRelease = info?.oc.is_support_release;
	};

	// Одно чтение телеметрии: только Т отбора для панели защиты. Ошибку чтения
	// не показываем — панель в этом случае объясняет пороги формулой.
	const loadTp1Target = async () => {
		try {
			const response = await fetchTelemetry();
			tp1Target = response?.telemetry?.tp1_target ?? null;
		} catch {
			tp1Target = null;
		}
	};

	/**
	 * При загрузке настройки применяются из активного профиля
	 * (`ProfileService` → `SsvcSettings::onProfileApply`). Если изменить только
	 * текущие настройки, после перезагрузки профиль вернёт прежнее значение.
	 * Пишем в профиль лишь изменённое поле, не затирая остальные.
	 */
	async function persistActiveProfileField(field: string, value: unknown): Promise<boolean> {
		try {
			const active = (await getActiveProfile()) as unknown as { id?: string } | null;
			const id = active?.id;
			if (id == null) return false;
			return await updateProfileContent(String(id), { ssvcSettings: { [field]: value } });
		} catch (err) {
			console.error('Не удалось сохранить настройку в активный профиль', err);
			return false;
		}
	}

	async function saveChanges(field: string, value: unknown) {
		const result = await updateSetting(field, value);
		if (result) {
			notifications.success('Настройка сохранена', 5000);
			const persisted = await persistActiveProfileField(field, value);
			if (!persisted) {
				notifications.warning(
					'Настройка применена, но не сохранена в активный профиль: после перезагрузки вернётся прежнее значение',
					7000
				);
			}
			await loadSsvcSettings();
		} else {
			notifications.error('Ошибка сохранения настроек', 5000);
		}
	}

	interface Tab {
		id: string;
		title: string;
		component: any;
		props: Record<string, unknown>;
	}

	const availableTabs = $derived(computeTabs(ssvcSettings));

	/**
	 * Параллельный отбор отдаётся контроллером только для поддерживающей его
	 * ветки SSVC0059: `fillSettings` пропускает пустые кортежи. Если полей нет,
	 * вкладка была бы пустой — не показываем её вовсе.
	 */
	function hasParallelSettings(settings: SsvcSettings): boolean {
		const s = settings as unknown as {
			parallel?: unknown;
			parallel_v1?: unknown;
			parallel_v3?: unknown;
		};
		const isPair = (value: unknown) => Array.isArray(value) && value.length >= 2;
		return (
			isPair(s.parallel) ||
			isPair(s.parallel_v1) ||
			(Array.isArray(s.parallel_v3) && s.parallel_v3.length > 0)
		);
	}

	function computeTabs(settings: SsvcSettings | null | undefined): Tab[] {
		if (!settings) return [];

		const tabs: Tab[] = [
			{
				id: 'general',
				title: 'Общие',
				component: GeneralSettings,
				props: { settings, onSave: saveChanges, apiFeatures, isSupportRelease, tp1Target }
			},
			{
				id: 'valve-bandwidth',
				title: 'Клапаны',
				component: ValveBandwidth,
				props: { settings, onSave: saveChanges }
			},
			{
				id: 'speed',
				title: 'Параметры отбора',
				component: SpeedSettings,
				props: { settings, onSave: saveChanges }
			}
		];

		if (hasParallelSettings(settings)) {
			tabs.push({
				id: 'parallel',
				title: 'Параллельный отбор',
				component: ParallelValve,
				props: { settings, onSave: saveChanges }
			});
		}

		return tabs;
	}

	const mobileItems = $derived(
		availableTabs.map((t) => ({
			id: t.id,
			title: t.title,
			component: t.component,
			props: t.props
		}))
	);

	const tabFromUrl = $derived($page.url.searchParams.get('tab'));

	$effect(() => {
		// Сброс кеша /rest/oc/info (переподключение) должен перечитать гейты
		// возможностей API, иначе они останутся от прошлого подключения.
		void $infoCacheVersion;
		loadSsvcSettings();
		// Возможности API и признак ветки прошивки нужны для гейта полей,
		// появившихся в новых версиях API (см. GeneralSettings).
		loadApiFeatures();

		// Т отбора для панели защиты могла измениться после запуска процесса:
		// обновляем её, не дожидаясь ручного «Обновить».
		const tp1Interval = setInterval(() => loadTp1Target(), 5000);
		return () => clearInterval(tp1Interval);
	});

	$effect(() => {
		const tabId = tabFromUrl;
		if (!ssvcSettings || availableTabs.length === 0) return;
		if (tabId) {
			const index = availableTabs.findIndex((t) => t.id === tabId);
			if (index !== -1) {
				activeTab = index;
				return;
			}
			goto('/oc/ssvc', { replaceState: true });
			return;
		}
		if (activeTab >= availableTabs.length) {
			activeTab = 0;
		}
	});

	$effect(() => {
		const tabId = tabFromUrl;
		if (!browser || !tabId || !ssvcSettings) return;
		tick().then(() => {
			try {
				if (!window.matchMedia(MOBILE_MQ).matches) return;
			} catch {
				return;
			}
			const el = document.getElementById(`oc-ssvc-section-${tabId}`);
			if (el && typeof el.scrollIntoView === 'function') {
				el.scrollIntoView({ block: 'start', behavior: 'instant' });
			}
		});
	});

	function setActiveTab(index: number) {
		activeTab = index;
		const tab = availableTabs[index];
		if (tab) {
			goto(`/oc/ssvc?tab=${tab.id}`, { replaceState: true });
		}
	}
</script>

{#if !ssvcSettings}
		<div class="flex flex-col items-center justify-center gap-2 py-6">
			<p class="loading-text">Загрузка настроек SSVC...</p>
			<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
		</div>
	{:else}
		<div class="hidden md:block">
			<div class="container oc-ssvc-settings-root">
				<div class="tabs-container">
					<div class="tabs tabs-lift tabs-md w-full" role="tablist" aria-label="SSVC settings tabs">
						{#each availableTabs as tab, index}
							<input
								type="radio"
								name="oc_ssvc_tabs"
								role="tab"
								class="tab flex-1 whitespace-nowrap"
								aria-label={tab.title}
								checked={activeTab === index}
								onchange={() => setActiveTab(index)}
							/>
							<div role="tabpanel" class="tab-content mt-4 w-full">
								{#if activeTab === index}
									{@const Component = tab.component}
									<div class="settings-container">
										<Component {...tab.props} />
									</div>
								{/if}
							</div>
						{/each}
					</div>

					<div class="ssvc-refresh-actions mt-6 flex justify-center">
						<button
							type="button"
							class="btn btn-ssvc-refresh"
							title="Перезагрузить настройки"
							onclick={loadSsvcSettings}
						>
							<Reload class="h-5 w-5" />
							<span>Обновить</span>
						</button>
					</div>
				</div>
			</div>
		</div>

		<div class="oc-ssvc-settings-root flex w-full flex-col md:hidden">
			<SettingsMobileScroll
				items={mobileItems}
				sectionIdPrefix="oc-ssvc"
				tabFromUrl={tabFromUrl}
				pageTitle="Настройки SSVC"
				onRefresh={loadSsvcSettings}
				denseGrid={true}
				sectionBodyCard={false}
			/>
		</div>
	{/if}

<style lang="scss">
	.tabs-container {
		display: flex;
		flex-direction: column;
	}

	/* Единая карточная сетка страницы SSVC: секции — самостоятельные карточки
	   на фоне страницы, без вложенного «контейнера-карточки». */
	:global(.oc-ssvc-settings-root .settings-container) {
		width: 100%;
		padding: 0;
		background: transparent;
		border: none;
		box-shadow: none;
		backdrop-filter: none;
	}

	:global(.oc-ssvc-settings-root .settings-grid) {
		gap: 1rem;
	}

	:global(.oc-ssvc-settings-root .settings-grid .settings-panel) {
		display: flex;
		flex-direction: column;
		gap: 1rem;
		overflow: visible;
	}

	:global(.oc-ssvc-settings-root .settings-grid .settings-panel .settings-section) {
		margin: 0;
		padding: 1rem 1.25rem;
		border: 1px solid color-mix(in srgb, var(--color-base-content) 12%, transparent);
		border-bottom: 1px solid color-mix(in srgb, var(--color-base-content) 12%, transparent);
		border-radius: 1rem;
		background: var(--color-base-100);
	}

	:global(.oc-ssvc-settings-root .settings-section-title) {
		color: var(--color-base-content);
	}

	@media (max-width: 767px) {
		:global(.oc-ssvc-settings-root .settings-grid .settings-panel .settings-section) {
			padding: 0.85rem 0.95rem;
		}
	}

	:global(.tabs .tab) {
		font-size: 1.125rem;
		font-weight: 600;
	}

	:global(.tabs .tab:checked) {
		background-color: var(--color-primary);
	}

	.btn-ssvc-refresh {
		background-color: var(--color-info);
		color: oklch(1 0 0);
		border: none;
	}

	.btn-ssvc-refresh:hover {
		opacity: 0.9;
	}
</style>
