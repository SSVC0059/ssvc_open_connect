<script lang="ts">
	import { page } from '$app/state';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import { TIME_ZONES } from './timezones';
	import { convertSeconds } from '$lib/utils/formatters';
	import Toggle from '$lib/components/Toggle.svelte';
	import NTP from '~icons/tabler/clock-check';
	import Server from '~icons/tabler/server';
	import Clock from '~icons/tabler/clock';
	import UTC from '~icons/tabler/clock-pin';
	import Stopwatch from '~icons/tabler/24-hours';
	import type { NTPSettings, NTPStatus } from '$lib/types/models';

	let ntpSettings: NTPSettings | null = $state(null);
	let ntpStatus: NTPStatus | null = $state(null);
	let isSettingsLoading = $state(true);
	let isStatusLoading = $state(true);
	let settingsError = $state('');
	let statusError = $state('');

	let formErrors = $state({
		server: false
	});

	let isFieldsDisabled = $state(true);

	async function loadNTPSettings() {
		try {
			const response = await fetch('/rest/ntpSettings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			ntpSettings = (await response.json()) as NTPSettings;
			settingsError = '';
			isFieldsDisabled = !ntpSettings.enabled;
		} catch (err) {
			console.error('Error loading NTP settings:', err);
			settingsError = 'Failed to load NTP settings.';
		} finally {
			isSettingsLoading = false;
		}
	}

	async function loadNTPStatus() {
		try {
			const response = await fetch('/rest/ntpStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			ntpStatus = (await response.json()) as NTPStatus;
			statusError = '';
		} catch (err) {
			console.error('Error loading NTP status:', err);
			statusError = 'Failed to load NTP status.';
		} finally {
			isStatusLoading = false;
		}
	}

	$effect(() => {
		if (!page.data.features.security || $user.admin) {
			loadNTPSettings();
			loadNTPStatus();
		} else {
			isSettingsLoading = false;
			isStatusLoading = false;
		}
	});

	function handleToggleEnabled(event: Event) {
		if (!ntpSettings) return;
		const input = event.currentTarget as HTMLInputElement;
		ntpSettings.enabled = input.checked;
		isFieldsDisabled = !ntpSettings.enabled;
	}

	async function submitNTPSettings() {
		if (!ntpSettings) return;

		let valid = true;

		// RegEx for IPv4 и hostname
		const regexExpIPv4 =
			/\b(?:(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])\.){3}(?:(?:2([0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9]))\b/;
		const regexExpURL =
			/[-a-zA-Z0-9@:%_\+.~#?&//=]{2,256}\.[a-z]{2,4}\b(\/[-a-zA-Z0-9@:%_\+.~#?&//=]*)?/i;

		if (
			ntpSettings.enabled &&
			!regexExpURL.test(ntpSettings.server) &&
			!regexExpIPv4.test(ntpSettings.server)
		) {
			valid = false;
			formErrors.server = true;
		} else {
			formErrors.server = false;
		}

		ntpSettings.tz_format = TIME_ZONES[ntpSettings.tz_label];

		if (!valid) return;

		try {
			const response = await fetch('/rest/ntpSettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(ntpSettings)
			});

			if (response.status === 200) {
				notifications.success('NTP settings updated.', 3000);
				ntpSettings = (await response.json()) as NTPSettings;
			} else {
				notifications.error('User not authorized.', 3000);
			}
		} catch (err) {
			console.error('Error posting NTP settings:', err);
			notifications.error('Failed to update NTP settings.', 3000);
		}
	}
</script>

<div class="ntp-settings-root">
	<div class="settings-container">
		<div class="settings-grid">
			<div class="settings-panel">
				<!-- Секция 1: статус NTP и текущее время -->
				<div class="settings-section">
					{#if isStatusLoading}
						<div class="loading-container flex flex-col items-center gap-2">
							<p class="loading-text">Loading NTP status...</p>
							<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
						</div>
					{:else if statusError}
						<div class="error-container">
							<p class="error-text">{statusError}</p>
						</div>
					{:else if ntpStatus}
						<div class="flex w-full flex-col space-y-1">
							<div class="rounded-box flex items-center space-x-3 px-4 py-2">
								<div
									class="mask mask-hexagon h-auto w-10 {ntpStatus.status === 1
										? 'bg-success'
										: 'bg-error'}"
								>
									<NTP
										class="h-auto w-full scale-75 {ntpStatus.status === 1
											? 'text-success-content'
											: 'text-error-content'}"
									/>
								</div>
								<div>
									<div class="font-bold">Status</div>
									<div class="text-sm opacity-75">
										{ntpStatus.status === 1 ? 'Active' : 'Inactive'}
									</div>
								</div>
							</div>

							<div class="rounded-box flex items-center space-x-3 px-4 py-2">
								<div class="mask mask-hexagon bg-primary h-auto w-10">
									<Server class="text-primary-content h-auto w-full scale-75" />
								</div>
								<div>
									<div class="font-bold">NTP Server</div>
									<div class="text-sm opacity-75">
										{ntpStatus.server}
									</div>
								</div>
							</div>

							<div class="rounded-box flex items-center space-x-3 px-4 py-2">
								<div class="mask mask-hexagon bg-primary h-auto w-10">
									<Clock class="text-primary-content h-auto w-full scale-75" />
								</div>
								<div>
									<div class="font-bold">Local Time</div>
									<div class="text-sm opacity-75">
										{new Intl.DateTimeFormat('en-GB', {
											dateStyle: 'long',
											timeStyle: 'long'
										}).format(new Date(ntpStatus.local_time))}
									</div>
								</div>
							</div>

							<div class="rounded-box flex items-center space-x-3 px-4 py-2">
								<div class="mask mask-hexagon bg-primary h-auto w-10">
									<UTC class="text-primary-content h-auto w-full scale-75" />
								</div>
								<div>
									<div class="font-bold">UTC Time</div>
									<div class="text-sm opacity-75">
										{new Intl.DateTimeFormat('en-GB', {
											dateStyle: 'long',
											timeStyle: 'long',
											timeZone: 'UTC'
										}).format(new Date(ntpStatus.utc_time))}
									</div>
								</div>
							</div>

							<div class="rounded-box flex items-center space-x-3 px-4 py-2">
								<div class="mask mask-hexagon bg-primary h-auto w-10">
									<Stopwatch class="text-primary-content h-auto w-full scale-75" />
								</div>
								<div>
									<div class="font-bold">Uptime</div>
									<div class="text-sm opacity-75">
										{convertSeconds(ntpStatus.uptime)}
									</div>
								</div>
							</div>
						</div>
					{/if}
				</div>

				<!-- Секция 2: настройки NTP -->
				<div class="settings-section">
					{#if isSettingsLoading}
						<div class="loading-container flex flex-col items-center gap-2">
							<p class="loading-text">Loading NTP settings...</p>
							<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
						</div>
					{:else if settingsError}
						<div class="error-container">
							<p class="error-text">{settingsError}</p>
						</div>
					{:else if ntpSettings}
						<div class="settings-group">
							<!-- Enable NTP toggle -->
							<div class="settings-item ntp-toggle-row">
								<div class="input-label-container">
									<span class="input-label">NTP subsystem</span>
									<span class="settings-description">
										{ntpSettings.enabled ? 'Enabled' : 'Disabled'}
									</span>
								</div>
								<Toggle checked={ntpSettings.enabled} onchange={handleToggleEnabled} />
							</div>

							<!-- NTP server -->
							<div class="settings-item settings-item--stacked">
								<span class="input-label">Server</span>
								<div class="input-wrapper">
									<input
										type="text"
										min="3"
										max="64"
										class="input-field"
										bind:value={ntpSettings.server}
										id="server"
										required
										disabled={isFieldsDisabled}
									/>
								</div>
								{#if formErrors.server}
									<p class="settings-description text-error text-sm">
										Please enter a valid NTP server (hostname or IPv4).
									</p>
								{:else}
									<p class="settings-description">Hostname or IPv4 address.</p>
								{/if}
							</div>

							<!-- Time zone -->
							<div class="settings-item settings-item--stacked">
								<span class="input-label">Time Zone</span>
								<div class="input-wrapper">
									<select
										class="input-field select w-full"
										bind:value={ntpSettings.tz_label}
										id="tz"
										disabled={isFieldsDisabled}
									>
										{#each Object.entries(TIME_ZONES) as [tz_label, tz_format]}
											<option value={tz_label}>{tz_label}</option>
										{/each}
									</select>
								</div>
							</div>

							<!-- Actions -->
							<div class="settings-item">
								<div class="modal-actions">
									<button class="btn btn-primary" type="button" onclick={submitNTPSettings}>
										Apply Settings
									</button>
								</div>
							</div>
						</div>
					{/if}
				</div>
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	:global(.ntp-settings-root .settings-container) {
		width: 100%;
	}

	:global(.ntp-settings-root .settings-grid) {
		grid-template-columns: 1fr;
	}

	:global(.ntp-settings-root .settings-panel) {
		width: 100%;
	}

	/* Убираем разделительные линии между строками настроек внутри NTP */
	:global(.ntp-settings-root .settings-group .settings-item) {
		border-bottom: none;
	}

	/* Вся строка с тумблером растягивается на ширину карточки,
	   текст слева, тумблер прижат вправо */
	:global(.ntp-settings-root .ntp-toggle-row) {
		width: 100%;
		display: flex;
		align-items: center;
		justify-content: space-between;
	}

	:global(.ntp-settings-root .ntp-toggle-row .input-label-container) {
		display: flex;
		align-items: center;
		gap: 0.4rem;
		margin-right: 0.75rem;
	}

	:global(.ntp-settings-root .ntp-toggle-row .settings-description) {
		margin: 0;
	}
</style>

