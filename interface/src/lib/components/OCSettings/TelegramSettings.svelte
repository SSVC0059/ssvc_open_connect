<!-- src/lib/components/OCSettings/TelegramSettings.svelte -->
<script lang="ts">
	import { getTelegramSettings, saveTelegramSettings } from '$lib/api/settingsApi';
	import Eye from '~icons/tabler/eye';
	import Eye_off from '~icons/tabler/eye-off';

	let botToken = $state('');
	let chatId = $state('');
	let isLoading = $state(true);
	let isSaving = $state(false);
	let error = $state('');
	let showToken = $state(false);

	// Загрузка текущих настроек
	const loadSettings = async () => {
		try {
			isLoading = true;
			const settings = await getTelegramSettings();
			botToken = settings.botToken;
			chatId = settings.chatId;
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
			await saveTelegramSettings({ botToken, chatId });
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
</script>

<div class=" max-w-3xl mx-auto p-6">
	<h2 class="text-xl font-semibold mb-6 text-gray-800 dark:text-gray-200">
		Настройки Telegram уведомлений
	</h2>

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
						bind:value={botToken}
						class="w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg focus:ring-blue-500 focus:border-blue-500 dark:bg-gray-700 dark:text-white"
						placeholder="Введите токен бота"
					/>
					<button
						type="button"
						onclick={toggleTokenVisibility}
						class="absolute inset-y-0 right-0 pr-3 flex items-center text-gray-500 hover:text-gray-700"
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
					bind:value={chatId}
					class="w-full px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg focus:ring-blue-500 focus:border-blue-500 dark:bg-gray-700 dark:text-white"
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
					disabled={isSaving}
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
