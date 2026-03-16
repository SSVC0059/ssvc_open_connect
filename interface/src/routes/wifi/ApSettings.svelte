<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import InputPassword from '$lib/components/InputPassword.svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import AP from '~icons/tabler/access-point';
	import MAC from '~icons/tabler/dna-2';
	import Home from '~icons/tabler/home';
	import Devices from '~icons/tabler/devices';
	import type { ApSettings, ApStatus } from '$lib/types/models';

let apSettings: ApSettings = $state({
	provision_mode: 0,
	ssid: '',
	password: '',
	channel: 1,
	max_clients: 1,
	local_ip: '',
	gateway_ip: '',
	subnet_mask: '',
	ssid_hidden: false
});
let apStatus: ApStatus = $state({
	status: 0,
	ip_address: '',
	mac_address: '',
	station_num: 0
});

	let formField: any = $state();

	async function getAPStatus() {
		try {
			const response = await fetch('/rest/apStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			apStatus = await response.json();
		} catch (error) {
			console.error('Error:', error);
		}
		return apStatus;
	}

	async function getAPSettings() {
		try {
			const response = await fetch('/rest/apSettings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			apSettings = await response.json();
		} catch (error) {
			console.error('Error:', error);
		}
		return apSettings;
	}

	const interval = setInterval(async () => {
		getAPStatus();
	}, 5000);

	onDestroy(() => clearInterval(interval));

	onMount(() => {
		if (!page.data.features.security || $user.admin) {
			getAPSettings();
		}
		getAPStatus();
	});

	let provisionMode = [
		{
			id: 0,
			text: `Always`
		},
		{
			id: 1,
			text: `When WiFi Disconnected`
		},
		{
			id: 2,
			text: `Never`
		}
	];

	let apStatusDescription = [
		{ bg_color: 'bg-success', text_color: 'text-success-content', description: 'Active' },
		{ bg_color: 'bg-error', text_color: 'text-error-content', description: 'Inactive' },
		{ bg_color: 'bg-warning', text_color: 'text-warning-content', description: 'Lingering' }
	];

	let formErrors = $state({
		ssid: false,
		channel: false,
		max_clients: false,
		local_ip: false,
		gateway_ip: false,
		subnet_mask: false
	});

	async function postAPSettings(data: ApSettings) {
		try {
			const response = await fetch('/rest/apSettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(data)
			});
			if (response.status == 200) {
				notifications.success('Access Point settings updated.', 3000);
				apSettings = await response.json();
			} else {
				notifications.error('User not authorized.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
	}

	function handleSubmitAP() {
		let valid = true;

		// Validate SSID
		if (apSettings.ssid.length < 3 || apSettings.ssid.length > 32) {
			valid = false;
			formErrors.ssid = true;
		} else {
			formErrors.ssid = false;
		}

		// Validate Channel
		let channel = Number(apSettings.channel);
		if (1 > channel || channel > 13) {
			valid = false;
			formErrors.channel = true;
		} else {
			formErrors.channel = false;
		}

		// Validate max_clients
		let maxClients = Number(apSettings.max_clients);
		if (1 > maxClients || maxClients > 8) {
			valid = false;
			formErrors.max_clients = true;
		} else {
			formErrors.max_clients = false;
		}

		// RegEx for IPv4
		const regexExp =
			/\b(?:(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])\.){3}(?:(?:2([0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9]))\b/;

		// Validate gateway IP
		if (!regexExp.test(apSettings.gateway_ip)) {
			valid = false;
			formErrors.gateway_ip = true;
		} else {
			formErrors.gateway_ip = false;
		}

		// Validate Subnet Mask
		if (!regexExp.test(apSettings.subnet_mask)) {
			valid = false;
			formErrors.subnet_mask = true;
		} else {
			formErrors.subnet_mask = false;
		}

		// Validate local IP
		if (!regexExp.test(apSettings.local_ip)) {
			valid = false;
			formErrors.local_ip = true;
		} else {
			formErrors.local_ip = false;
		}

		// Submit JSON to REST API
		if (valid) {
			postAPSettings(apSettings);
		}
	}

	function preventDefault(fn: (event: SubmitEvent) => void) {
		return function (event: SubmitEvent) {
			event.preventDefault();
			fn(event);
		};
	}
</script>

<div class="wifi-ap-root">
	<div class="settings-container">
		<div class="settings-grid">
			<div class="settings-panel">
				<!-- Секция 1: статус точки доступа -->
				<div class="settings-section">
					<div class="flex w-full flex-col space-y-1">
						<div class="rounded-box flex items-center space-x-3 px-4 py-2">
							<div
								class="mask mask-hexagon h-auto w-10 {apStatusDescription[apStatus.status].bg_color}"
							>
								<AP class="h-auto w-full scale-75 {apStatusDescription[apStatus.status].text_color}" />
							</div>
							<div>
								<div class="font-bold">Status</div>
								<div class="text-sm opacity-75">
									{apStatusDescription[apStatus.status].description}
								</div>
							</div>
						</div>

						<div class="rounded-box flex items-center space-x-3 px-4 py-2">
							<div class="mask mask-hexagon bg-primary h-auto w-10">
								<Home class="text-primary-content h-auto w-full scale-75" />
							</div>
							<div>
								<div class="font-bold">IP Address</div>
								<div class="text-sm opacity-75">
									{apStatus.ip_address}
								</div>
							</div>
						</div>

						<div class="rounded-box flex items-center space-x-3 px-4 py-2">
							<div class="mask mask-hexagon bg-primary h-auto w-10">
								<MAC class="text-primary-content h-auto w-full scale-75" />
							</div>
							<div>
								<div class="font-bold">MAC Address</div>
								<div class="text-sm opacity-75">
									{apStatus.mac_address}
								</div>
							</div>
						</div>

						<div class="rounded-box flex items-center space-x-3 px-4 py-2">
							<div class="mask mask-hexagon bg-primary h-auto w-10">
								<Devices class="text-primary-content h-auto w-full scale-75" />
							</div>
							<div>
								<div class="font-bold">AP Clients</div>
								<div class="text-sm opacity-75">
									{apStatus.station_num}
								</div>
							</div>
						</div>
					</div>
				</div>

				<!-- Секция 2: настройки точки доступа -->
				{#if !page.data.features.security || $user.admin}
					<div class="settings-section">
						{#await getAPSettings()}
							<div class="flex flex-col items-center justify-center gap-2 py-6">
								<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
							</div>
						{:then nothing}
							<div
								class="flex flex-col gap-2 p-0"
								transition:slide|local={{ duration: 300, easing: cubicOut }}
							>
								<form
									class="settings-group grid w-full grid-cols-1 content-center gap-x-4 gap-y-2 mb-4 sm:grid-cols-2"
									onsubmit={preventDefault(handleSubmitAP)}
									novalidate
									bind:this={formField}
								>
									<div class="settings-item settings-item--stacked">
										<span class="input-label">Provide Access Point ...</span>
										<div class="input-wrapper">
											<select
												class="input-field select w-full"
												id="apmode"
												bind:value={apSettings.provision_mode}
											>
												{#each provisionMode as mode}
													<option value={mode.id}>
														{mode.text}
													</option>
												{/each}
											</select>
										</div>
									</div>

									<div class="settings-item settings-item--stacked">
										<span class="input-label">SSID</span>
										<div class="input-wrapper">
											<input
												type="text"
												class="input-field input w-full invalid:border-error invalid:border-2 {formErrors.ssid
													? 'border-error border-2'
													: ''}"
												bind:value={apSettings.ssid}
												id="ssid"
												min="2"
												max="32"
												required
											/>
										</div>
										<p class="settings-description text-error text-sm {formErrors.ssid ? '' : 'hidden'}">
											SSID must be between 2 and 32 characters long
										</p>
									</div>

									<div class="settings-item settings-item--stacked">
										<span class="input-label">Password</span>
										<div class="input-wrapper">
											<InputPassword bind:value={apSettings.password} id="pwd" />
										</div>
									</div>

									<div class="settings-item settings-item--stacked">
										<span class="input-label">Preferred Channel</span>
										<div class="input-wrapper">
											<input
												type="number"
												min="1"
												max="13"
												class="input-field input w-full invalid:border-error invalid:border-2 {formErrors.channel
													? 'border-error border-2'
													: ''}"
												bind:value={apSettings.channel}
												id="channel"
												required
											/>
										</div>
										<p
											class="settings-description text-error text-sm {formErrors.channel ? '' : 'hidden'}"
										>
											Must be channel 1 to 13
										</p>
									</div>

									<div class="settings-item settings-item--stacked">
										<span class="input-label">Max Clients</span>
										<div class="input-wrapper">
											<input
												type="number"
												min="1"
												max="8"
												class="input-field input w-full invalid:border-error invalid:border-2 {formErrors.max_clients
													? 'border-error border-2'
													: ''}"
												bind:value={apSettings.max_clients}
												id="clients"
												required
											/>
										</div>
										<p
											class="settings-description text-error text-sm {formErrors.max_clients ? '' : 'hidden'}"
										>
											Maximum 8 clients allowed
										</p>
									</div>

									<div class="settings-item settings-item--stacked">
										<span class="input-label">Local IP</span>
										<div class="input-wrapper">
											<input
												type="text"
												class="input-field input w-full {formErrors.local_ip ? 'border-error border-2' : ''}"
												minlength="7"
												maxlength="15"
												size="15"
												bind:value={apSettings.local_ip}
												id="localIP"
												required
											/>
										</div>
										<p
											class="settings-description text-error text-sm {formErrors.local_ip ? '' : 'hidden'}"
										>
											Must be a valid IPv4 address
										</p>
									</div>

									<div class="settings-item settings-item--stacked">
										<span class="input-label">Gateway IP</span>
										<div class="input-wrapper">
											<input
												type="text"
												class="input-field input w-full {formErrors.gateway_ip
													? 'border-error border-2'
													: ''}"
												minlength="7"
												maxlength="15"
												size="15"
												bind:value={apSettings.gateway_ip}
												id="gateway"
												required
											/>
										</div>
										<p
											class="settings-description text-error text-sm {formErrors.gateway_ip ? '' : 'hidden'}"
										>
											Must be a valid IPv4 address
										</p>
									</div>

									<div class="settings-item settings-item--stacked">
										<span class="input-label">Subnet Mask</span>
										<div class="input-wrapper">
											<input
												type="text"
												class="input-field input w-full {formErrors.subnet_mask
													? 'border-error border-2'
													: ''}"
												minlength="7"
												maxlength="15"
												size="15"
												bind:value={apSettings.subnet_mask}
												id="subnet"
												required
											/>
										</div>
										<p
											class="settings-description text-error text-sm {formErrors.subnet_mask ? '' : 'hidden'}"
										>
											Must be a valid IPv4 address
										</p>
									</div>

									<div class="settings-item">
										<label class="label my-auto cursor-pointer justify-start gap-4">
											<input
												type="checkbox"
												bind:checked={apSettings.ssid_hidden}
												class="checkbox checkbox-primary"
											/>
											<span class="">Hide SSID</span>
										</label>
									</div>

									<div class="settings-item">
										<div class="modal-actions">
											<button class="btn btn-primary" type="submit">Apply Settings</button>
										</div>
									</div>
								</form>
							</div>
						{/await}
					</div>
				{/if}
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	:global(.wifi-ap-root .settings-container) {
		width: 100%;
	}

	:global(.wifi-ap-root .settings-grid) {
		grid-template-columns: 1fr;
	}

	:global(.wifi-ap-root .settings-panel) {
		width: 100%;
	}
</style>

