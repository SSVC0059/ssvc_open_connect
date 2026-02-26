<script lang="ts">
	import { onDestroy, onMount } from 'svelte';
	import { modals } from 'svelte-modals';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import CPU from '~icons/tabler/cpu';
	import CPP from '~icons/tabler/binary';
	import Power from '~icons/tabler/reload';
	import Sleep from '~icons/tabler/zzz';
	import FactoryReset from '~icons/tabler/refresh-dot';
	import Speed from '~icons/tabler/activity';
	import Flash from '~icons/tabler/device-sd-card';
	import Pyramid from '~icons/tabler/pyramid';
	import Sketch from '~icons/tabler/chart-pie';
	import Folder from '~icons/tabler/folder';
	import Heap from '~icons/tabler/box-model';
	import Cancel from '~icons/tabler/x';
	import Temperature from '~icons/tabler/temperature';
	import Health from '~icons/tabler/stethoscope';
	import Stopwatch from '~icons/tabler/24-hours';
	import SDK from '~icons/tabler/sdk';
	import type { SystemInformation, Analytics } from '$lib/types/models';
	import { socket } from '$lib/stores/socket';

let systemInformation: SystemInformation = $state({
	esp_platform: '',
	firmware_version: '',
	cpu_freq_mhz: 0,
	cpu_type: '',
	cpu_rev: 0,
	cpu_cores: 0,
	sketch_size: 0,
	free_sketch_space: 0,
	sdk_version: '',
	arduino_version: '',
	flash_chip_size: 0,
	flash_chip_speed: 0,
	cpu_reset_reason: '',
	max_alloc_heap: 0,
	psram_size: 0,
	free_psram: 0,
	used_psram: 0,
	free_heap: 0,
	used_heap: 0,
	total_heap: 0,
	min_free_heap: 0,
	core_temp: 0,
	fs_total: 0,
	fs_used: 0,
	uptime: 0
});

	async function getSystemStatus() {
		try {
			const response = await fetch('/rest/systemStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			systemInformation = await response.json();
		} catch (error) {
			console.log('Error:', error);
		}
		return systemInformation;
	}

	onMount(() => socket.on('analytics', handleSystemData));

	onDestroy(() => socket.off('analytics', handleSystemData));

	const handleSystemData = (data: Analytics) =>
		(systemInformation = { ...systemInformation, ...data });

	async function postRestart() {
		const response = await fetch('/rest/restart', {
			method: 'POST',
			headers: {
				Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
			}
		});
	}

	function confirmRestart() {
		modals.open(ConfirmDialog as any, {
			title: 'Подтверждение перезагрузки',
			message: 'Вы действительно хотите перезагрузить устройство?',
			labels: {
				cancel: { label: 'Отмена', icon: Cancel },
				confirm: { label: 'Перезапуск', icon: Power }
			},
			onConfirm: () => {
				modals.close();
				postRestart();
			}
		});
	}

	async function postFactoryReset() {
		const response = await fetch('/rest/factoryReset', {
			method: 'POST',
			headers: {
				Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
			}
		});
	}

	function confirmReset() {
		modals.open(ConfirmDialog as any, {
			title: 'Подтверждение полного сброса',
			message: 'Вы действительно хотите выполнить полный сброс?',
			labels: {
				cancel: { label: 'Отмена', icon: Cancel },
				confirm: { label: 'Полный сброс', icon: FactoryReset }
			},
			onConfirm: () => {
				modals.close();
				postFactoryReset();
			}
		});
	}

	async function postSleep() {
		const response = await fetch('/rest/sleep', {
			method: 'POST',
			headers: {
				Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
			}
		});
	}

	function confirmSleep() {
		modals.open(ConfirmDialog as any, {
			title: 'Confirm Going to Sleep',
			message: 'Are you sure you want to put the device into sleep?',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Sleep', icon: Sleep }
			},
			onConfirm: () => {
				modals.close();
				postSleep();
			}
		});
	}

	function convertSeconds(seconds: number) {
		let minutes = Math.floor(seconds / 60);
		let hours = Math.floor(minutes / 60);
		let days = Math.floor(hours / 24);

		hours = hours % 24;
		minutes = minutes % 60;
		seconds = seconds % 60;

		let result = '';
		if (days > 0) {
			result += days + ' day' + (days > 1 ? 's' : '') + ' ';
		}
		if (hours > 0) {
			result += hours + ' hour' + (hours > 1 ? 's' : '') + ' ';
		}
		if (minutes > 0) {
			result += minutes + ' minute' + (minutes > 1 ? 's' : '') + ' ';
		}
		result += seconds + ' second' + (seconds > 1 ? 's' : '');

		return result;
	}

	type StatusRow = { icon: typeof CPU; name: string; value: string };
	const statusRows = $derived.by((): StatusRow[] => {
		const si = systemInformation;
		const rows: StatusRow[] = [
			{ icon: Stopwatch, name: 'Uptime', value: convertSeconds(si.uptime) },
			{
				icon: Heap,
				name: 'Memory',
				value: si.total_heap
					? `${(((si.total_heap - si.free_heap) / si.total_heap) * 100).toFixed(1)} % of ${Math.round(si.total_heap / 1000).toLocaleString('en-US')} KB (${Math.round(si.free_heap / 1000).toLocaleString('en-US')} KB free, Max alloc ${Math.round(si.max_alloc_heap / 1000).toLocaleString('en-US')} KB)`
					: '—'
			},
			...(si.psram_size
				? [
						{
							icon: Pyramid,
							name: 'PSRAM',
							value: `${((si.used_psram / si.psram_size) * 100).toFixed(1)} % of ${Math.round(si.psram_size / 1000).toLocaleString('en-US')} KB (${Math.round(si.free_psram / 1000).toLocaleString('en-US')} KB free)`
						} as StatusRow
					]
				: []),
			{
				icon: Folder,
				name: 'File System',
				value: si.fs_total
					? `${((si.fs_used / si.fs_total) * 100).toFixed(1)} % of ${Math.round(si.fs_total / 1000).toLocaleString('en-US')} KB (${Math.round((si.fs_total - si.fs_used) / 1000).toLocaleString('en-US')} KB free)`
					: '—'
			},
			{
				icon: Temperature,
				name: 'Core Temperature',
				value: si.core_temp === 53.33 ? 'NaN' : `${si.core_temp.toFixed(2)} °C`
			},
			{ icon: SDK, name: 'SDK Version', value: si.sdk_version },
			{
				icon: Sketch,
				name: 'Sketch Size',
				value: `${Math.round(si.sketch_size / 1000).toLocaleString('en-US')} KB`
			},
			{
				icon: Flash,
				name: 'Flash Chip',
				value: `${Math.round(si.flash_chip_size / 1000000).toLocaleString('en-US')} MB @ ${Math.round(si.flash_chip_speed / 1000000).toLocaleString('en-US')} MHz`
			},
			{
				icon: CPU,
				name: 'CPU',
				value: `${si.cpu_type} Rev ${si.cpu_rev} (${si.cpu_cores} cores) @ ${si.cpu_freq_mhz.toLocaleString('en-US')} MHz`
			},
			{ icon: CPP, name: 'Arduino', value: si.arduino_version },
			{ icon: Speed, name: 'Sketch', value: si.firmware_version }
		];
		return rows;
	});
