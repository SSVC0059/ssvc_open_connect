<script lang="ts">
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { modals } from 'svelte-modals';
	import type { ModalComponent } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import NightlyConfirmDialog from '$lib/components/NightlyConfirmDialog.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Github from '~icons/tabler/brand-github';
	import CloudDown from '~icons/tabler/cloud-download';
	import Cancel from '~icons/tabler/x';
	import Prerelease from '~icons/tabler/test-pipe';
	import Error from '~icons/tabler/circle-x';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import { compareVersions } from 'compare-versions';
	import FirmwareUpdateDialog from '$lib/components/FirmwareUpdateDialog.svelte';
	import InfoDialog from '$lib/components/InfoDialog.svelte';
	import Check from '~icons/tabler/check';
	import { telemetry } from '$lib/stores/telemetry';

	const NIGHTLY_TAG = 'nightly';
	const SEMVER_REGEX = /^v?\d+\.\d+\.\d+/;

	function isNightly(release: { tag_name: string }) {
		return release.tag_name === NIGHTLY_TAG;
	}

	function isCurrentVersion(release: { tag_name: string }) {
		if (isNightly(release)) return false;
		if (!SEMVER_REGEX.test(release.tag_name)) return false;
		try {
			return compareVersions(page.data.features.firmware_version, release.tag_name) === 0;
		} catch {
			return false;
		}
	}

	function canInstall(release: { tag_name: string }) {
		return !isCurrentVersion(release);
	}

	async function getGithubAPI() {
		try {
			const githubResponse = await fetch(
				'https://api.github.com/repos/' + page.data.github + '/releases',
				{
					method: 'GET',
					headers: {
						accept: 'application/vnd.github+json',
						'X-GitHub-Api-Version': '2022-11-28'
					}
				}
			);
			const results = await githubResponse.json();
			return results;
		} catch (error) {
			console.warn(error);
		}
		return;
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

	function getDownloadUrl(assets: any): string {
		for (let i = 0; i < assets.length; i++) {
			if (
				assets[i].name.includes('.bin') &&
				assets[i].name.includes(page.data.features.firmware_built_target)
			) {
				return assets[i].browser_download_url;
			}
		}
		return '';
	}

	function confirmGithubUpdate(assets: any, nightly = false) {
		const url = getDownloadUrl(assets);
		if (url === '') {
			modals.open(InfoDialog as unknown as ModalComponent<any>, {
				title: 'Подходящая прошивка не найдена',
				message:
					'Для текущего устройства не найдено соответствующей прошивки. Загрузите прошивку вручную или создайте ее из исходных текстов.',
				dismiss: { label: 'OK', icon: Check },
				onDismiss: () => modals.close()
			});
			return;
		}

		const doInstall = () => {
			telemetry.setOTAStatus({ status: 'none', progress: 0, error: '' });
			postGithubDownload(url);
			modals.close();
			modals.open(FirmwareUpdateDialog as unknown as ModalComponent<any>, {
				title: 'Загрузка встроенного ПО'
			});
		};

		if (nightly) {
			modals.open(NightlyConfirmDialog as unknown as ModalComponent<any>, {
				isOpen: true,
				onConfirm: doInstall,
				onCancel: () => modals.close()
			});
		} else {
			modals.open(ConfirmDialog as unknown as ModalComponent<any>, {
				title: 'Подтвердите установку новой прошивки на устройство',
				message: 'Вы уверены, что хотите перезаписать существующую прошивку новой?',
				labels: {
					cancel: { label: 'Отменить', icon: Cancel },
					confirm: { label: 'Обновление', icon: CloudDown }
				},
				onConfirm: doInstall
			});
		}
	}
</script>

<SettingsCard collapsible={false}>
	{#snippet icon()}
		<Github class="lex-shrink-0 mr-2 h-6 w-6 self-end rounded-full" />
	{/snippet}
	{#snippet title()}
		<span>Менеджер обновлений Github</span>
	{/snippet}
	{#await getGithubAPI()}
		<Spinner />
	{:then githubReleases}
		{@const stableReleases = githubReleases.filter((r: { tag_name: string }) => !isNightly(r))}
		{@const nightlyRelease = githubReleases.find((r: { tag_name: string }) => isNightly(r))}
		<div class="alert alert-info">
			<div>
				<span class="font-bold">Текущая версия:</span>
				v{page.data.features.firmware_version}
			</div>
		</div>
		<div class="relative w-full overflow-visible">
			<div class="overflow-x-auto" transition:slide|local={{ duration: 300, easing: cubicOut }}>
				<table class="table w-full table-auto">
					<thead>
						<tr class="font-bold">
							<th align="left">Release</th>
							<th align="center" class="hidden sm:block">Release Date</th>
							<th align="center">Exp.</th>
							<th align="center">Install</th>
						</tr>
					</thead>
					<tbody>
						{#each stableReleases as release}
							<tr
								class={isCurrentVersion(release)
									? 'bg-primary text-primary-content'
									: 'bg-base-100 h-14'}
							>
								<td align="left" class="text-base font-semibold">
									<a
										href={release.html_url}
										class="link link-hover"
										target="_blank"
										rel="noopener noreferrer">{release.name}</a
									></td
								>
								<td align="center" class="hidden min-h-full align-middl sm:block">
									<div class="my-2">
										{new Intl.DateTimeFormat('en-GB', {
											dateStyle: 'medium'
										}).format(new Date(release.published_at))}
									</div>
								</td>
								<td align="center">
									{#if release.prerelease}
										<Prerelease class="text-accent h-5 w-5" />
									{/if}
								</td>
								<td align="center">
									{#if canInstall(release)}
										<button
											class="btn btn-ghost btn-sm"
											onclick={() => confirmGithubUpdate(release.assets, false)}
										>
											<CloudDown class="text-secondary h-6 w-6" />
										</button>
									{/if}
								</td>
							</tr>
						{/each}
					</tbody>
				</table>
			</div>
		</div>

		{#if nightlyRelease}
			<details class="collapse collapse-arrow mt-4 border border-warning bg-warning/10">
				<summary class="collapse-title flex items-center gap-2 font-medium">
					<AlertTriangle class="text-warning h-5 w-5" />
					<span>Экспериментальная ночная сборка (nightly)</span>
				</summary>
				<div class="collapse-content">
					<div class="alert alert-warning mb-3">
						<p class="text-sm">
							Ночная сборка обновляется ежедневно и может быть нестабильной. Не рекомендуется для обычных пользователей.
						</p>
					</div>
					<div class="flex items-center justify-between rounded-lg bg-base-200 p-3">
						<div>
							<a
								href={nightlyRelease.html_url}
								class="link link-hover font-semibold"
								target="_blank"
								rel="noopener noreferrer"
							>
								{nightlyRelease.name}
							</a>
							<span class="text-base-content/70 ml-2 text-sm">
								{new Intl.DateTimeFormat('en-GB', { dateStyle: 'medium' }).format(
									new Date(nightlyRelease.published_at)
								)}
							</span>
						</div>
						<button
							class="btn btn-warning btn-sm"
							onclick={() => confirmGithubUpdate(nightlyRelease.assets, true)}
						>
							<CloudDown class="h-5 w-5" />
							<span>Установить nightly</span>
						</button>
					</div>
				</div>
			</details>
		{/if}
	{:catch error}
		<div class="alert alert-error shadow-lg">
			<Error class="h-6 w-6 shrink-0" />
			<span>Пожалуйста, подключитесь к сети с доступом в Интернет, чтобы выполнить обновление встроенного ПО.</span>
		</div>
	{/await}
</SettingsCard>
