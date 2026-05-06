<svelte:options runes />

<script lang="ts">
	import { onMount } from 'svelte';
	import { fetchHardwareConfig, putHardwareConfig } from '$lib/api/ssvcApi';
	import type { OcHardwareConfigResponse } from '$lib/types/ssvc';
	import { page } from '$app/state';
	import { user } from '$lib/stores/user';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import Power from '~icons/tabler/reload';
	import Cancel from '~icons/tabler/x';
	import CircleCheck from '~icons/tabler/circle-check';
	import CircleX from '~icons/tabler/circle-x';

	let { disabled = false, onSaved }: { disabled?: boolean; onSaved?: () => void | Promise<void> } =
		$props();

	let cfg = $state<OcHardwareConfigResponse | null>(null);
	let loadError = $state('');
	let saveError = $state('');
	let isLoading = $state(true);
	let isSaving = $state(false);

	type HardwareSectionId = 'bmp581' | 'ds3231' | 'lcd1602' | 'relay';
	type MobilePanel = 'list' | 'editor';

	const sectionOrder: HardwareSectionId[] = ['bmp581', 'ds3231', 'lcd1602', 'relay'];
	let selectedSection = $state<HardwareSectionId>('bmp581');
	let isNarrowLayout = $state(false);
	let mobilePanel = $state<MobilePanel>('list');

	const MAX_RELAY_CHIPS = 8;
	const I2C_MIN = 8;
	const I2C_MAX = 119;
	const PCF8574_DEFAULT_ADDRESSES = [0x24, 0x25, 0x26, 0x27, 0x20, 0x21, 0x22, 0x23];
	let bmp581HexInput = $state('');
	let ds3231HexInput = $state('');
	let lcd1602HexInput = $state('');
	let relayHexInputs = $state<string[]>([]);

	function formatHexAddress(value: number): string {
		return `0x${Math.trunc(value).toString(16).toUpperCase().padStart(2, '0')}`;
	}

	function parseI2cAddress(raw: string): number | null {
		const v = raw.trim();
		if (!v) return null;
		let parsed = Number.NaN;
		if (/^0x[0-9a-f]+$/i.test(v)) {
			parsed = Number.parseInt(v.slice(2), 16);
		} else if (/^[0-9]+$/.test(v)) {
			parsed = Number.parseInt(v, 10);
		} else if (/^[0-9a-f]+$/i.test(v)) {
			parsed = Number.parseInt(v, 16);
		}
		if (!Number.isInteger(parsed) || parsed < I2C_MIN || parsed > I2C_MAX) {
			return null;
		}
		return parsed;
	}

	function hasDuplicateAddresses(addresses: number[]): boolean {
		return new Set(addresses).size !== addresses.length;
	}

	function chooseNextRelayAddress(): number {
		const used = new Set(
			relayHexInputs
				.map((raw) => parseI2cAddress(raw))
				.filter((address): address is number => address != null)
		);
		const preferred = PCF8574_DEFAULT_ADDRESSES.find((address) => !used.has(address));
		if (preferred != null) {
			return preferred;
		}
		for (let address = I2C_MIN; address <= I2C_MAX; address += 1) {
			if (!used.has(address)) {
				return address;
			}
		}
		return 0x24;
	}

	$effect(() => {
		if (!cfg) return;
		bmp581HexInput = formatHexAddress(cfg.bmp581I2cAddress);
		ds3231HexInput = formatHexAddress(cfg.ds3231I2cAddress);
		lcd1602HexInput = formatHexAddress(cfg.lcd1602I2cAddress ?? 0x27);
		relayHexInputs = cfg.relayPcf8574Addresses.map((a) => formatHexAddress(a));
	});

	async function load() {
		isLoading = true;
		loadError = '';
		try {
			cfg = await fetchHardwareConfig();
			if (cfg && (!cfg.relayPcf8574Addresses || cfg.relayPcf8574Addresses.length === 0)) {
				cfg.relayPcf8574Addresses = [0x24];
			}
			if (cfg) {
				if (typeof cfg.rtcEnabled !== 'boolean') {
					cfg.rtcEnabled = false;
				}
				if (cfg.ds3231I2cAddress == null || Number.isNaN(Number(cfg.ds3231I2cAddress))) {
					cfg.ds3231I2cAddress = 104;
				}
				if (typeof cfg.lcd1602Enabled !== 'boolean') {
					cfg.lcd1602Enabled = false;
				}
				if (cfg.lcd1602I2cAddress == null || Number.isNaN(Number(cfg.lcd1602I2cAddress))) {
					cfg.lcd1602I2cAddress = 0x27;
				}
			}
		} catch (err) {
			loadError = err instanceof Error ? err.message : 'Ошибка загрузки';
		} finally {
			isLoading = false;
		}
	}

	$effect(() => {
		load();
	});

	onMount(() => {
		const mediaQuery = window.matchMedia('(max-width: 960px)');
		const syncLayout = () => {
			isNarrowLayout = mediaQuery.matches;
			if (!mediaQuery.matches) {
				mobilePanel = 'list';
			}
		};
		syncLayout();
		mediaQuery.addEventListener('change', syncLayout);
		return () => mediaQuery.removeEventListener('change', syncLayout);
	});

	async function save() {
		if (!cfg || disabled) return;
		saveError = '';
		const bmpAddr = parseI2cAddress(bmp581HexInput);
		const rtcAddr = parseI2cAddress(ds3231HexInput);
		const lcdAddr = parseI2cAddress(lcd1602HexInput);
		const relayAddrs = relayHexInputs.map((a) => parseI2cAddress(a));
		if (bmpAddr == null || rtcAddr == null || lcdAddr == null || relayAddrs.some((a) => a == null)) {
			saveError = 'Проверьте I2C адреса: используйте 0xNN (или число 8..119).';
			return;
		}
		const parsedRelayAddrs = relayAddrs.filter((address): address is number => address != null);
		if (hasDuplicateAddresses(parsedRelayAddrs)) {
			saveError = 'Адреса PCF8574 для реле не должны повторяться: каждый чип должен иметь уникальный I2C адрес.';
			return;
		}
		cfg.bmp581I2cAddress = bmpAddr;
		cfg.ds3231I2cAddress = rtcAddr;
		cfg.lcd1602I2cAddress = lcdAddr;
		cfg.relayPcf8574Addresses = parsedRelayAddrs;
		isSaving = true;
		try {
			const next = await putHardwareConfig({
				pressureSensorEnabled: cfg.pressureSensorEnabled,
				bmp581I2cAddress: Number(cfg.bmp581I2cAddress),
				userRelayPcfEnabled: cfg.userRelayPcfEnabled,
				relayPcf8574Addresses: cfg.relayPcf8574Addresses.map((a) => Number(a)),
				rtcEnabled: cfg.rtcEnabled,
				ds3231I2cAddress: Number(cfg.ds3231I2cAddress),
				lcd1602Enabled: cfg.lcd1602Enabled,
				lcd1602I2cAddress: Number(cfg.lcd1602I2cAddress)
			});
			if (next) {
				cfg = next;
			}
			await onSaved?.();
		} catch (err) {
			saveError = err instanceof Error ? err.message : 'Ошибка сохранения';
		} finally {
			isSaving = false;
		}
	}

	function openRestartModal() {
		modals.open(ConfirmDialog, {
			title: 'Перезагрузка устройства',
			message:
				'После изменения оборудования нужна перезагрузка контроллера. Перезагрузить сейчас?',
			labels: {
				cancel: { label: 'Позже', icon: Cancel },
				confirm: { label: 'Перезагрузить', icon: Power }
			},
			onConfirm: () => {
				modals.close();
				void fetch('/rest/restart', {
					method: 'POST',
					headers: {
						Authorization: page.data.features.security
							? 'Bearer ' + $user.bearer_token
							: 'Basic'
					}
				});
			}
		});
	}

	const relayLocked = $derived(cfg?.capabilities?.relayOptionsAvailable !== true);
	const showRebootBanner = $derived(cfg?.needsReboot === true || cfg?.pendingReboot === true);

	const healthyModules = $derived.by(() => {
		if (!cfg) return 0;
		let healthy = 0;
		if (cfg.pressureSensorEnabled && cfg.bmp581ProbeOk === true) healthy += 1;
		if (cfg.rtcEnabled && cfg.ds3231ProbeOk === true) healthy += 1;
		if (cfg.lcd1602Enabled && cfg.lcd1602ProbeOk === true) healthy += 1;
		if (cfg.userRelayPcfEnabled && cfg.relayHardwareOk === true) healthy += 1;
		return healthy;
	});

	function addRelayChip() {
		if (!cfg || cfg.relayPcf8574Addresses.length >= MAX_RELAY_CHIPS) return;
		const nextAddress = chooseNextRelayAddress();
		cfg.relayPcf8574Addresses = [...cfg.relayPcf8574Addresses, nextAddress];
		relayHexInputs = [...relayHexInputs, formatHexAddress(nextAddress)];
	}

	function removeRelayChip(idx: number) {
		if (!cfg || cfg.relayPcf8574Addresses.length <= 1) return;
		cfg.relayPcf8574Addresses = cfg.relayPcf8574Addresses.filter((_, i) => i !== idx);
		relayHexInputs = relayHexInputs.filter((_, i) => i !== idx);
	}

	function commitBmpAddress() {
		if (!cfg) return;
		const parsed = parseI2cAddress(bmp581HexInput);
		if (parsed == null) {
			saveError = 'Неверный адрес BMP581. Пример: 0x47';
			return;
		}
		cfg.bmp581I2cAddress = parsed;
		bmp581HexInput = formatHexAddress(parsed);
		saveError = '';
	}

	function commitDsAddress() {
		if (!cfg) return;
		const parsed = parseI2cAddress(ds3231HexInput);
		if (parsed == null) {
			saveError = 'Неверный адрес DS3231. Пример: 0x68';
			return;
		}
		cfg.ds3231I2cAddress = parsed;
		ds3231HexInput = formatHexAddress(parsed);
		saveError = '';
	}

	function commitLcdAddress() {
		if (!cfg) return;
		const parsed = parseI2cAddress(lcd1602HexInput);
		if (parsed == null) {
			saveError = 'Неверный адрес LCD1602. Пример: 0x27';
			return;
		}
		cfg.lcd1602I2cAddress = parsed;
		lcd1602HexInput = formatHexAddress(parsed);
		saveError = '';
	}

	function updateRelayHexInput(idx: number, value: string) {
		const next = [...relayHexInputs];
		next[idx] = value;
		relayHexInputs = next;
	}

	function commitRelayAddress(idx: number) {
		if (!cfg) return;
		const parsed = parseI2cAddress(relayHexInputs[idx] ?? '');
		if (parsed == null) {
			saveError = `Неверный адрес PCF8574 для чипа ${idx + 1}. Пример: 0x24`;
			return;
		}
		cfg.relayPcf8574Addresses[idx] = parsed;
		updateRelayHexInput(idx, formatHexAddress(parsed));
		saveError = '';
	}

	function sectionTitle(id: HardwareSectionId): string {
		if (id === 'bmp581') return 'Датчик давления (BMP581)';
		if (id === 'ds3231') return 'Часы реального времени (DS3231)';
		if (id === 'lcd1602') return 'LCD1602 (I2C дисплей)';
		return 'Реле / PCF8574';
	}

	function sectionStatus(id: HardwareSectionId): string {
		if (!cfg) return 'Нет данных';
		if (id === 'bmp581') return cfg.pressureSensorEnabled ? 'Включен' : 'Выключен';
		if (id === 'ds3231') return cfg.rtcEnabled ? 'Включен' : 'Выключен';
		if (id === 'lcd1602') return cfg.lcd1602Enabled ? 'Включен' : 'Выключен';
		if (relayLocked) return 'Зарезервирован';
		return cfg.userRelayPcfEnabled ? 'Включен' : 'Выключен';
	}

	function sectionStatusBadgeClass(id: HardwareSectionId): string {
		if (!cfg) return 'badge-ghost';
		if (id === 'bmp581') {
			if (!cfg.pressureSensorEnabled) return 'badge-ghost';
			if (cfg.bmp581ProbeOk === true) return 'badge-success';
			if (cfg.bmp581ProbeOk === false) return 'badge-error';
			return 'badge-warning';
		}
		if (id === 'ds3231') {
			if (!cfg.rtcEnabled) return 'badge-ghost';
			if (cfg.ds3231ProbeOk === true) return 'badge-success';
			if (cfg.ds3231ProbeOk === false) return 'badge-error';
			return 'badge-warning';
		}
		if (id === 'lcd1602') {
			if (!cfg.lcd1602Enabled) return 'badge-ghost';
			if (cfg.lcd1602ProbeOk === true) return 'badge-success';
			if (cfg.lcd1602ProbeOk === false) return 'badge-error';
			return 'badge-warning';
		}
		if (relayLocked) return 'badge-ghost';
		if (!cfg.userRelayPcfEnabled) return 'badge-ghost';
		if (cfg.relayHardwareOk === true) return 'badge-success';
		if (cfg.relayHardwareOk === false) return 'badge-error';
		return 'badge-warning';
	}

	function sectionSummary(id: HardwareSectionId): string {
		if (!cfg) return 'Конфигурация недоступна';
		if (id === 'bmp581') {
			return `I2C ${formatHexAddress(cfg.bmp581I2cAddress)} (${cfg.bmp581I2cAddress})`;
		}
		if (id === 'ds3231') {
			return `I2C ${formatHexAddress(cfg.ds3231I2cAddress)} (${cfg.ds3231I2cAddress})`;
		}
		if (id === 'lcd1602') {
			const address = cfg.lcd1602I2cAddress ?? 0x27;
			return `I2C ${formatHexAddress(address)} (${address})`;
		}
		const chips = cfg.relayPcf8574Addresses.length;
		return `${chips} чип(ов)`;
	}

	function openSection(id: HardwareSectionId) {
		selectedSection = id;
		if (isNarrowLayout) {
			mobilePanel = 'editor';
		}
	}

	function showSectionList() {
		mobilePanel = 'list';
	}

	function previousSection() {
		const idx = sectionOrder.indexOf(selectedSection);
		if (idx > 0) {
			openSection(sectionOrder[idx - 1]);
		}
	}

	function nextSection() {
		const idx = sectionOrder.indexOf(selectedSection);
		if (idx < sectionOrder.length - 1) {
			openSection(sectionOrder[idx + 1]);
		}
	}

	function isFirstSection(): boolean {
		return sectionOrder.indexOf(selectedSection) === 0;
	}

	function isLastSection(): boolean {
		return sectionOrder.indexOf(selectedSection) === sectionOrder.length - 1;
	}
