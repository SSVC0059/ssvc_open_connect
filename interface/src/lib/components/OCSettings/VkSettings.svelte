<!-- VK community messenger settings (OpenConnect) -->
<script lang="ts">
	import { user } from '$lib/stores/user';
	import { getVkSettings, saveVkSettings, fetchVkConversationPeers } from '$lib/api/ssvcApi';
	import Eye from '~icons/tabler/eye';
	import Eye_off from '~icons/tabler/eye-off';
	import { page } from '$app/stores';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import Cancel from '~icons/tabler/x';
	import Power from '~icons/tabler/reload';
	import { notifications } from '$lib/components/toasts/notifications';
	import Toggle from '$lib/components/Toggle.svelte';
	import type { VkConfig, VkConversationPeer } from '$lib/types/ssvc';

	let access_token = $state('');
	let api_version = $state('5.199');
	let group_id = $state('');
	let peer_id = $state('');
	let live_enabled = $state(true);
	let alerts_enabled = $state(false);
	let summary_enabled = $state(false);
	let wall_post_enabled = $state(false);

	let isLoading = $state(true);
	let isSaving = $state(false);
	let error = $state('');
	let showToken = $state(false);
	let peersLoading = $state(false);
	let peerChoices = $state<VkConversationPeer[]>([]);
	let peerPickerOpen = $state(false);
	let peersError = $state('');
	let {
		disabled = false,
		onToggle
	}: {
		disabled?: boolean;
		onToggle: () => Promise<boolean>;
	} = $props();

	function resolvePeerFromConfig(s: VkConfig): string {
		if (s.peer_id?.trim()) {
			return s.peer_id.trim();
		}
		return (
			s.live_peer_id?.trim() ||
			s.alerts_peer_id?.trim() ||
			s.summary_peer_id?.trim() ||
			''
		);
	}

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
			peer_id = resolvePeerFromConfig(s);
			live_enabled = s.live_enabled ?? true;
			alerts_enabled = s.alerts_enabled ?? false;
			summary_enabled = s.summary_enabled ?? false;
			wall_post_enabled = s.wall_post_enabled ?? false;
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
				peer_id,
				live_enabled,
				alerts_enabled,
				summary_enabled,
				wall_post_enabled
			};
			const ok = await saveVkSettings(body);
			if (ok) {
				notifications.success('Настройки VK сохранены', 3000);
			} else {
				notifications.error('Не удалось сохранить настройки VK', 5000);
			}
		} catch (err) {
			notifications.error(err instanceof Error ? err.message : 'Ошибка сохранения', 5000);
		} finally {
			isSaving = false;
		}
	};

	const toggleTokenVisibility = () => {
		showToken = !showToken;
	};

	const openPeerPicker = async () => {
		if (disabled) {
			return;
		}
		if (!access_token.trim()) {
			notifications.error('Сначала укажите ключ доступа сообщества', 5000);
			return;
		}
		peersError = '';
		peersLoading = true;
		peerPickerOpen = true;
		try {
			const res = await fetchVkConversationPeers(access_token.trim(), api_version.trim() || '5.199');
			if (res == null || !res.ok) {
				peerChoices = [];
				peersError = res?.error ?? 'Не удалось загрузить диалоги (проверьте токен и Wi‑Fi на устройстве)';
				return;
			}
			peerChoices = res.peers ?? [];
			if (peerChoices.length === 0) {
				peersError =
					'Нет диалогов. Напишите сообществу VK от пользователя, затем обновите список.';
			}
		} catch (e) {
			peersError = e instanceof Error ? e.message : 'Ошибка загрузки';
			peerChoices = [];
		} finally {
			peersLoading = false;
		}
	};

	const applyPeerChoice = (id: number) => {
		peer_id = String(id);
		peerPickerOpen = false;
	};

	const closePeerPicker = () => {
		peerPickerOpen = false;
	};

	$effect(() => {
		loadSettings();
	});

	async function postRestart() {
		try {
			isSaving = true;
			const ok = await onToggle();
			if (!ok) {
				notifications.error('Не удалось изменить состояние подсистемы VK', 5000);
				return;
			}
			const res = await fetch('/rest/restart', {
				method: 'POST',
				headers: {
					Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
				}
			});
			if (!res.ok) {
				throw new Error('Ошибка перезагрузки микроконтроллера (' + res.status + ')');
			}
			notifications.success('Изменение подсистемы VK применено, микроконтроллер перезагружается', 3000);
		} catch (err) {
			notifications.error(err instanceof Error ? err.message : 'Ошибка включения подсистемы', 5000);
		} finally {
			isSaving = false;
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
								Управление сообществом → Настройки → Работа с API → ключ с правом «Сообщения»; для
								стены — «Запись на стене».
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
							<span class="input-label">Peer ID (чат с сообществом)</span>
							<div class="peer-row">
								<input
									type="text"
									bind:value={peer_id}
									disabled={disabled}
									class="input-field"
									placeholder="ID пользователя или беседы"
								/>
								<button
									type="button"
									class="btn btn-outline btn-sm"
									disabled={disabled || peersLoading}
									onclick={openPeerPicker}
								>
									Из диалогов VK
								</button>
							</div>
							<p class="settings-description">
								Один диалог для телеметрии, тревог и итога в ЛС. Сначала напишите сообществу от своего
								аккаунта.
							</p>
						</div>

						<div class="settings-item">
							<div class="input-label-container">
								<span class="input-label">Телеметрия (live, правка ≤1 раз / 20 с)</span>
							</div>
							<Toggle bind:checked={live_enabled} disabled={disabled} />
						</div>

						<div class="settings-item">
							<div class="input-label-container">
								<span class="input-label">Тревоги (новое сообщение ≤1 раз / 20 с)</span>
								<span class="settings-description">Предыдущее тревожное сообщение удаляется</span>
							</div>
							<Toggle bind:checked={alerts_enabled} disabled={disabled} />
						</div>

						<div class="settings-item">
							<div class="input-label-container">
								<span class="input-label">Итог ректификации в ЛС</span>
							</div>
							<Toggle bind:checked={summary_enabled} disabled={disabled} />
						</div>

						<div class="settings-item">
							<div class="input-label-container">
								<span class="input-label">Пост на стене при завершении</span>
							</div>
							<Toggle bind:checked={wall_post_enabled} disabled={disabled} />
						</div>

						{#if wall_post_enabled}
							<div class="settings-item settings-item--stacked">
								<span class="input-label">ID сообщества (для стены)</span>
								<input
									type="text"
									bind:value={group_id}
									disabled={disabled}
									class="input-field"
									placeholder="123456789"
								/>
								<p class="settings-description">
									Положительное число из ссылки на группу. Нужно для wall.post.
								</p>
							</div>
						{/if}

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

	{#if peerPickerOpen}
		<div class="peer-picker-backdrop" role="presentation" onclick={closePeerPicker}></div>
		<div class="peer-picker-panel" role="dialog" aria-label="Выбор peer_id">
			<h3 class="peer-picker-title">Диалоги сообщества VK</h3>
			{#if peersLoading}
				<p>Загрузка…</p>
			{:else if peersError}
				<p class="error-text">{peersError}</p>
			{:else}
				<ul class="peer-picker-list">
					{#each peerChoices as p (p.peer_id)}
						<li>
							<button type="button" class="peer-picker-item" onclick={() => applyPeerChoice(p.peer_id)}>
								<span class="peer-picker-id">{p.peer_id}</span>
								{#if p.title}
									<span class="peer-picker-title-text">{p.title}</span>
								{/if}
								{#if p.type}
									<span class="peer-picker-type">{p.type}</span>
								{/if}
							</button>
						</li>
					{/each}
				</ul>
			{/if}
			<button type="button" class="btn btn-ghost btn-sm mt-2" onclick={closePeerPicker}>Закрыть</button>
		</div>
	{/if}
</div>

<style lang="scss">
	@use '$lib/styles/base/variables' as v;

	.settings-grid {
		grid-template-columns: 1fr;

		@media (min-width: v.$breakpoint-md) {
			grid-template-columns: 1fr;
		}
	}

	.peer-row {
		display: flex;
		flex-wrap: wrap;
		gap: 0.5rem;
		align-items: center;

		.input-field {
			flex: 1 1 12rem;
			min-width: 0;
		}
	}

	.peer-picker-backdrop {
		position: fixed;
		inset: 0;
		background: rgb(0 0 0 / 0.45);
		z-index: 40;
	}

	.peer-picker-panel {
		position: fixed;
		z-index: 41;
		left: 50%;
		top: 50%;
		transform: translate(-50%, -50%);
		max-width: min(28rem, 92vw);
		max-height: 70vh;
		overflow: auto;
		padding: 1rem 1.25rem;
		border-radius: 0.5rem;
		background: var(--color-base-100, #fff);
		box-shadow: 0 8px 32px rgb(0 0 0 / 0.2);
	}

	.peer-picker-title {
		font-size: 1rem;
		font-weight: 600;
		margin-bottom: 0.75rem;
	}

	.peer-picker-list {
		list-style: none;
		padding: 0;
		margin: 0;
	}

	.peer-picker-item {
		display: flex;
		flex-wrap: wrap;
		gap: 0.35rem 0.75rem;
		width: 100%;
		text-align: left;
		padding: 0.5rem 0.25rem;
		border-bottom: 1px solid rgb(0 0 0 / 0.08);
	}

	.peer-picker-id {
		font-family: ui-monospace, monospace;
		font-weight: 600;
	}

	.peer-picker-type {
		font-size: 0.75rem;
		opacity: 0.7;
	}

	.peer-picker-title-text {
		font-size: 0.875rem;
	}
</style>
