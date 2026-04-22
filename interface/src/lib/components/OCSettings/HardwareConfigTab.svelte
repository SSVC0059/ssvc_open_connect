<svelte:options runes />

<script lang="ts">
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

	async function load() {
		isLoading = true;
		loadError = '';
		try {
			cfg = await fetchHardwareConfig();
			if (cfg && (!cfg.relayPcf8574Addresses || cfg.relayPcf8574Addresses.length === 0)) {
				cfg.relayPcf8574Addresses = [0x24];
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

	async function save() {
		if (!cfg || disabled) return;
		isSaving = true;
		saveError = '';
		try {
			const next = await putHardwareConfig({
				pressureSensorEnabled: cfg.pressureSensorEnabled,
				bmp581I2cAddress: Number(cfg.bmp581I2cAddress),
				userRelayPcfEnabled: cfg.userRelayPcfEnabled,
				relayPcf8574Addresses: cfg.relayPcf8574Addresses.map((a) => Number(a))
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
		// svelte-modals typings vs Svelte 5 components (same pattern as SystemStatus.svelte)
		// @ts-expect-error ModalComponent mismatch
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

	const MAX_RELAY_CHIPS = 8;

	function addRelayChip() {
		if (!cfg || cfg.relayPcf8574Addresses.length >= MAX_RELAY_CHIPS) return;
		cfg.relayPcf8574Addresses = [...cfg.relayPcf8574Addresses, 0x24];
	}

	function removeRelayChip(idx: number) {
		if (!cfg || cfg.relayPcf8574Addresses.length <= 1) return;
		cfg.relayPcf8574Addresses = cfg.relayPcf8574Addresses.filter((_, i) => i !== idx);
	}
</script>

<div class="hardware-config settings-container">
	{#if isLoading}
		<div class="flex flex-col items-center gap-2 py-8">
			<p class="text-base-content/80">Загрузка конфигурации оборудования…</p>
			<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
		</div>
	{:else if loadError}
		<p class="text-error">{loadError}</p>
		<button type="button" class="btn btn-outline btn-sm mt-2" onclick={() => load()}>Повторить</button>
	{:else if cfg}
		{#if showRebootBanner}
			<div class="alert alert-warning mb-4 shadow-sm" role="status">
				<span>
					Изменения применятся после перезагрузки. Можно перезагрузить сейчас или позже.
				</span>
				<button type="button" class="btn btn-sm btn-neutral" onclick={openRestartModal}>
					Перезагрузить
				</button>
			</div>
		{/if}

		<div class="settings-panel flex flex-col gap-6">
			<section class="flex flex-col gap-3 rounded-box border border-base-300/80 bg-base-100 p-4 shadow-sm">
				<h3 class="text-lg font-semibold">Датчик давления (BMP581)</h3>
				<label class="flex cursor-pointer items-center gap-3 {!disabled ? '' : 'opacity-60'}">
					<input
						type="checkbox"
						class="toggle toggle-primary"
						bind:checked={cfg.pressureSensorEnabled}
						disabled={disabled}
					/>
					<span>Включить модуль давления на шине I²C</span>
				</label>
				<div class="flex flex-wrap items-end gap-4">
					<label class="form-control w-full max-w-xs">
						<span class="label-text">Адрес I²C BMP581 (7-bit, десятичный)</span>
						<input
							type="number"
							min={8}
							max={119}
							class="input input-bordered w-full"
							bind:value={cfg.bmp581I2cAddress}
							disabled={disabled}
						/>
						<span class="label-text-alt text-base-content/60">
							Часто 0x46 → 70, 0x47 → 71
						</span>
					</label>
					{#if cfg.pressureSensorEnabled && cfg.bmp581ProbeOk !== undefined}
						<div
							class="flex h-12 w-12 shrink-0 items-center justify-center self-end rounded-lg border border-base-300/80 bg-base-200/50"
							title={cfg.bmp581ProbeOk
								? 'Датчик отвечает на шине I²C'
								: 'Нет ответа на адресе — проверьте подключение'}
							aria-label={cfg.bmp581ProbeOk ? 'Датчик отвечает на I²C' : 'Нет ответа на I²C'}
						>
							{#if cfg.bmp581ProbeOk}
								<CircleCheck class="h-7 w-7 text-success" />
							{:else}
								<CircleX class="h-7 w-7 text-error" />
							{/if}
						</div>
					{/if}
				</div>
			</section>

			<section
				class="flex flex-col gap-3 rounded-box border border-base-300/80 bg-base-100 p-4 shadow-sm {!relayLocked
					? ''
					: 'opacity-70'}"
			>
				<h3 class="text-lg font-semibold">Реле / PCF8574</h3>
				{#if relayLocked}
					<p class="text-sm text-base-content/70">
						В этой сборке прошивки расширитель реле по I²C не используется — поля зарезервированы.
					</p>
				{/if}
				<label class="flex cursor-pointer items-center gap-3">
					<input
						type="checkbox"
						class="toggle toggle-primary"
						bind:checked={cfg.userRelayPcfEnabled}
						disabled={disabled || relayLocked}
					/>
					<span>Реле на PCF8574</span>
				</label>
				<p class="text-sm text-base-content/70">
					Порядок адресов задаёт порядок чипов; первый — линии системной тревоги AlarmManager (биты 0–1).
				</p>
				<ul class="flex flex-col gap-3">
					{#each cfg.relayPcf8574Addresses as _, chipIdx (chipIdx)}
						<li class="flex flex-wrap items-end gap-3 rounded-lg border border-base-300/60 bg-base-200/40 p-3">
							<div class="flex max-w-xs flex-1 flex-col gap-1">
								{#if cfg.relayPcf8574ProbeOk != null && cfg.relayPcf8574ProbeOk[chipIdx] !== undefined}
									<div
										class="flex h-6 items-center justify-center"
										title={cfg.relayPcf8574ProbeOk[chipIdx]
											? 'Чип отвечает на I²C'
											: 'Нет ответа на адресе'}
										aria-label={cfg.relayPcf8574ProbeOk[chipIdx]
											? 'Чип отвечает на I²C'
											: 'Нет ответа на I²C'}
									>
										{#if cfg.relayPcf8574ProbeOk[chipIdx]}
											<CircleCheck class="h-6 w-6 text-success" />
										{:else}
											<CircleX class="h-6 w-6 text-error" />
										{/if}
									</div>
								{:else}
									<div class="h-6" aria-hidden="true"></div>
								{/if}
								<label class="form-control w-full">
									<span class="label-text">Адрес PCF8574, чип {chipIdx + 1} (7-bit)</span>
									<input
										type="number"
										min={8}
										max={119}
										class="input input-bordered w-full"
										bind:value={cfg.relayPcf8574Addresses[chipIdx]}
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
				<div class="flex flex-wrap gap-2">
					<button
						type="button"
						class="btn btn-outline btn-sm"
						disabled={disabled || relayLocked || cfg.relayPcf8574Addresses.length >= MAX_RELAY_CHIPS}
						onclick={() => addRelayChip()}
					>
						Добавить чип
					</button>
					{#if cfg.relayHardwareOk === false}
						<span class="text-sm text-warning">
							Не все реле-модули отвечают на I²C (после сохранения проверьте проводку и перезагрузку).
						</span>
					{/if}
				</div>
			</section>

			{#if saveError}
				<p class="text-error">{saveError}</p>
			{/if}

			<div class="flex flex-wrap items-center gap-3">
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
				{#if showRebootBanner}
					<button type="button" class="btn btn-outline" onclick={openRestartModal}>
						Перезагрузить сейчас
					</button>
				{/if}
			</div>
		</div>
	{/if}
</div>

<style lang="scss">
	.hardware-config {
		width: 100%;
	}
</style>