</script>

<div class="system-status-root">
	<div class="settings-container">
		<div class="settings-grid">
			<div class="settings-panel">
				<div class="settings-section">
					<div class="status-content">
						{#await getSystemStatus()}
							<div class="flex flex-col items-center justify-center gap-2 py-6">
								<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
							</div>
						{:then nothing}
							<div
								class="status-table-wrapper overflow-x-auto rounded-box border border-base-content/5 bg-base-100"
								transition:slide|local={{ duration: 300, easing: cubicOut }}
							>
								<table class="table">
									<tbody>
										{#each statusRows as row (row.name)}
											{@const Icon = row.icon}
											<tr class="active">
												<td class="w-12">
													<div class="mask mask-hexagon bg-primary h-10 w-10 flex items-center justify-center">
														<Icon class="text-primary-content h-5 w-5" />
													</div>
												</td>
												<td class="font-medium">{row.name}</td>
												<td class="text-sm opacity-90">{row.value}</td>
											</tr>
										{/each}
									</tbody>
								</table>
							</div>

							{#if !page.data.features.security || $user.admin}
								<div class="status-actions flex flex-wrap justify-center gap-2">
									<button class="btn btn-warning" type="button" onclick={confirmRestart}>
										<Power class="h-5 w-5" />
										<span>Restart</span>
									</button>
									<button class="btn btn-error btn-factory-reset" type="button" onclick={confirmReset}>
										<FactoryReset class="h-5 w-5" />
										<span>Factory reset</span>
									</button>
									<button class="btn btn-secondary btn-status-success" type="button" onclick={confirmSleep}>
										<Sleep class="h-5 w-5" />
										<span>Sleep</span>
									</button>
								</div>
							{/if}
						{/await}
					</div>
				</div>
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	:global(.system-status-root .settings-container) {
		width: 100%;
	}

	:global(.system-status-root .settings-grid) {
		grid-template-columns: 1fr;
	}

	:global(.system-status-root .settings-panel) {
		width: 100%;
	}

	.status-content {
		width: 100%;
		display: flex;
		flex-direction: column;
		align-items: center;
	}

	.status-table-wrapper {
		width: 100%;
		@media (min-width: 1024px) {
			width: 60%;
		}
	}

	.status-actions {
		margin-top: 1rem;
	}

	.btn-factory-reset {
		background-color: var(--color-error);
	}

	.btn-status-success {
		background-color: var(--color-success);
	}
</style>

