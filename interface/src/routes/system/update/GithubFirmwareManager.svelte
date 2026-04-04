<script lang="ts">
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { modals } from 'svelte-modals';
	import type { ModalComponent } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import NightlyConfirmDialog from '$lib/components/NightlyConfirmDialog.svelte';
	import Github from '~icons/tabler/brand-github';
	import CloudDown from '~icons/tabler/cloud-download';
	import FileUpload from '~icons/tabler/file-upload';
	import Cancel from '~icons/tabler/x';
	import Prerelease from '~icons/tabler/test-pipe';
	import ErrorIcon from '~icons/tabler/circle-x';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import { compareVersions } from 'compare-versions';
	import FirmwareUpdateDialog from '$lib/components/FirmwareUpdateDialog.svelte';
	import InfoDialog from '$lib/components/InfoDialog.svelte';
	import Check from '~icons/tabler/check';
	import { telemetry } from '$lib/stores/telemetry';
	import type { SystemInformation } from '$lib/types/models';

	/** Минимальная свободная heap (байт) для OTA с устройства; иначе предлагаем установку через браузер. */
	const FREE_HEAP_THRESHOLD = 60 * 1024;
	/** Эмуляция нехватки памяти: откройте страницу с ?simulateLowMemory=1 в URL. */
	function isSimulateLowMemory(): boolean {
		if (typeof window === 'undefined') return false;
		return new URLSearchParams(window.location.search).get('simulateLowMemory') === '1';
	}

	const SEMVER_REGEX = /^v?\d+\.\d+\.\d+/;
	/** Тег ночной сборки: старый "nightly" или новый v{BASE}-nightly (например v0.2.6.2-nightly). */
	function isNightly(release: { tag_name: string }) {
		const tag = release.tag_name;
		return tag === 'nightly' || tag.endsWith('-nightly');
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

	async function getSystemStatus(): Promise<SystemInformation | null> {
		try {
			const response = await fetch('/rest/systemStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			if (!response.ok) return null;
			return await response.json();
		} catch {
			return null;
		}
	}

	async function postGithubDownload(url: string) {
		try {
			await fetch('/rest/downloadUpdate', {
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

	/** Установка через браузер: скачивание в браузере и отправка на устройство как в «Загрузка обновления». */
	async function installViaBrowser(downloadUrl: string) {
		modals.close();
		telemetry.setOTAStatus({ status: 'none', progress: 0, error: '' });
		try {
			const response = await fetch(downloadUrl, { mode: 'cors' });
			if (!response.ok) throw new globalThis.Error(`HTTP ${response.status}`);
			const blob = await response.blob();
			const file = new File([blob], 'firmware.bin', { type: 'application/octet-stream' });
			const formData = new FormData();
			formData.append('file', file);
			// Запрос не ждём — прогресс и ошибки приходят по WebSocket в FirmwareUpdateDialog.
			fetch('/rest/uploadFirmware', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
				},
				body: formData
			});
			modals.open(FirmwareUpdateDialog as unknown as ModalComponent<any>, {
				title: 'Загрузка встроенного ПО'
			});
		} catch (e) {
			console.warn('Install via browser failed:', e);
			telemetry.setOTAStatus({ status: 'error', progress: 0, error: '' });
			window.open(downloadUrl, '_blank', 'noopener,noreferrer');
			setTimeout(() => window.dispatchEvent(new CustomEvent('openFirmwareFileSelect')), 300);
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

	/** Подтверждение и сразу установка через браузер (скачать → загрузить файл на устройство). */
	function confirmInstallViaBrowser(assets: any, nightly = false) {
		const url = getDownloadUrl(assets);
		if (url === '') {
			modals.open(InfoDialog as unknown as ModalComponent<any>, {
				isOpen: true,
				title: 'Подходящая прошивка не найдена',
				message:
					'Для текущего устройства не найдено соответствующей прошивки. Загрузите прошивку вручную или создайте ее из исходных текстов.',
				dismiss: { label: 'OK', icon: Check },
				onDismiss: () => modals.close()
			});
			return;
		}
		const doDownload = () => {
			modals.close();
			window.open(url, '_blank', 'noopener,noreferrer');
		};
		if (nightly) {
			modals.open(NightlyConfirmDialog as unknown as ModalComponent<any>, {
				isOpen: true,
				onConfirm: doDownload,
				onCancel: () => modals.close()
			});
		} else {
			modals.open(ConfirmDialog as unknown as ModalComponent<any>, {
				title: 'Скачать релиз',
				message:
					'Автоматически начнётся скачивание файла прошивки (.bin).<br><br>После сохранения файла перепрошить устройство можно через раздел <strong>«Загрузка обновления»</strong> — нажмите кнопку выбора файла и укажите скачанный релиз.',
				labels: {
					cancel: { label: 'Отменить', icon: Cancel },
					confirm: { label: 'Скачать и сохранить', icon: FileUpload }
				},
				onConfirm: doDownload,
				onCancel: () => modals.close()
			});
		}
	}

	function confirmGithubUpdate(assets: any, nightly = false) {
		const url = getDownloadUrl(assets);
		if (url === '') {
			modals.open(InfoDialog as unknown as ModalComponent<any>, {
				isOpen: true,
				title: 'Подходящая прошивка не найдена',
				message:
					'Для текущего устройства не найдено соответствующей прошивки. Загрузите прошивку вручную или создайте ее из исходных текстов.',
				dismiss: { label: 'OK', icon: Check },
				onDismiss: () => modals.close()
			});
			return;
		}

		const runDeviceDownload = () => {
			telemetry.setOTAStatus({ status: 'none', progress: 0, error: '' });
			postGithubDownload(url);
			modals.close();
			modals.open(FirmwareUpdateDialog as unknown as ModalComponent<any>, {
				title: 'Загрузка встроенного ПО'
			});
		};

		const onConfirmInstall = async () => {
			modals.close();
			const status = await getSystemStatus();
			const simulate = isSimulateLowMemory();
			const effectiveStatus = simulate
				? ({ free_heap: 50 * 1024 } as SystemInformation)
				: status;
			const lowMemory =
				simulate ||
				(effectiveStatus != null &&
					typeof effectiveStatus.free_heap === 'number' &&
					effectiveStatus.free_heap < FREE_HEAP_THRESHOLD);
			if (lowMemory && effectiveStatus) {
				const freeKb = Math.round((effectiveStatus.free_heap / 1024) | 0);
				modals.open(ConfirmDialog as unknown as ModalComponent<any>, {
					title: 'Мало свободной памяти на устройстве',
					message: `Свободно памяти: ${freeKb} KB${simulate ? ' (эмуляция)' : ''}. Прямая установка с интернета может завершиться ошибкой.<br><br>Рекомендуется скачать прошивку в браузере и загрузить файл на устройство (как в разделе «Загрузка обновления»).`,
					labels: {
						cancel: { label: 'Всё равно с устройства', icon: CloudDown },
						confirm: { label: 'Скачать в браузере и загрузить', icon: CloudDown }
					},
					onConfirm: () => installViaBrowser(url),
					onCancel: () => {
						modals.close();
						runDeviceDownload();
					}
				});
			} else {
				runDeviceDownload();
			}
		};

		if (nightly) {
			modals.open(NightlyConfirmDialog as unknown as ModalComponent<any>, {
				isOpen: true,
				onConfirm: onConfirmInstall,
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
				onConfirm: onConfirmInstall,
				onCancel: () => modals.close()
			});
		}
	}
</script>

<div
	class="update-card rounded-box border border-base-content/10 w-full lg:w-3/4 mx-auto flex flex-col gap-4 p-4 sm:p-6"
>
	<div class="flex items-center gap-2 text-xl font-medium">
		<Github class="h-6 w-6" />
		<span>Менеджер обновлений Github</span>
	</div>

	{#await getGithubAPI()}
		<div class="flex flex-col items-center justify-center gap-2 py-6">
			<span class="loading loading-spinner loading-lg text-primary" aria-hidden="true"></span>
		</div>
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
							<th align="center">Установить</th>
							<th align="center">Скачать релиз</th>
						</tr>
					</thead>
					<tbody>
						{#each stableReleases as release}
							<tr
								class={isCurrentVersion(release)
									? 'bg-primary text-primary-content'
									: 'h-14'}
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
											title="Установить с интернета (устройство скачивает)"
											onclick={() => confirmGithubUpdate(release.assets, false)}
										>
											<CloudDown class="text-secondary h-6 w-6" />
										</button>
									{/if}
								</td>
								<td align="center">
									<button
										class="btn btn-ghost btn-sm gap-1"
										title="Скачать файл прошивки на компьютер"
										onclick={() => confirmInstallViaBrowser(release.assets, false)}
									>
										<FileUpload class="text-secondary h-5 w-5 shrink-0" />
										<span>Скачать релиз</span>
									</button>
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
						<div class="flex flex-wrap gap-2">
							<button
								class="btn btn-warning btn-sm"
								title="Установить с интернета (устройство скачивает)"
								onclick={() => confirmGithubUpdate(nightlyRelease.assets, true)}
							>
								<CloudDown class="h-5 w-5" />
								<span>Установить nightly</span>
							</button>
							<button
								class="btn btn-outline btn-warning btn-sm"
								title="Скачать релиз"
								onclick={() => confirmInstallViaBrowser(nightlyRelease.assets, true)}
							>
								<FileUpload class="h-5 w-5" />
								<span>Скачать релиз</span>
							</button>
						</div>
					</div>
				</div>
			</details>
		{/if}
	{:catch error}
		<div class="alert alert-error shadow-lg">
			<ErrorIcon class="h-6 w-6 shrink-0" />
			<span>Пожалуйста, подключитесь к сети с доступом в Интернет, чтобы выполнить обновление встроенного ПО.</span>
		</div>
	{/await}
</div>
