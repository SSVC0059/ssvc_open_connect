<!-- src/lib/components/OCSettings/TelegramSettings.svelte -->
<script lang="ts">
	import { user } from '$lib/stores/user';

	import { getTelegramSettings, saveTelegramSettings } from '$lib/api/ssvcApi';
	import Eye from '~icons/tabler/eye';
	import Eye_off from '~icons/tabler/eye-off';
	import { page } from '$app/state';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import Cancel from '~icons/tabler/x';
	import Power from '~icons/tabler/reload';
	import { notifications } from '$lib/components/toasts/notifications';

	let token = $state('');
	let chat_id = $state('');
	let isLoading = $state(true);
	let isSaving = $state(false);
	let error = $state('');
	let showToken = $state(false);
	let { disabled = false, onToggle } = $props();
	let isProcessing = $state(false);

	// Загрузка текущих настроек
	const loadSettings = async () => {
		try {
			isLoading = true;
			const settings = await getTelegramSettings();
			if (settings == null) {
				return;
			}
			token = settings.token;
			chat_id = settings.chat_id;
			error = '';
		} catch (err) {
			error = err instanceof Error ? err.message : 'Неизвестная ошибка';
		} finally {
			isLoading = false;
		}
	};

	// Сохранение настроек
	const saveSettings = async () => {
		try {
			isSaving = true;
			await saveTelegramSettings({ token, chat_id });
		} catch (err) {
			const message = err instanceof Error ? err.message : 'Ошибка сохранения';
		} finally {
			isSaving = false;
		}
	};

	// Переключение видимости токена
	const toggleTokenVisibility = () => {
		showToken = !showToken;
	};

	// Загружаем настройки при монтировании
	$effect(() => {
		loadSettings();
	});

	async function postRestart() {
		try {
			isSaving = true;
			await onToggle();
			const response = await fetch('/rest/restart', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
				}
			});
		} finally {
			isSaving = false;
			notifications.error('Ошибка включения подсистемы', 5000);
		}
	}

	function confirmToggle() {
		let status = disabled ? 'включить' : 'отключить'
		modals.open(ConfirmDialog, {
			title: 'Включение подсистемы',
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
	<div class="settings-item">
		<div class="settings-label-container">
			<span class="settings-label">Состояние подсистемы</span>
			<span class="settings-description">{disabled ? 'Отключена' : 'Активна'}</span>
		</div>
		<label class="toggle-container">
			<input
				type="checkbox"
				class="toggle-input"
				checked={!disabled}
				onchange={confirmToggle}
			/>
			<span class="toggle-slider"></span>
		</label>
	</div>

	{#if isLoading}
		<div class="loading-container">
			<p class="loading-text">Загрузка настроек...</p>
			<div class="loading-spinner"></div>
		</div>
	{:else if error}
		<div class="error-container">
			<p class="error-text">Ошибка: {error}</p>
		</div>
	{:else}
			<div class="settings-section">
				<!-- Поле для токена бота -->
				<div class="input-group">
					<span class="input-label">Telegram Bot Token</span>
					<div class="input-wrapper">
						<input
							type={showToken ? 'text' : 'password'}
							bind:value={token}
							disabled={disabled}
							class="input-field"
							placeholder="Введите токен бота"
						/>
						<button
							type="button"
							onclick={toggleTokenVisibility}
							disabled={disabled}
							class="edit-button"
							title={showToken ? 'Скрыть токен' : 'Показать токен'}
						>
							{#if showToken}
								<Eye />
							{:else}
								<Eye_off />
							{/if}
						</button>
					</div>
					<p class="settings-description">
						Пример: <code>7532468911:MAHOs4hPCb0lfK6QlOFynhoKIEIJrAfiCd34</code>
					</p>
				</div>

				<!-- Поле для Chat ID -->
				<div class="input-group">
					<span class="input-label">Chat ID</span>
					<div class="input-wrapper">
						<input
							type="text"
							bind:value={chat_id}
							disabled={disabled}
							class="input-field"
							placeholder="Введите ID чата"
						/>
					</div>
					<p class="settings-description">
						Числовой ID чата или канала (например: <code>-1002737904165</code>)
					</p>
				</div>

				<!-- Кнопка сохранения -->
				<div class="modal-actions">
					<button
						onclick={saveSettings}
						disabled={disabled || isSaving}
						class="btn btn-primary"
					>
						{#if isSaving}
							<svg class="btn-icon spinner" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
								<circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
								<path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
							</svg>
							Сохранение...
						{:else}
							Сохранить настройки
						{/if}
					</button>
				</div>
			</div>
	{/if}
</div>

