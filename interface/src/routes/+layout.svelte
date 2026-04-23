<script lang="ts">
	import type { LayoutData } from './$types';
	import { onDestroy, onMount } from 'svelte';
	import { user } from '$lib/stores/user';
	import { telemetry } from '$lib/stores/telemetry';
	import { analytics } from '$lib/stores/analytics';
	import { batteryHistory } from '$lib/stores/battery';
	import { socket } from '$lib/stores/socket';
	import type { userProfile } from '$lib/stores/user';
	import { page } from '$app/state';
	import { Modals } from 'svelte-modals';
	import Toast from '$lib/components/toasts/Toast.svelte';
	import { notifications } from '$lib/components/toasts/notifications';
	import { fade } from 'svelte/transition';
	import '../app.css';
	import '../normalize.css'
	import Menu from './menu.svelte';
	import Statusbar from './statusbar.svelte';
	import Login from './login.svelte';
	import type { Analytics } from '$lib/types/models';
	import type { RSSI } from '$lib/types/models';
	import type { Battery } from '$lib/types/models';
	import type { OTAStatus } from '$lib/types/models';

	import '$lib/styles/open-connect-main.scss';

	interface Props {
		data: LayoutData;
		children?: import('svelte').Snippet;
	}

	let { data, children }: Props = $props();

	onMount(async () => {
		if ($user.bearer_token !== '') {
			await validateUser($user);
		}
		if (!(page.data.features.security && $user.bearer_token === '')) {
			initSocket();
		}
	});

	const initSocket = () => {
		const ws_token = page.data.features.security ? '?access_token=' + $user.bearer_token : '';
		const ws_protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
		socket.init(
			`${ws_protocol}://${window.location.host}/ws/events${ws_token}`,
			page.data.features.event_use_json
		);
		addEventListeners();
	};

	onDestroy(() => {
		removeEventListeners();
	});

	const addEventListeners = () => {
		socket.on('open', handleOpen);
		socket.on('close', handleClose);
		socket.on('error', handleError);
		socket.on('rssi', handleNetworkStatus);
		socket.on('notification', handleNotification);
		if (page.data.features.analytics) socket.on('analytics', handleAnalytics);
		if (page.data.features.battery) socket.on('battery', handleBattery);
		if (page.data.features.download_firmware) socket.on('otastatus', handleOTA);
	};

	const removeEventListeners = () => {
		socket.off('analytics', handleAnalytics);
		socket.off('open', handleOpen);
		socket.off('close', handleClose);
		socket.off('rssi', handleNetworkStatus);
		socket.off('notification', handleNotification);
		socket.off('battery', handleBattery);
		socket.off('otastatus', handleOTA);
	};

	async function validateUser(userdata: userProfile) {
		try {
			const response = await fetch('/rest/verifyAuthorization', {
				method: 'GET',
				headers: {
					Authorization: 'Bearer ' + userdata.bearer_token,
					'Content-Type': 'application/json'
				}
			});
			if (response.status !== 200) {
				user.invalidate();
			}
		} catch (error) {
			console.error('Error:', error);
		}
	}

	const handleOpen = () => {
		notifications.success('Соединение с ESP32 установлено', 5000);
	};

	const handleClose = () => {
		notifications.error('Потеряно соединение с ESP32', 5000);
		telemetry.setRSSI({ rssi: 0, ssid: '' });
	};

	const handleError = (data: any) => console.error(data);

	const handleNotification = (data: any) => {
		switch (data.type) {
			case 'info':
				notifications.info(data.message, 5000);
				break;
			case 'warning':
				notifications.warning(data.message, 5000);
				break;
			case 'error':
				notifications.error(data.message, 5000);
				break;
			case 'success':
				notifications.success(data.message, 5000);
				break;
			default:
				break;
		}
	};

	const handleAnalytics = (data: Analytics) => analytics.addData(data);

	const handleNetworkStatus = (data: RSSI) => telemetry.setRSSI(data);

	const handleBattery = (data: Battery) => {
		telemetry.setBattery(data);
		batteryHistory.addData(data);
	};

	const handleOTA = (data: OTAStatus) => {
		telemetry.setOTAStatus(data);
	};

	let menuOpen = $state(false);

</script>

<svelte:head>
	<title>{page.data.title}</title>
</svelte:head>

