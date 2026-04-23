<script lang="ts">
	import { onDestroy, onMount } from 'svelte';
	import { page } from '$app/state';
	import { user } from '$lib/stores/user';
	import type { NTPStatus } from '$lib/types/models';

	let deviceTime = $state<Date | null>(null);
	let serverBaseMs = 0;
	let clientBaseMs = 0;
	let tickId: number | null = null;
	let pollId: number | null = null;

	const deviceDateFmt = new Intl.DateTimeFormat('ru-RU', {
		day: '2-digit',
		month: '2-digit',
		year: 'numeric'
	});
	const deviceClockFmt = new Intl.DateTimeFormat('ru-RU', {
		hour: '2-digit',
		minute: '2-digit',
		second: '2-digit'
	});

	const deviceDateLabel = $derived(deviceTime ? deviceDateFmt.format(deviceTime) : '--.--.----');
	const deviceClockLabel = $derived(deviceTime ? deviceClockFmt.format(deviceTime) : '--:--:--');

	function updateDerivedDeviceTime() {
		if (serverBaseMs <= 0 || clientBaseMs <= 0) return;
		deviceTime = new Date(serverBaseMs + (Date.now() - clientBaseMs));
	}

	async function fetchDeviceTime() {
		if (!page.data.features.ntp) return;
		try {
			const response = await fetch('/rest/ntpStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
				}
			});
			if (!response.ok) return;
			const status = (await response.json()) as NTPStatus;
			const parsedMs = Date.parse(status.local_time);
			if (Number.isNaN(parsedMs)) return;
			serverBaseMs = parsedMs;
			clientBaseMs = Date.now();
			updateDerivedDeviceTime();
		} catch {
			// Keep last successful value on transient network/API errors.
		}
	}

	onMount(() => {
		void fetchDeviceTime();
		tickId = window.setInterval(updateDerivedDeviceTime, 1000);
		pollId = window.setInterval(() => void fetchDeviceTime(), 30000);
	});

	onDestroy(() => {
		if (tickId !== null) window.clearInterval(tickId);
		if (pollId !== null) window.clearInterval(pollId);
	});
</script>

<div
	class="flex-none px-1 text-[11px] font-semibold leading-tight tabular-nums opacity-80 lg:text-xs"
	title="Локальные дата и время устройства"
>
	<div class="text-center">{deviceDateLabel}</div>
	<div class="text-center">{deviceClockLabel}</div>
</div>
