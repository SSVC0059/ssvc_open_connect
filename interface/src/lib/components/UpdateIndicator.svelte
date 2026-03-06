<script lang="ts">
	import { page } from '$app/state';
	import { modals } from 'svelte-modals';
	import type { ModalComponent } from 'svelte-modals';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import Firmware from '~icons/tabler/refresh-alert';
	import Cancel from '~icons/tabler/x';
	import CloudDown from '~icons/tabler/cloud-download';
	import FirmwareUpdateDialog from '$lib/components/FirmwareUpdateDialog.svelte';
	import { compareVersions } from 'compare-versions';
	import { onMount } from 'svelte';

	interface Props {
		update?: boolean;
	}

	let { update = $bindable(false) }: Props = $props();

	let firmwareVersion: string = $state('');
	let firmwareDownloadLink: string;

	/** Проверяет, что тег — семантическая версия (v1.2.3 или 1.2.3), не nightly. */
	function isStableTag(tag: string): boolean {
		return tag !== 'nightly' && /^v?\d+\.\d+\.\d+/.test(tag);
	}

	async function getGithubAPI() {
		const githubUrl = `https://api.github.com/repos/${page.data.github}/releases`;
		try {
			const response = await fetch(githubUrl, {
				method: 'GET',
				headers: {
					accept: 'application/vnd.github+json',
					'X-GitHub-Api-Version': '2022-11-28'
				}
			});
			if (response.status !== 200) {
				throw new Error(`Failed to fetch releases from ${githubUrl}`);
			}
			const releases: { tag_name: string; assets: { name: string; browser_download_url: string }[] }[] =
				await response.json();

			update = false;
			firmwareVersion = '';

			// Только стабильные релизы (v*), без nightly; сортировка по версии, новый первый
			const stableReleases = releases
				.filter((r) => isStableTag(r.tag_name))
				.sort((a, b) => compareVersions(b.tag_name, a.tag_name));

			const latestStable = stableReleases[0];
			if (!latestStable || compareVersions(latestStable.tag_name, page.data.features.firmware_version) <= 0) {
				return;
			}

			for (let i = 0; i < latestStable.assets.length; i++) {
				if (
					latestStable.assets[i].name.includes('.bin') &&
					latestStable.assets[i].name.includes(page.data.features.firmware_built_target)
				) {
					update = true;
					firmwareVersion = latestStable.tag_name;
					firmwareDownloadLink = latestStable.assets[i].browser_download_url;
					notifications.info('Firmware update available.', 5000);
					break;
				}
			}
		} catch (error) {
			console.warn(error);
		}
	}

	async function postGithubDownload(url: string) {
		try {
			const apiResponse = await fetch('/rest/downloadUpdate', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify({ download_url: url })
			});
		} catch (error) {
			console.error('Error:', error);
		}
	}

	onMount(() => {
		if (page.data.features.download_firmware && (!page.data.features.security || $user.admin)) {
			getGithubAPI();
			const interval = setInterval(
				async () => {
					getGithubAPI();
				},
				60 * 60 * 1000
			); // once per hour
		}
	});

	function confirmGithubUpdate(url: string) {
		modals.open(ConfirmDialog as unknown as ModalComponent<any>, {
			title: 'Confirm flashing new firmware to the device',
			message: 'Are you sure you want to overwrite the existing firmware with a new one?',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Update', icon: CloudDown }
			},
			onConfirm: () => {
				postGithubDownload(url);
				modals.open(FirmwareUpdateDialog, {
					title: 'Загрузка прошивки'
				});
			}
		});
	}
</script>

{#if update}
	<button
		class="btn btn-square btn-ghost h-9 w-9"
		onclick={() => confirmGithubUpdate(firmwareDownloadLink)}
	>
		<span
			class="indicator-item indicator-top indicator-center badge badge-info badge-xs top-2 scale-75 lg:top-1"
			>{firmwareVersion}</span
		>
		<Firmware class="h-7 w-7" />
	</button>
{/if}