{#if page.data.features.security && $user.bearer_token === ''}
	<Login signIn={initSocket} />
{:else}
	<div class="drawer lg:drawer-open">
		<input id="main-menu" type="checkbox" class="drawer-toggle" bind:checked={menuOpen} />
		<div class="drawer-content bg-base-100 flex h-dvh min-h-0 flex-col overflow-hidden">
			<!-- Status bar content here -->
			<Statusbar />

			<!-- Main scroll area: одна вертикальная прокрутка под navbar (узлы настроек без вложенного overflow) -->
			<div
				class="bg-base-100 flex min-h-0 flex-1 touch-pan-y flex-col overflow-y-auto overscroll-y-contain"
			>
				{@render children?.()}
			</div>
		</div>
		<!-- Side Navigation -->
		<div class="drawer-side z-30 shadow-lg">
			<label for="main-menu" class="drawer-overlay oc-drawer-overlay"></label>
			<Menu
				closeMenu={() => {
					menuOpen = false;
				}}
			/>
		</div>
	</div>
{/if}

<Modals>
	<!-- svelte-ignore a11y_click_events_have_key_events -->
	{#snippet backdrop({ close }: { close: () => void })}
		<div
			class="modal-backdrop-oc fixed inset-0 z-40 max-h-full max-w-full bg-black/20 backdrop-blur-sm max-lg:bg-black/15 max-lg:backdrop-blur-none"
			transition:fade|global
			onclick={() => close()}
			role="button"
			tabindex="0"
			aria-label="Close modal"
		></div>
	{/snippet}
</Modals>

<Toast />

<style lang="scss">
	@use "$lib/styles/base/mixins" as m;

	/* Мобильный drawer-overlay: без backdrop-blur — меньше артефактов при смене маршрута */
	:global(.oc-drawer-overlay) {
		@media (max-width: 1023px) {
			background-color: oklch(var(--bc) / 0.28) !important;
			backdrop-filter: none !important;
			-webkit-backdrop-filter: none !important;
		}
	}

	/* ===== Кнопки (shared) ===== */
	:global(.btn) {
		border: none;
		background: none;
		cursor: pointer;
		font: inherit;
		display: inline-flex;
		align-items: center;
		flex: none;
		padding: 0.5rem 1rem;
		border-radius: calc(var(--border-radius) / 2);
		font-weight: 500;
		transition: var(--transition);
		text-decoration: none;
	}

	:global(html .btn:disabled) {
		opacity: 0.5;
		cursor: not-allowed;
		transform: none;
	}

	:global(.btn:hover:not(:disabled)) {
		transform: translateY(-1px);
	}

	:global(.btn.btn-primary) {
		background: var(--blue-500);
		color: var(--white);
	}
	:global(.btn.btn-primary:hover:not(:disabled)) {
		background: var(--blue-600);
	}

	:global(.btn.btn-warning) {
		background: var(--yellow-500);
		color: var(--white);
	}
	:global(.btn.btn-warning:hover:not(:disabled)) {
		background: var(--yellow-600);
	}

	:global(.btn.btn-success) {
		background: var(--green-500);
		color: var(--white);
	}
	:global(.btn.btn-success:hover:not(:disabled)) {
		background: var(--green-600);
	}

	:global(.btn.btn-back) {
		background: var(--yellow-500);
		color: var(--white);
	}
	:global(.btn.btn-back:hover:not(:disabled)) {
		background: var(--yellow-500);
	}

	:global(.btn-icon) {
		width: 1.25rem;
		height: 1.25rem;
		margin-right: 0.5rem;
	}

	:global(.values-container) {
		display: flex;
		gap: 0.5rem;
		align-items: center;
		flex-wrap: wrap;
	}

	:global(.value-item) {
		display: flex;
		align-items: baseline;
		gap: 0.25rem;
	}

	:global(.value-text) {
		font: inherit;
		@include m.dark-theme-color;
	}

	:global(.value-unit) {
		font-size: 0.875rem;
		opacity: 0.75;
		@include m.dark-theme-color;
	}

	:global(.edit-button) {
		border: none;
		background: none;
		cursor: pointer;
		font: inherit;
		flex-shrink: 0;
		color: var(--blue-500);
		padding: 0.25rem;
		border-radius: calc(var(--border-radius) / 4);
		transition: var(--transition);
		@include m.dark-theme-color;
	}
	:global(.edit-button:hover) {
		color: var(--blue-600);
	}

	/* ===== Поля ввода и лейблы (shared) — семантика DaisyUI ===== */
	:global(.input-label) {
		color: oklch(var(--bc));
		font-size: 0.95rem;
		cursor: pointer;
	}

	:global(.input-field),
	:global(.input-cell),
	:global(.time-input) {
		width: 100%;
		padding: 0.5rem 0.75rem;
		font-size: 0.875rem;
		/* Светлая тема: делаем поля явно темнее фона карточек и с более контрастной рамкой */
		border: 1px solid oklch(var(--bc, 0.25 0 260) / 0.25);
		border-radius: var(--border-radius);
		outline: none;
		transition: var(--transition);
		background-color: oklch(var(--b3));
		color: oklch(var(--bc));
	}
	@media (prefers-color-scheme: dark) {
		:global(.input-field),
		:global(.input-cell),
		:global(.time-input) {
			background-color: var(--dark-step-inactive-bg, #5a6575);
			border-color: var(--dark-border-color, #4a5568);
		}
	}
	:global(.input-field:focus),
	:global(.input-cell:focus),
	:global(.time-input:focus) {
		border-color: oklch(var(--p));
		box-shadow: 0 0 0 3px oklch(var(--p) / 0.2);
	}

	:global(.input-wrapper) {
		display: flex;
		align-items: center;
		gap: 0.5rem;
	}
	:global(.input-wrapper .btn) {
		padding: 0.5rem;
		font-size: 1.125rem;
	}

	:global(.modal-actions) {
		display: flex;
		justify-content: flex-end;
		gap: 0.75rem;
		margin-top: 1.5rem;
	}

	:global(.info-text) {
		font-size: 0.8rem;
		color: var(--red-300);
		margin-top: 0.25rem;
		text-align: center;
		@include m.dark-theme-color;
	}
</style>
