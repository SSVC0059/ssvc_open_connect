<svelte:options runes />

<script lang="ts">
	import { onMount } from 'svelte';
	import {
		fetchHardwareConfig,
		fetchHardwareFaults,
		fetchXkc25Status,
		putHardwareConfig
	} from '$lib/api/ssvcApi';
	import type {
		OcHardwareConfigResponse,
		OcHardwareFaultsResponse,
		Xkc25SensorConfig,
		Xkc25StartGuardMode,
		Xkc25StatusResponse,
		Xkc25TriggerAction
	} from '$lib/types/ssvc';
	import { modals } from 'svelte-modals';
	import RebootProgressDialog from '$lib/components/RebootProgressDialog.svelte';
	import Xkc25Section from '$lib/components/OCSettings/Xkc25Section.svelte';
	import CircleCheck from '~icons/tabler/circle-check';
	import CircleX from '~icons/tabler/circle-x';

	let { disabled = false, onSaved }: { disabled?: boolean; onSaved?: () => void | Promise<void> } =
		$props();

	let cfg = $state<OcHardwareConfigResponse | null>(null);
	let loadError = $state('');
	let saveError = $state('');
	let isLoading = $state(true);
	let isSaving = $state(false);

	/** Состояние секции XKC-25 держим отдельно: поля конфига необязательные, а редактору нужны значения. */
	let xkc25Sensors = $state<Xkc25SensorConfig[]>([]);
	/** Реакция на срабатывание — взаимоисключающий режим: 0 уведомление / 1 пауза / 2 остановка. */
	let xkc25TriggerAction = $state<Xkc25TriggerAction>(1);
	/** Ёмкость, на которой сейчас стоит переносной датчик (один прибор на все ёмкости). */
	let xkc25SharedTankIndex = $state(0);
	let xkc25StartGuardMode = $state<Xkc25StartGuardMode>(1);
	let xkc25Status = $state<Xkc25StatusResponse | null>(null);
	/** Последний опрос статуса не удался: живые данные показываем как устаревшие. */
	let xkc25StatusStale = $state(false);
	let xkc25CalibrationBusy = $state(false);

	/**
	 * Активные аппаратные сбои. Проверки внутри конфига отвечают лишь на вопрос
	 * «устройство ответило в момент запроса», а здесь — то, что монитор аварий
	 * уже зафиксировал (включая повторные попытки дисплея, RTC и реле).
	 */
	let hardwareFaults = $state<OcHardwareFaultsResponse | null>(null);

	type HardwareSectionId = 'bmp581' | 'ds3231' | 'lcd1602' | 'relay' | 'xkc25';
	type MobilePanel = 'list' | 'editor';

	const sectionOrder: HardwareSectionId[] = ['bmp581', 'ds3231', 'lcd1602', 'relay', 'xkc25'];
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

	/**
	 * Переносит поля XKC-25 из ответа контроллера в состояние редактора.
	 * Вызывается только при загрузке и после сохранения, чтобы не затирать
	 * несохранённые правки секции при изменении других полей конфига.
	 */
	function syncXkc25State(config: OcHardwareConfigResponse) {
		xkc25Sensors = (config.xkc25Sensors ?? []).map((sensor) => ({ ...sensor }));
		const action = config.xkc25TriggerAction;
		xkc25TriggerAction = action === 0 || action === 1 || action === 2 ? action : 1;
		const tank = Number(config.xkc25SharedTankIndex ?? 0);
		xkc25SharedTankIndex = tank === 0 || tank === 1 || tank === 2 ? tank : 0;
		const mode = config.xkc25StartGuardMode;
		xkc25StartGuardMode = mode === 0 || mode === 1 || mode === 2 ? mode : 1;
	}

	async function refreshXkc25Status() {
		try {
			const next = await fetchXkc25Status();
			if (next) {
				xkc25Status = next;
				xkc25StatusStale = false;
				return;
			}
			// Ответа с данными нет: держим прежние значения, но помечаем их устаревшими.
			xkc25StatusStale = true;
		} catch {
			xkc25StatusStale = true;
		}
	}

	async function refreshHardwareFaults() {
		try {
			const next = await fetchHardwareFaults();
			// Сбой опроса не сбрасывает список: показываем последние известные данные.
			if (next) {
				hardwareFaults = next;
			}
		} catch {
			// Список сбоев — вспомогательный: ошибку сети здесь молча пропускаем.
		}
	}

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
				syncXkc25State(cfg);
			}
			await refreshXkc25Status();
			await refreshHardwareFaults();
		} catch (err) {
			loadError = err instanceof Error ? err.message : 'Ошибка загрузки';
		} finally {
			isLoading = false;
		}
	}

	$effect(() => {
		load();
	});

	/** Живой уровень пина нужен только в открытой секции XKC-25: опрос ~1 Гц. */
	$effect(() => {
		// Во время шага калибровки опрос не нужен: он даёт лишнюю нагрузку и гонку за состояние.
		if (selectedSection !== 'xkc25' || isLoading || xkc25CalibrationBusy) return;
		const timer = setInterval(() => {
			void refreshXkc25Status();
		}, 1000);
		return () => clearInterval(timer);
	});

	/** Секция размонтируется при переходе к другому модулю: занятость калибровки снимаем. */
	$effect(() => {
		if (selectedSection !== 'xkc25' && xkc25CalibrationBusy) {
			xkc25CalibrationBusy = false;
		}
	});

	/**
	 * Сбои появляются и снимаются асинхронно (у дисплея — повторные попытки
	 * с нарастающей паузой), поэтому список обновляем и по таймеру.
	 */
	$effect(() => {
		if (isLoading) return;
		const timer = setInterval(() => {
			void refreshHardwareFaults();
		}, 5000);
		return () => clearInterval(timer);
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
		const xkc25Pins = xkc25Sensors.map((sensor) => Number(sensor.gpio));
		if (xkc25Pins.some((pin) => !Number.isInteger(pin) || pin < 0 || pin > 39)) {
			saveError = 'Проверьте пины датчиков уровня: допустимы GPIO 0..39.';
			return;
		}
		if (new Set(xkc25Pins).size !== xkc25Pins.length) {
			saveError = 'У датчиков уровня не должны повторяться пины: каждый датчик подключается к своему GPIO.';
			return;
		}
		if (
			xkc25Sensors.some(
				(sensor) =>
					!Number.isInteger(sensor.zoneIndex) || sensor.zoneIndex < 0 || sensor.zoneIndex > 2
			)
		) {
			saveError = 'Проверьте привязку датчиков уровня к приёмным ёмкостям (1..3).';
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
				lcd1602I2cAddress: Number(cfg.lcd1602I2cAddress),
				// Калибровочные поля не отправляем: прошивка их по PUT игнорирует (см. 6.1.3).
				xkc25Sensors: xkc25Sensors.map((sensor) => ({
					enabled: sensor.enabled,
					gpio: Number(sensor.gpio),
					activeLow: sensor.activeLow,
					zoneIndex: Number(sensor.zoneIndex),
					sharedAcrossTanks: sensor.sharedAcrossTanks === true
				})),
				xkc25TriggerAction,
				xkc25SharedTankIndex,
				xkc25StartGuardMode
			});
			if (next) {
				cfg = next;
				syncXkc25State(next);
			}
			await refreshXkc25Status();
			await onSaved?.();
		} catch (err) {
			saveError = err instanceof Error ? err.message : 'Ошибка сохранения';
		} finally {
			isSaving = false;
		}
	}

	/**
	 * Перезагрузка контроллера: диалог сам отправляет команду и показывает шкалу,
	 * пока устройство не вернётся на связь. Данные перечитываем только после
	 * возврата — иначе запросы уходят в перезагружающийся контроллер.
	 */
	function openRestartModal() {
		modals.open(RebootProgressDialog, {
			title: 'Перезагрузка устройства',
			message: 'После изменения оборудования нужна перезагрузка контроллера. Перезагрузить сейчас?',
			onFinished: async () => {
				await load();
				await onSaved?.();
			}
		});
	}

	const relayLocked = $derived(cfg?.capabilities?.relayOptionsAvailable !== true);
	const showRebootBanner = $derived(cfg?.needsReboot === true || cfg?.pendingReboot === true);

	/**
	 * Структурный ключ подключения: пока он расходится с прошивкой, слоты датчиков
	 * в редакторе и в контроллере не совпадают, и калибровать нельзя.
	 */
	function xkc25WiringKey(list: Xkc25SensorConfig[]): string {
		return JSON.stringify(
			list.map((sensor) => [
				sensor.enabled,
				Number(sensor.gpio),
				Number(sensor.zoneIndex),
				sensor.activeLow
			])
		);
	}

	const xkc25Dirty = $derived(
		xkc25WiringKey(xkc25Sensors) !== xkc25WiringKey(cfg?.xkc25Sensors ?? [])
	);

	const xkc25EnabledSensors = $derived(xkc25Sensors.filter((sensor) => sensor.enabled));
	/** У GPIO нет подтверждения наличия датчика: подтверждением считаем пройденную калибровку. */
	const xkc25AllCalibrated = $derived(
		xkc25EnabledSensors.length > 0 &&
			xkc25EnabledSensors.every((sensor) => sensor.calibrated === true)
	);

	const healthyModules = $derived.by(() => {
		if (!cfg) return 0;
		let healthy = 0;
		if (cfg.pressureSensorEnabled && cfg.bmp581ProbeOk === true) healthy += 1;
		if (cfg.rtcEnabled && cfg.ds3231ProbeOk === true) healthy += 1;
		if (cfg.lcd1602Enabled && cfg.lcd1602ProbeOk === true) healthy += 1;
		if (cfg.userRelayPcfEnabled && cfg.relayHardwareOk === true) healthy += 1;
		if (xkc25AllCalibrated) healthy += 1;
		return healthy;
	});

	const activeHardwareFaults = $derived(hardwareFaults?.faults ?? []);
	const i2cBusDown = $derived(hardwareFaults?.i2cBusActive === false);
	/** Кто ответил на шине в момент последнего опроса — для локализации сбоя. */
	const i2cScanLabel = $derived(
		(hardwareFaults?.i2cScan ?? []).map((address) => formatHexAddress(address)).join(', ')
	);

	const FAULT_ROLE_LABELS: Record<string, string> = {
		i2c_bus: 'Шина I²C',
		lcd1602: 'Дисплей LCD1602',
		ds3231: 'Часы DS3231',
		bmp581: 'Датчик давления BMP581'
	};

	/** Роли расширителей реле нумерованы: `pcf8574_port0` … `pcf8574_port7`. */
	function faultRoleLabel(role: string): string {
		const known = FAULT_ROLE_LABELS[role];
		if (known) return known;
		const portMatch = /^pcf8574_port(\d+)$/.exec(role);
		if (portMatch) {
			return `Расширитель PCF8574 №${Number(portMatch[1]) + 1}`;
		}
		return role;
	}

	/**
	 * Подпись под полем адреса: показывает, во что разобрано введённое значение.
	 * Нужна из-за неоднозначности записи без префикса: «27» — это десятичное 27
	 * (0x1B), а не 0x27 (39).
	 */
	function addressEcho(raw: string): string {
		const parsed = parseI2cAddress(raw);
		if (parsed == null) {
			return 'Не разобран: укажите 0xNN или число 8..119';
		}
		return `Распознан адрес ${formatHexAddress(parsed)} (десятичное ${parsed})`;
	}

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
		if (id === 'xkc25') return 'Датчики уровня (XKC-25)';
		return 'Реле / PCF8574';
	}

	function sectionStatus(id: HardwareSectionId): string {
		if (!cfg) return 'Нет данных';
		if (id === 'bmp581') return cfg.pressureSensorEnabled ? 'Включен' : 'Выключен';
		if (id === 'ds3231') return cfg.rtcEnabled ? 'Включен' : 'Выключен';
		if (id === 'lcd1602') return cfg.lcd1602Enabled ? 'Включен' : 'Выключен';
		if (id === 'xkc25') {
			if (xkc25EnabledSensors.length === 0) return 'Выключен';
			return xkc25AllCalibrated ? 'Включен' : 'Не откалиброван';
		}
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
		if (id === 'xkc25') {
			if (xkc25EnabledSensors.length === 0) return 'badge-ghost';
			// У GPIO нет пробинга: подтверждением считаем пройденную калибровку.
			return xkc25AllCalibrated ? 'badge-success' : 'badge-warning';
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
		if (id === 'xkc25') {
			if (xkc25Sensors.length === 0) return 'Датчики не добавлены';
			return `${xkc25Sensors.length} из 3 · включено ${xkc25EnabledSensors.length}`;
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
						Активные и подтвержденные модули: {healthyModules} из 5
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

			{#if i2cBusDown || activeHardwareFaults.length > 0}
				<div class="alert alert-error py-2 hardware-faults" role="alert">
					<div class="hardware-faults-body">
						{#if i2cBusDown}
							<span class="hardware-faults-title">
								Шина I²C недоступна: устройства на ней не опрашиваются.
							</span>
						{/if}
						{#if activeHardwareFaults.length > 0}
							<span class="hardware-faults-title">
								Аппаратные сбои: {activeHardwareFaults.length}
							</span>
							<ul class="hardware-faults-list">
								{#each activeHardwareFaults as fault (fault.role + ':' + fault.address)}
									<li>
										<span class="hardware-faults-role">{faultRoleLabel(fault.role)}</span>
										{#if fault.address > 0}
											<span class="hardware-faults-addr">{formatHexAddress(fault.address)}</span>
										{/if}
										<span class="hardware-faults-code">{fault.codeLabel}</span>
									</li>
								{/each}
							</ul>
						{/if}
						{#if hardwareFaults && !i2cBusDown}
							<span class="hardware-faults-scan">
								Отвечают на шине: {i2cScanLabel || 'никто'}
							</span>
						{/if}
					</div>
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
									<span class="label-text-alt address-echo">{addressEcho(bmp581HexInput)}</span>
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
									<span class="label-text-alt address-echo">{addressEcho(ds3231HexInput)}</span>
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
									<span class="label-text-alt address-echo">{addressEcho(lcd1602HexInput)}</span>
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
						{:else if selectedSection === 'xkc25'}
							<Xkc25Section
								bind:sensors={xkc25Sensors}
								bind:triggerAction={xkc25TriggerAction}
								bind:sharedTankIndex={xkc25SharedTankIndex}
								bind:startGuardMode={xkc25StartGuardMode}
								bind:busy={xkc25CalibrationBusy}
								status={xkc25Status}
								stale={xkc25StatusStale}
								dirty={xkc25Dirty}
								onStatusRefresh={refreshXkc25Status}
								{disabled}
							/>

							{#if showRebootBanner}
								<div class="editor-step">
									<div class="alert alert-warning py-2" role="status">
										<span>Изменения применятся после перезагрузки.</span>
										<button type="button" class="btn btn-xs btn-neutral" onclick={openRestartModal}>
											Перезагрузить
										</button>
									</div>
								</div>
							{/if}
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
													<span class="label-text-alt address-echo">
														{addressEcho(relayHexInputs[chipIdx] ?? '')}
													</span>
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
	/* Цвета — из токенов daisyUI 5 (`--color-*`): короткие имена daisyUI 4
	   (`--b1`, `--b3`, `--bc`, `--p`, `--su`, `--er`) в теме не определены, поэтому
	   `hsl(var(--b3))` и подобные значения браузер молча отбрасывал. */
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
		color: color-mix(in srgb, var(--color-base-content) 85%, transparent);
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
		border: 1px solid var(--color-base-300);
		border-radius: 0.75rem;
		background: var(--color-base-100);
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
		border: 1px solid var(--color-base-300);
		border-radius: 0.6rem;
		text-align: left;
		background: var(--color-base-100);
		transition: border-color 0.15s ease, background-color 0.15s ease, box-shadow 0.15s ease;
	}

	.section-item:hover {
		border-color: color-mix(in srgb, var(--color-primary) 50%, transparent);
	}

	.section-item.active {
		border-color: var(--color-primary);
		background: color-mix(in srgb, var(--color-primary) 10%, transparent);
		box-shadow: inset 3px 0 0 var(--color-primary);
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
		color: color-mix(in srgb, var(--color-base-content) 85%, transparent);
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
		border-top: 1px solid var(--color-base-300);
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
		color: color-mix(in srgb, var(--color-base-content) 85%, transparent);
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
		list-style: none;
		margin: 0;
		padding: 0;
	}

	.relay-chip-row {
		display: flex;
		align-items: end;
		justify-content: space-between;
		gap: 0.75rem;
		padding: 0.65rem;
		border: 1px solid var(--color-base-300);
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
		color: var(--color-success);
	}

	.chip-probe-fail {
		color: var(--color-error);
	}

	.chip-probe-unknown {
		color: color-mix(in srgb, var(--color-base-content) 65%, transparent);
		font-size: 0.72rem;
	}

	.address-echo {
		color: color-mix(in srgb, var(--color-base-content) 70%, transparent);
		font-variant-numeric: tabular-nums;
	}

	.hardware-faults-body {
		display: flex;
		flex-direction: column;
		gap: 0.25rem;
	}

	.hardware-faults-title {
		font-weight: 600;
	}

	.hardware-faults-list {
		margin: 0;
		padding-left: 1.1rem;
		list-style: disc;
		font-size: 0.85rem;
	}

	.hardware-faults-list li {
		display: flex;
		align-items: baseline;
		gap: 0.4rem;
		flex-wrap: wrap;
	}

	.hardware-faults-role {
		font-weight: 600;
	}

	.hardware-faults-addr {
		font-variant-numeric: tabular-nums;
		opacity: 0.85;
	}

	.hardware-faults-code {
		opacity: 0.9;
	}

	.hardware-faults-scan {
		font-size: 0.8rem;
		font-variant-numeric: tabular-nums;
		opacity: 0.85;
	}

	.mobile-editor-nav {
		display: flex;
		flex-direction: column;
		align-items: stretch;
		gap: 0.5rem;
		padding: 0.65rem;
		margin-bottom: 0.35rem;
		border: 1px solid var(--color-base-300);
		border-radius: 0.6rem;
		background: var(--color-base-100);
	}

	.mobile-editor-title-wrap {
		display: flex;
		flex-direction: column;
		gap: 0.15rem;
	}

	.mobile-editor-caption {
		font-size: 0.75rem;
		color: color-mix(in srgb, var(--color-base-content) 70%, transparent);
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
		color: color-mix(in srgb, var(--color-base-content) 75%, transparent);
	}

	.mobile-nav-actions {
		display: flex;
		align-items: center;
		gap: 0.35rem;
		flex-wrap: wrap;
		padding: 0.25rem;
		border-radius: 0.5rem;
		background: var(--color-base-100);
	}

	.mobile-nav-btn {
		background: var(--color-base-100);
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