</script>

<div class="hardware-config settings-container">
	{#if isLoading}
		<div class="flex flex-col items-center gap-2 py-8">
			<p class="text-base-content/90">Загрузка конфигурации оборудования…</p>
			<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
		</div>
	{:else if loadError}
		<p class="text-error">{loadError}</p>
		<button type="button" class="btn btn-outline btn-sm mt-2" onclick={() => load()}>Повторить</button>
	{:else if cfg}
		<div class="hardware-shell">
			<div class="hardware-header">
				<div>
					<h3 class="hardware-title">Конфигурация оборудования</h3>
					<p class="hardware-subtitle">
						Активные и подтвержденные модули: {healthyModules} из 4
					</p>
				</div>
				<button
					type="button"
					class="btn btn-primary"
					disabled={disabled || isSaving}
					onclick={() => save()}
				>
					{#if isSaving}
						<span class="loading loading-spinner loading-sm"></span>
					{/if}
					Сохранить
				</button>
			</div>

			{#if saveError}
				<div class="alert alert-error py-2">
					<span>{saveError}</span>
				</div>
			{/if}

			<div class="hardware-layout">
				{#if !isNarrowLayout || mobilePanel === 'list'}
					<div class="sections-list">
						{#each sectionOrder as sectionId, idx (sectionId)}
							<button
								type="button"
								class="section-item"
								class:active={selectedSection === sectionId}
								onclick={() => openSection(sectionId)}
							>
								<div class="section-item-top">
									<span class="section-item-title">{sectionTitle(sectionId)}</span>
									<span class="badge badge-sm {sectionStatusBadgeClass(sectionId)}">
										{sectionStatus(sectionId)}
									</span>
								</div>
								<p class="section-item-summary">{sectionSummary(sectionId)}</p>
							</button>
							{#if idx < sectionOrder.length - 1}
								<div class="section-row-divider" aria-hidden="true"></div>
							{/if}
						{/each}
					</div>
				{/if}

				{#if !isNarrowLayout}
					<div class="columns-divider" aria-hidden="true"></div>
				{/if}

				{#if !isNarrowLayout || mobilePanel === 'editor'}
					<div class="section-editor">
						{#if isNarrowLayout}
							<div class="mobile-editor-nav">
								<div class="mobile-editor-title-wrap">
									<span class="mobile-editor-caption">Редактор устройства</span>
									<span class="mobile-editor-title">{sectionTitle(selectedSection)}</span>
									<div class="mobile-editor-meta">
										<span class="badge badge-sm badge-ghost">{sectionStatus(selectedSection)}</span>
										<span class="mobile-editor-summary">{sectionSummary(selectedSection)}</span>
									</div>
								</div>
								<div class="mobile-nav-actions">
									<button
										type="button"
										class="btn btn-sm btn-outline mobile-nav-btn"
										onclick={showSectionList}
									>
										К списку
									</button>
									<button
										type="button"
										class="btn btn-sm btn-outline mobile-nav-btn"
										onclick={previousSection}
										disabled={isFirstSection()}
									>
										Назад
									</button>
									<button
										type="button"
										class="btn btn-sm btn-outline mobile-nav-btn"
										onclick={nextSection}
										disabled={isLastSection()}
									>
										Далее
									</button>
								</div>
							</div>
						{/if}

						{#if selectedSection === 'bmp581'}
							<div class="editor-step">
								<div class="step-title">1. Состояние</div>
								<label class="label-cb">
									<input
										type="checkbox"
										class="toggle toggle-primary"
										bind:checked={cfg.pressureSensorEnabled}
										disabled={disabled}
									/>
									<span>Включить модуль давления на шине I²C</span>
								</label>
							</div>

							<div class="editor-step">
								<div class="step-title">2. Параметры</div>
								<label class="form-control w-full max-w-sm">
									<span class="label-text">Адрес I²C BMP581 (7-bit, HEX)</span>
									<input
										type="text"
										inputmode="text"
										placeholder="0x47"
										class="input input-bordered w-full"
										bind:value={bmp581HexInput}
										onblur={commitBmpAddress}
										disabled={disabled}
									/>
									<span class="label-text-alt">Например: 0x46, 0x47 (допустимо также 70, 71)</span>
								</label>
							</div>

							<div class="editor-step">
								{#if showRebootBanner}
									<div class="alert alert-warning py-2" role="status">
										<span>Изменения применятся после перезагрузки.</span>
										<button type="button" class="btn btn-xs btn-neutral" onclick={openRestartModal}>
											Перезагрузить
										</button>
									</div>
								{/if}
							</div>
						{:else if selectedSection === 'ds3231'}
							<div class="editor-step">
								<div class="step-title">1. Состояние</div>
								<label class="label-cb">
									<input
										type="checkbox"
										class="toggle toggle-primary"
										bind:checked={cfg.rtcEnabled}
										disabled={disabled}
									/>
									<span>Включить DS3231 на шине I²C</span>
								</label>
							</div>

							<div class="editor-step">
								<div class="step-title">2. Параметры</div>
								<label class="form-control w-full max-w-sm">
									<span class="label-text">Адрес I²C DS3231 (7-bit, HEX)</span>
									<input
										type="text"
										inputmode="text"
										placeholder="0x68"
										class="input input-bordered w-full"
										bind:value={ds3231HexInput}
										onblur={commitDsAddress}
										disabled={disabled}
									/>
									<span class="label-text-alt">По умолчанию: 0x68 (104)</span>
								</label>
							</div>

							<div class="editor-step">
								{#if showRebootBanner}
									<div class="alert alert-warning py-2" role="status">
										<span>Изменения применятся после перезагрузки.</span>
										<button type="button" class="btn btn-xs btn-neutral" onclick={openRestartModal}>
											Перезагрузить
										</button>
									</div>
								{/if}
							</div>
						{:else if selectedSection === 'lcd1602'}
							<div class="editor-step">
								<div class="step-title">1. Состояние</div>
								<label class="label-cb">
									<input
										type="checkbox"
										class="toggle toggle-primary"
										bind:checked={cfg.lcd1602Enabled}
										disabled={disabled}
									/>
									<span>Включить LCD1602 на шине I²C</span>
								</label>
							</div>

							<div class="editor-step">
								<div class="step-title">2. Параметры</div>
								<label class="form-control w-full max-w-sm">
									<span class="chip-address-head">
										<span class="label-text">Адрес I²C LCD1602 (7-bit, HEX)</span>
										{#if cfg.lcd1602ProbeOk === true}
											<span class="chip-probe chip-probe-ok">
												<CircleCheck class="h-4 w-4" />
												OK
											</span>
										{:else if cfg.lcd1602ProbeOk === false}
											<span class="chip-probe chip-probe-fail">
												<CircleX class="h-4 w-4" />
												Нет ответа
											</span>
										{:else}
											<span class="chip-probe chip-probe-unknown">Нет данных</span>
										{/if}
									</span>
									<input
										type="text"
										inputmode="text"
										placeholder="0x27"
										class="input input-bordered w-full"
										bind:value={lcd1602HexInput}
										onblur={commitLcdAddress}
										disabled={disabled}
									/>
									<span class="label-text-alt">По умолчанию: 0x27 (39)</span>
								</label>
							</div>

							<div class="editor-step">
								{#if showRebootBanner}
									<div class="alert alert-warning py-2" role="status">
										<span>Изменения применятся после перезагрузки.</span>
										<button type="button" class="btn btn-xs btn-neutral" onclick={openRestartModal}>
											Перезагрузить
										</button>
									</div>
								{/if}
							</div>
						{:else}
							<div class="editor-step">
								<div class="step-title">1. Состояние</div>
								{#if relayLocked}
									<div class="alert alert-info py-2">
										<span>
											В этой сборке прошивки расширитель реле по I²C не используется — поля
											зарезервированы.
										</span>
									</div>
								{/if}
								<label class="label-cb">
									<input
										type="checkbox"
										class="toggle toggle-primary"
										bind:checked={cfg.userRelayPcfEnabled}
										disabled={disabled || relayLocked}
									/>
									<span>Реле на PCF8574</span>
								</label>
							</div>

							<div class="editor-step">
								<div class="step-title">2. Параметры</div>
								<p class="step-hint">
									Порядок адресов задает порядок чипов; первый используется линиями системной тревоги.
								</p>
								<ul class="relay-chip-list">
									{#each cfg.relayPcf8574Addresses as _, chipIdx (chipIdx)}
										<li class="relay-chip-row">
											<div class="relay-chip-main">
												<span class="chip-index">Чип {chipIdx + 1}</span>
												<label class="form-control w-full max-w-xs">
													<span class="chip-address-head">
														<span class="label-text">Адрес PCF8574 (7-bit)</span>
														{#if cfg.relayPcf8574ProbeOk != null && cfg.relayPcf8574ProbeOk[chipIdx] !== undefined}
															<span
																class="chip-probe chip-probe-{cfg.relayPcf8574ProbeOk[chipIdx] ? 'ok' : 'fail'}"
															>
																{#if cfg.relayPcf8574ProbeOk[chipIdx]}
																	<CircleCheck class="h-4 w-4" />
																	OK
																{:else}
																	<CircleX class="h-4 w-4" />
																	Нет ответа
																{/if}
															</span>
														{:else}
															<span class="chip-probe chip-probe-unknown">Нет данных</span>
														{/if}
													</span>
													<input
														type="text"
														inputmode="text"
														placeholder="0x24"
														class="input input-bordered w-full"
														value={relayHexInputs[chipIdx] ?? ''}
														oninput={(e) =>
															updateRelayHexInput(chipIdx, (e.currentTarget as HTMLInputElement).value)}
														onblur={() => commitRelayAddress(chipIdx)}
														disabled={disabled || relayLocked}
													/>
												</label>
											</div>
											<button
												type="button"
												class="btn btn-ghost btn-sm"
												disabled={disabled || relayLocked || cfg.relayPcf8574Addresses.length <= 1}
												onclick={() => removeRelayChip(chipIdx)}
											>
												Удалить
											</button>
										</li>
									{/each}
								</ul>
								<button
									type="button"
									class="btn btn-outline btn-sm"
									disabled={disabled || relayLocked || cfg.relayPcf8574Addresses.length >= MAX_RELAY_CHIPS}
									onclick={() => addRelayChip()}
								>
									Добавить чип
								</button>
							</div>

							<div class="editor-step">							
								{#if cfg.relayHardwareOk === false}
									<div class="alert alert-warning py-2">
										<span>
											Не все реле-модули отвечают на I²C. После сохранения проверьте проводку и
											перезагрузку.
										</span>
									</div>
								{/if}
								{#if showRebootBanner}
									<div class="alert alert-warning py-2" role="status">
										<span>Изменения применятся после перезагрузки.</span>
										<button type="button" class="btn btn-xs btn-neutral" onclick={openRestartModal}>
											Перезагрузить
										</button>
									</div>
								{/if}
							</div>
						{/if}
					</div>
				{/if}
			</div>
		</div>
	{/if}
</div>

<style lang="scss">
	.hardware-config {
		width: 100%;
	}

	.hardware-shell {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
	}

	.hardware-header {
		display: flex;
		justify-content: space-between;
		align-items: start;
		gap: 0.75rem;
	}

	.hardware-title {
		font-size: 1.1rem;
		font-weight: 600;
	}

	.hardware-subtitle {
		font-size: 0.9rem;
		color: hsl(var(--bc) / 0.85);
	}

	.hardware-layout {
		display: grid;
		grid-template-columns: 320px 2px minmax(0, 1fr);
		column-gap: 0.75rem;
		row-gap: 0;
		align-items: stretch;
	}

	.columns-divider {
		width: 2px;
		height: 100%;
		border-radius: 999px;
		background: var(--border-color);
		opacity: 0.75;
	}

	.sections-list,
	.section-editor {
		border: 1px solid hsl(var(--b3));
		border-radius: 0.75rem;
		background: hsl(var(--b1));
		padding: 0.75rem;
	}

	.sections-list {
		display: flex;
		flex-direction: column;
		gap: 0.3rem;
	}

	.section-item {
		display: flex;
		flex-direction: column;
		align-items: stretch;
		gap: 0.5rem;
		width: 100%;
		min-height: 74px;
		padding: 0.75rem;
		border: 1px solid hsl(var(--b3));
		border-radius: 0.6rem;
		text-align: left;
		background: hsl(var(--b1));
		transition: border-color 0.15s ease, background-color 0.15s ease, box-shadow 0.15s ease;
	}

	.section-item:hover {
		border-color: hsl(var(--p) / 0.5);
	}

	.section-item.active {
		border-color: hsl(var(--p));
		background: hsl(var(--p) / 0.1);
		box-shadow: inset 3px 0 0 hsl(var(--p));
	}

	.section-row-divider {
		height: 1px;
		margin: 0 0.25rem;
		background: var(--border-color);
		opacity: 0.65;
	}

	.section-item-top {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.5rem;
	}

	.section-item-title {
		font-size: 0.95rem;
		font-weight: 600;
	}

	.section-item-summary {
		font-size: 0.86rem;
		color: hsl(var(--bc) / 0.85);
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.section-editor {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
		min-height: 620px;
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

	.tech-details {
		font-size: 0.86rem;
	}

	.relay-chip-list {
		display: flex;
		flex-direction: column;
		gap: 0.6rem;
	}

	.relay-chip-row {
		display: flex;
		align-items: end;
		justify-content: space-between;
		gap: 0.75rem;
		padding: 0.65rem;
		border: 1px solid hsl(var(--b3));
		border-radius: 0.6rem;
	}

	.relay-chip-main {
		display: flex;
		flex-direction: column;
		gap: 0.5rem;
	}

	.chip-index {
		font-size: 0.86rem;
		font-weight: 500;
	}

	.chip-address-head {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.4rem;
	}

	.chip-probe {
		display: inline-flex;
		align-items: center;
		gap: 0.2rem;
		font-size: 0.72rem;
		font-weight: 600;
		white-space: nowrap;
	}

	.chip-probe-ok {
		color: hsl(var(--su));
	}

	.chip-probe-fail {
		color: hsl(var(--er));
	}

	.chip-probe-unknown {
		color: hsl(var(--bc) / 0.65);
		font-size: 0.72rem;
	}

	.mobile-editor-nav {
		display: flex;
		flex-direction: column;
		align-items: stretch;
		gap: 0.5rem;
		padding: 0.65rem;
		margin-bottom: 0.35rem;
		border: 1px solid hsl(var(--b3));
		border-radius: 0.6rem;
		background: hsl(var(--b1));
	}

	.mobile-editor-title-wrap {
		display: flex;
		flex-direction: column;
		gap: 0.15rem;
	}

	.mobile-editor-caption {
		font-size: 0.75rem;
		color: hsl(var(--bc) / 0.7);
	}

	.mobile-editor-title {
		font-size: 0.95rem;
		font-weight: 600;
	}

	.mobile-editor-meta {
		display: inline-flex;
		align-items: center;
		gap: 0.4rem;
		flex-wrap: wrap;
	}

	.mobile-editor-summary {
		font-size: 0.8rem;
		color: hsl(var(--bc) / 0.75);
	}

	.mobile-nav-actions {
		display: flex;
		align-items: center;
		gap: 0.35rem;
		flex-wrap: wrap;
		padding: 0.25rem;
		border-radius: 0.5rem;
		background: hsl(var(--b1));
	}

	.mobile-nav-btn {
		background: hsl(var(--b1));
	}

	@media (max-width: 960px) {
		.hardware-header {
			flex-wrap: wrap;
		}

		.hardware-layout {
			grid-template-columns: 1fr;
			gap: 0.5rem;
		}

		.section-editor {
			min-height: auto;
			padding-top: 0.75rem;
		}
	}
</style>
