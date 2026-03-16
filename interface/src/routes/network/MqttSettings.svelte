<script lang="ts">
	import { page } from '$app/state';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import Toggle from '$lib/components/Toggle.svelte';
	import InputPassword from '$lib/components/InputPassword.svelte';
	import MQTT from '~icons/tabler/topology-star-3';
	import Client from '~icons/tabler/robot';
	import type { MQTTSettings, MQTTStatus } from '$lib/types/models';

	let mqttSettings: MQTTSettings | null = $state(null);
	let mqttStatus: MQTTStatus | null = $state(null);

	let isSettingsLoading = $state(true);
	let isStatusLoading = $state(true);
	let settingsError = $state('');
	let statusError = $state('');

	let formErrors = $state({
		host: false,
		port: false,
		keep_alive: false,
		topic_length: false,
		rate_limit: false
	});

	let isFieldsDisabled = $state(true);

	async function loadMQTTSettings() {
		try {
			const response = await fetch('/rest/mqttSettings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			mqttSettings = (await response.json()) as MQTTSettings;
			isFieldsDisabled = !mqttSettings.enabled;
			settingsError = '';
		} catch (err) {
			console.error('Error loading MQTT settings:', err);
			settingsError = 'Failed to load MQTT settings.';
		} finally {
			isSettingsLoading = false;
		}
	}

	async function loadMQTTStatus() {
		try {
			const response = await fetch('/rest/mqttStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			mqttStatus = (await response.json()) as MQTTStatus;
			statusError = '';
		} catch (err) {
			console.error('Error loading MQTT status:', err);
			statusError = 'Failed to load MQTT status.';
		} finally {
			isStatusLoading = false;
		}
	}

	let statusInterval: ReturnType<typeof setInterval> | null = null;

	$effect(() => {
		if (!page.data.features.security || $user.admin) {
			loadMQTTSettings();
			loadMQTTStatus();

			if (statusInterval) {
				clearInterval(statusInterval);
			}

			statusInterval = setInterval(() => {
				loadMQTTStatus();
			}, 5000);

			return () => {
				if (statusInterval) {
					clearInterval(statusInterval);
					statusInterval = null;
				}
			};
		} else {
			isSettingsLoading = false;
			isStatusLoading = false;
		}
	});

	function handleToggleEnabled(event: Event) {
		if (!mqttSettings) return;
		const input = event.currentTarget as HTMLInputElement;
		mqttSettings.enabled = input.checked;
		isFieldsDisabled = !mqttSettings.enabled;
	}

	async function submitMQTTSettings() {
		if (!mqttSettings) return;

		let valid = true;

		// Validate Server URI (hostname or IPv4 with optional path)
		const regexExpURL =
			/(([-a-zA-Z0-9@:%_\+.~#?&//=]{2,256}\.[a-z]{2,4})|(\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b))(\/[-a-zA-Z0-9@:%_\+.~#?&//=]*)?/i;

		if (mqttSettings.enabled && !regexExpURL.test(mqttSettings.uri)) {
			valid = false;
			formErrors.host = true;
		} else {
			formErrors.host = false;
		}

		// Validate keep alive (1–600 seconds)
		const keepalive = Number(mqttSettings.keep_alive);
		if (1 <= keepalive && keepalive <= 600) {
			formErrors.keep_alive = false;
		} else {
			formErrors.keep_alive = true;
			valid = false;
		}

		// Validate publish interval (0–1000 ms)
		const ratelimit = Number(mqttSettings.message_interval_ms);
		if (0 <= ratelimit && ratelimit <= 1000) {
			formErrors.rate_limit = false;
		} else {
			formErrors.rate_limit = true;
			valid = false;
		}

		if (!valid) return;

		try {
			const response = await fetch('/rest/mqttSettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(mqttSettings)
			});

			if (response.status === 200) {
				notifications.success('MQTT settings updated.', 3000);
				mqttSettings = (await response.json()) as MQTTSettings;
			} else {
				notifications.error('User not authorized.', 3000);
			}
		} catch (err) {
			console.error('Error posting MQTT settings:', err);
			notifications.error('Failed to update MQTT settings.', 3000);
		}
	}
</script>

<div class="mqtt-settings-root">
	<div class="settings-container">
		<div class="settings-grid">
			<div class="settings-panel">
				<!-- Section 1: MQTT status -->
				<div class="settings-section">
					{#if isStatusLoading}
						<div class="loading-container flex flex-col items-center gap-2">
							<p class="loading-text">Loading MQTT status...</p>
							<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
						</div>
					{:else if statusError}
						<div class="error-container">
							<p class="error-text">{statusError}</p>
						</div>
						{:else if mqttStatus}
						<div class="flex w-full flex-col space-y-1">
							<div class="rounded-box flex items-center space-x-3 px-4 py-2">
								<div
									class="mask mask-hexagon h-auto w-10 {mqttStatus.connected === true
										? 'bg-success'
										: 'bg-error'}"
								>
									<MQTT
										class="h-auto w-full scale-75 {mqttStatus.connected === true
											? 'text-success-content'
											: 'text-error-content'}"
									/>
								</div>
								<div>
									<div class="font-bold">Status</div>
									<div class="text-sm opacity-75">
										{#if mqttStatus.connected}
											Connected
										{:else if !mqttStatus.enabled}
											MQTT Disabled
										{:else}
											{mqttStatus.last_error}
										{/if}
									</div>
								</div>
							</div>

							<div class="rounded-box flex items-center space-x-3 px-4 py-2">
								<div class="mask mask-hexagon bg-primary h-auto w-10">
									<Client class="text-primary-content h-auto w-full scale-75" />
								</div>
								<div>
									<div class="font-bold">Client ID</div>
									<div class="text-sm opacity-75">
										{mqttStatus.client_id}
									</div>
								</div>
							</div>
						</div>
					{/if}
				</div>

				<!-- Section 2: MQTT settings -->
				<div class="settings-section">
					{#if isSettingsLoading}
						<div class="loading-container flex flex-col items-center gap-2">
							<p class="loading-text">Loading MQTT settings...</p>
							<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
						</div>
					{:else if settingsError}
						<div class="error-container">
							<p class="error-text">{settingsError}</p>
						</div>
					{:else if mqttSettings}
						<div class="settings-group">
							<!-- Enable MQTT toggle -->
							<div class="settings-item mqtt-toggle-row">
								<div class="input-label-container">
									<span class="input-label">MQTT subsystem</span>
									<span class="settings-description">
										{mqttSettings.enabled ? 'Enabled' : 'Disabled'}
									</span>
								</div>
								<Toggle checked={mqttSettings.enabled} onchange={handleToggleEnabled} />
							</div>

							<!-- URI -->
							<div class="settings-item settings-item--stacked" class:read-only={isFieldsDisabled}>
								<span class="input-label">URI</span>
								<div class="input-wrapper">
									<input
										type="text"
										min="3"
										max="64"
										class="input-field"
										bind:value={mqttSettings.uri}
										id="host"
										required
										disabled={isFieldsDisabled}
									/>
								</div>
								{#if formErrors.host}
									<p class="settings-description text-error text-sm">
										Must be a valid URI (hostname or IPv4).
									</p>
								{:else}
									<p class="settings-description">Hostname or IPv4 address with optional path.</p>
								{/if}
							</div>

							<!-- Username -->
							<div class="settings-item settings-item--stacked" class:read-only={isFieldsDisabled}>
								<span class="input-label">Username</span>
								<div class="input-wrapper">
									<input
										type="text"
										class="input-field"
										bind:value={mqttSettings.username}
										id="user"
										disabled={isFieldsDisabled}
									/>
								</div>
							</div>

							<!-- Password -->
							<div class="settings-item settings-item--stacked" class:read-only={isFieldsDisabled}>
								<span class="input-label">Password</span>
								<div class="input-wrapper">
									<InputPassword bind:value={mqttSettings.password} id="pwd" />
								</div>
							</div>

							<!-- Client ID -->
							<div class="settings-item settings-item--stacked" class:read-only={isFieldsDisabled}>
								<span class="input-label">Client ID</span>
								<div class="input-wrapper">
									<input
										type="text"
										class="input-field"
										bind:value={mqttSettings.client_id}
										id="clientid"
										disabled={isFieldsDisabled}
									/>
								</div>
							</div>

							<!-- Keep Alive -->
							<div class="settings-item settings-item--stacked" class:read-only={isFieldsDisabled}>
								<span class="input-label">Keep Alive</span>
								<div class="input-wrapper">
									<input
										type="number"
										min="1"
										max="600"
										class="input-field"
										bind:value={mqttSettings.keep_alive}
										id="keepalive"
										required
										disabled={isFieldsDisabled}
									/>
								</div>
								{#if formErrors.keep_alive}
									<p class="settings-description text-error text-sm">
										Must be between 1 and 600 seconds.
									</p>
								{:else}
									<p class="settings-description">Seconds between keep-alive packets (1–600).</p>
								{/if}
							</div>

							<!-- Publish Message Interval -->
							<div class="settings-item settings-item--stacked" class:read-only={isFieldsDisabled}>
								<span class="input-label">Publish Message Interval</span>
								<div class="input-wrapper">
									<input
										type="number"
										min="0"
										max="1000"
										class="input-field"
										bind:value={mqttSettings.message_interval_ms}
										id="ratelimit"
										required
										disabled={isFieldsDisabled}
									/>
								</div>
								{#if formErrors.rate_limit}
									<p class="settings-description text-error text-sm">
										Must be between 0 and 1000 milliseconds.
									</p>
								{:else}
									<p class="settings-description">
										Milliseconds between published messages (0–1000).
									</p>
								{/if}
							</div>

							<!-- Clean Session -->
							<div class="settings-item" class:read-only={isFieldsDisabled}>
								<span class="settings-label">Clean Session</span>
								<div class="toggle-container">
									<input
										type="checkbox"
										bind:checked={mqttSettings.clean_session}
										class="checkbox checkbox-primary"
										disabled={isFieldsDisabled}
									/>
								</div>
							</div>

							<!-- Actions -->
							<div class="settings-item">
								<div class="modal-actions">
									<button class="btn btn-primary" type="button" onclick={submitMQTTSettings}>
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
	:global(.mqtt-settings-root .settings-container) {
		width: 100%;
	}

	:global(.mqtt-settings-root .settings-grid) {
		grid-template-columns: 1fr;
	}

	:global(.mqtt-settings-root .settings-panel) {
		width: 100%;
	}

	/* Убираем разделительные линии между строками настроек внутри MQTT */
	:global(.mqtt-settings-root .settings-group .settings-item) {
		border-bottom: none;
	}

	/* Вся строка с тумблером растягивается на ширину карточки,
	   текст слева, тумблер прижат вправо */
	:global(.mqtt-settings-root .mqtt-toggle-row) {
		width: 100%;
		display: flex;
		align-items: center;
		justify-content: space-between;
	}

	:global(.mqtt-settings-root .mqtt-toggle-row .input-label-container) {
		display: flex;
		align-items: center;
		gap: 0.4rem;
		margin-right: 0.75rem;
	}

	:global(.mqtt-settings-root .mqtt-toggle-row .settings-description) {
		margin: 0;
	}
</style>

