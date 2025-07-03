<!-- src/lib/components/OCSettings/TelegramSettings.svelte -->
<script lang="ts">
	import { user } from '$lib/stores/user';

	import { getTelegramSettings, saveTelegramSettings } from '$lib/api/settingsApi';
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

<div class=" max-w-3xl mx-auto p-6 relative" class:disabled>
	{#if disabled}
		<div class="absolute inset-0 bg-white/80 dark:bg-gray-800/80 z-10 rounded-lg flex items-center justify-center">
			<div class="text-center p-6 bg-white dark:bg-gray-700 border border-gray-200 dark:border-gray-600 rounded-lg shadow-lg max-w-xs w-full">
				<h3 class="font-medium text-lg mb-2">Подсистема Telegram отключена</h3>
				<p class="text-gray-600 dark:text-gray-300 mb-4 text-sm">
					Для настройки уведомлений включите подсистему
				</p>

				<button
					onclick={confirmToggle}
					class="w-full px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white font-medium rounded-lg transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
					disabled={isSaving}
				>
					{#if isSaving}
						<svg class="animate-spin h-5 w-5 text-white" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
							<circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
							<path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
						</svg>
						Обработка...
					{:else}
						Включить подсистему
					{/if}
				</button>
			</div>
		</div>
	{/if}
	<h2 class="text-xl font-semibold mb-6 text-gray-800 dark:text-gray-200">
		Настройки Telegram уведомлений
	</h2>

	<!-- Блок управления состоянием подсистемы -->
	<div class="mt-6 pt-4 border-t border-gray-200 dark:border-gray-700">
		<div class="flex justify-between items-center">
			<div>
				<h3 class="font-medium text-gray-900 dark:text-white">Состояние подсистемы</h3>
				<p class="text-sm text-gray-500 dark:text-gray-400">
					{disabled ? 'Отключена' : 'Активна'}
				</p>
			</div>
			<button
				onclick={confirmToggle}
				class="relative inline-flex h-6 w-11 items-center rounded-full transition-colors
        {disabled ? 'bg-gray-200 dark:bg-gray-600' : 'bg-blue-600'}"
			>
				<span class="sr-only">Переключить состояние</span>
				<span class="inline-block h-4 w-4 transform rounded-full bg-white transition-transform
        {disabled ? 'translate-x-1' : 'translate-x-6'}"
				></span>
			</button>
		</div>
	</div>

	{#if isLoading}
		<div class="flex flex-col items-center justify-center p-8 text-gray-600">
			<p class="mb-4">Загрузка настроек...</p>
			<div
				class="w-8 h-8 border-4 border-blue-200 border-t-blue-500 rounded-full animate-spin"
			></div>
		</div>
	{:else if error}
		<div class="p-4 text-red-600 bg-red-50 rounded-lg">
			<p>Ошибка: {error}</p>
		</div>
	{:else}
		<div class="w-full rounded-lg border border-gray-200 p-4 hover:shadow-md transition-shadow">
			<!-- Поле для токена бота -->
			<div>
				<label for="botToken" class="w-full space-y-3"> Telegram Bot Token </label>
				<div class="relative">
					<input
						id="botToken"
						type={showToken ? 'text' : 'password'}
						bind:value={token}
						disabled={disabled}
						class="w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg focus:ring-blue-500 focus:border-blue-500 dark:bg-gray-700 dark:text-white disabled:opacity-50 disabled:cursor-not-allowed"
						placeholder="Введите токен бота"
					/>
					<button
						type="button"
						onclick={toggleTokenVisibility}
						disabled={disabled}
						class="absolute inset-y-0 right-0 pr-3 flex items-center text-gray-500 hover:text-gray-700 disabled:opacity-50 disabled:cursor-not-allowed"
						title={showToken ? 'Скрыть токен' : 'Показать токен'}
					>
						{#if showToken}
							<Eye />
						{:else}
							<Eye_off />
						{/if}
					</button>
				</div>
				<p class="mt-1 text-xs text-gray-500 dark:text-gray-400">
					Пример: <code class="bg-gray-100 dark:bg-gray-700 px-1"
						>7532468911:MAHOs4hPCb0lfK6QlOFynhoKIEIJrAfiCd34</code
					>
				</p>
			</div>

			<!-- Поле для Chat ID -->
			<div>
				<label for="chatId" class="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
					Chat ID
				</label>
				<input
					id="chatId"
					type="text"
					bind:value={chat_id}
					disabled={disabled}
					class="w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg focus:ring-blue-500 focus:border-blue-500 dark:bg-gray-700 dark:text-white  disabled:opacity-50 disabled:cursor-not-allowed"
					placeholder="Введите ID чата"
				/>
				<p class="mt-1 text-xs text-gray-500 dark:text-gray-400">
					Числовой ID чата или канала (например: <code class="bg-gray-100 dark:bg-gray-700 px-1"
						>-1002737904165</code
					>)
				</p>
			</div>

			<!-- Кнопка сохранения -->
			<div class="flex justify-end">
				<button
					onclick={saveSettings}
					disabled={disabled || isSaving}
					class="px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white font-medium rounded-lg transition-colors focus:outline-none focus:ring-2 focus:ring-blue-500 focus:ring-offset-2 dark:focus:ring-offset-gray-800 disabled:opacity-50 disabled:cursor-not-allowed"
				>
					{#if isSaving}
						<span class="inline-flex items-center">
							<svg
								class="animate-spin -ml-1 mr-2 h-4 w-4 text-white"
								xmlns="http://www.w3.org/2000/svg"
								fill="none"
								viewBox="0 0 24 24"
							>
								<circle
									class="opacity-25"
									cx="12"
									cy="12"
									r="10"
									stroke="currentColor"
									stroke-width="4"
								></circle>
								<path
									class="opacity-75"
									fill="currentColor"
									d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"
								></path>
							</svg>
							Сохранение...
						</span>
					{:else}
						Сохранить настройки
					{/if}
				</button>
			</div>
		</div>
	{/if}

</div>

<style>
    .disabled {
        position: relative;
    }
</style>
