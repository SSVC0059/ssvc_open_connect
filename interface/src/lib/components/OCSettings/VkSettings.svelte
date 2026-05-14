<!-- VK community messenger settings (OpenConnect) -->
<script lang="ts">
	import { user } from '$lib/stores/user';
	import { getVkSettings, saveVkSettings } from '$lib/api/ssvcApi';
	import Eye from '~icons/tabler/eye';
	import Eye_off from '~icons/tabler/eye-off';
	import { page } from '$app/stores';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import Cancel from '~icons/tabler/x';
	import Power from '~icons/tabler/reload';
	import { notifications } from '$lib/components/toasts/notifications';
	import Toggle from '$lib/components/Toggle.svelte';
	import type { VkConfig } from '$lib/types/ssvc';

	let access_token = $state('');
	let api_version = $state('5.199');
	let group_id = $state('');
	let live_enabled = $state(true);
	let live_peer_id = $state('');
	let alerts_enabled = $state(false);
	let alerts_peer_id = $state('');
	let summary_enabled = $state(false);
	let summary_peer_id = $state('');
	let wall_post_enabled = $state(false);
	let keyboard_base_url = $state('');

	let isLoading = $state(true);
	let isSaving = $state(false);
	let error = $state('');
	let showToken = $state(false);
	let { disabled = false, onToggle } = $props();

	const loadSettings = async () => {
		try {
			isLoading = true;
			const s = await getVkSettings();
			if (s == null) {
				return;
			}
			access_token = s.access_token ?? '';
			api_version = s.api_version || '5.199';
			group_id = s.group_id ?? '';
			live_enabled = s.live_enabled ?? true;
			live_peer_id = s.live_peer_id ?? '';
			alerts_enabled = s.alerts_enabled ?? false;
			alerts_peer_id = s.alerts_peer_id ?? '';
			summary_enabled = s.summary_enabled ?? false;
			summary_peer_id = s.summary_peer_id ?? '';
			wall_post_enabled = s.wall_post_enabled ?? false;
			keyboard_base_url = s.keyboard_base_url ?? '';
			error = '';
		} catch (err) {
			error = err instanceof Error ? err.message : 'Неизвестная ошибка';
		} finally {
			isLoading = false;
		}
	};

	const saveSettings = async () => {
		try {
			isSaving = true;
			const body: VkConfig = {
				access_token,
				api_version,
				group_id,
				live_enabled,
				live_peer_id,
				alerts_enabled,
				alerts_peer_id,
				summary_enabled,
				summary_peer_id,
				wall_post_enabled,
				keyboard_base_url
			};
			await saveVkSettings(body);
		} catch (err) {
			err instanceof Error ? err.message : 'Ошибка сохранения';
		} finally {
			isSaving = false;
		}
	};

	const toggleTokenVisibility = () => {
		showToken = !showToken;
	};

	$effect(() => {
		loadSettings();
	});

	async function postRestart() {
		try {
			isSaving = true;
			await onToggle();
			await fetch('/rest/restart', {
				method: 'POST',
				headers: {
					Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
				}
			});
		} finally {
			isSaving = false;
			notifications.error('Ошибка включения подсистемы', 5000);
		}
	}

	function confirmToggle(event: Event) {
		const input = event.currentTarget as HTMLInputElement;
		input.checked = !input.checked;
		const status = disabled ? 'включить' : 'отключить';
		modals.open(ConfirmDialog, {
			title: 'Включение подсистемы VK',
			message: 'Вы действительно хотите ' + status + ' подсистему и перезагрузить микроконтроллер?',
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
</script>

<div class="settings-container" class:disabled>
	<div class="settings-grid">
		<div class="settings-panel">
			<div class="settings-section">
				<div class="settings-group">
					<div class="settings-item">
						<div class="input-label-container">
							<span class="input-label">Состояние подсистемы VK</span>
							<span class="settings-description">{disabled ? 'Отключена' : 'Активна'}</span>
						</div>
						<Toggle checked={!disabled} onchange={confirmToggle} />
					</div>

					{#if isLoading}
						<div class="settings-item">
							<div class="loading-container flex flex-col items-center gap-2">
								<p class="loading-text">Загрузка настроек...</p>
								<span
									class="loading loading-spinner loading-lg text-primary"
									aria-hidden="true"
								></span>
							</div>
						</div>
					{:else if error}
						<div class="settings-item">
							<div class="error-container">
								<p class="error-text">Ошибка: {error}</p>
							</div>
						</div>
					{:else}
						<div class="settings-item settings-item--stacked">
							<span class="input-label">Ключ доступа сообщества</span>
							<div class="input-wrapper">
								<input
									type={showToken ? 'text' : 'password'}
									bind:value={access_token}
									disabled={disabled}
									class="input-field"
									placeholder="vk1.a...."
								/>
								<button
									type="button"
									onclick={toggleTokenVisibility}
									disabled={disabled}
									class="edit-button"
									title={showToken ? 'Скрыть' : 'Показать'}
								>
									{#if showToken}
										<Eye />
									{:else}
										<Eye_off />
									{/if}
								</button>
							</div>
							<p class="settings-description">
								Управление сообществом → Настройки → Работа с API → ключ (сообщения; для стены — право на
								запись на стене).
							</p>
						</div>

						<div class="settings-item settings-item--stacked">
							<span class="input-label">Версия API</span>
							<input
								type="text"
								bind:value={api_version}
								disabled={disabled}
								class="input-field"
								placeholder="5.199"
							/>
						</div>

						<div class="settings-item settings-item--stacked">
							<span class="input-label">ID сообщества (для стены)</span>
							<input
								type="text"
								bind:value={group_id}
								disabled={disabled}
								class="input-field"
								placeholder="123456789"
							/>
							<p class="settings-description">Положительное число из ссылки на группу. Нужно для wall.post.</p>
						</div>

						<div class="settings-item">
							<div class="input-label-container">
								<span class="input-label">Живой статус (одно сообщение + правки)</span>
							</div>
							<Toggle bind:checked={live_enabled} disabled={disabled} />
						</div>
						<div class="settings-item settings-item--stacked">
							<span class="input-label">Peer ID (статус)</span>
							<input
								type="text"
								bind:value={live_peer_id}
								disabled={disabled}
								class="input-field"
								placeholder="ID пользователя"
							/>
						</div>

						<div class="settings-item">
							<div class="input-label-container">
								<span class="input-label">Дублировать тревоги в VK</span>
							</div>
							<Toggle bind:checked={alerts_enabled} disabled={disabled} />
						</div>
						<div class="settings-item settings-item--stacked">
							<span class="input-label">Peer ID (тревоги)</span>
							<input
								type="text"
								bind:value={alerts_peer_id}
								disabled={disabled}
								class="input-field"
							/>
						</div>

						<div class="settings-item">
							<div class="input-label-container">
								<span class="input-label">Итог ректификации в ЛС</span>
							</div>
							<Toggle bind:checked={summary_enabled} disabled={disabled} />
						</div>
						<div class="settings-item settings-item--stacked">
							<span class="input-label">Peer ID (итог)</span>
							<input
								type="text"
								bind:value={summary_peer_id}
								disabled={disabled}
								class="input-field"
							/>
						</div>

						<div class="settings-item">
							<div class="input-label-container">
								<span class="input-label">Пост на стене при завершении</span>
							</div>
							<Toggle bind:checked={wall_post_enabled} disabled={disabled} />
						</div>

						<div class="settings-item settings-item--stacked">
							<span class="input-label">Кнопка «Веб» (open_link), опционально</span>
							<input
								type="text"
								bind:value={keyboard_base_url}
								disabled={disabled}
								class="input-field"
								placeholder="пусто = без кнопки (как python vk_bot.py); иначе https://…"
							/>
						</div>

						<div class="settings-item">
							<div class="modal-actions">
								<button
									onclick={saveSettings}
									disabled={disabled || isSaving}
									class="btn btn-primary"
								>
									{#if isSaving}
										Сохранение...
									{:else}
										Сохранить настройки
									{/if}
								</button>
							</div>
						</div>
					{/if}
				</div>
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	@use '$lib/styles/base/variables' as v;

	.settings-grid {
		grid-template-columns: 1fr;

		@media (min-width: v.$breakpoint-md) {
			grid-template-columns: 1fr;
		}
	}
</style>
