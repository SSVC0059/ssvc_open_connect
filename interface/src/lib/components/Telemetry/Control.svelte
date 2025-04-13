<script lang="ts">
	import type { RectStatus } from '$lib/types/models';

	import Stop from '~icons/tabler/player-stop-filled';
	import Pause from '~icons/tabler/player-pause-filled';
	import Play from '~icons/tabler/player-play-filled';
	import Next from '~icons/tabler/player-track-next'; // Иконка для кнопки Start
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';

	const { status, onStatusUpdate } = $props<{
		status: RectStatus | undefined;
		onStatusUpdate: () => Promise<void>; // Функция возвращает Promise
	}>();

	function confirmModalStop() {
		modals.open(ConfirmDialog, {
			title: 'Прерывание процесса',
			message: 'Вы точно хотите прервать ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				modals.close();
				sendRequest('stop');
			}
		});
	}

	function confirmModalPause() {
		modals.open(ConfirmDialog, {
			title: 'Приостановка процесса',
			message: 'Вы точно хотите приостановить ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				modals.close();
				sendRequest('pause');
			}
		});
	}

	function confirmModalResume() {
		modals.open(ConfirmDialog, {
			title: 'Продолжение процесса процесса',
			message: 'Вы точно хотите продолжить ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				modals.close();
				sendRequest('resume');
			}
		});
	}

	function confirmModalNext() {
		modals.open(ConfirmDialog, {
			title: 'Переход на следующий этап',
			message: 'Перейди на следующий этап?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				modals.close();
				sendRequest('next');
			}
		});
	}

	function confirmModalStart() {
		modals.open(ConfirmDialog, {
			title: 'Запуск процесса',
			message: 'Вы точно хотите начать ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				modals.close();
				sendRequest('start');
			}
		});
	}

	async function sendRequest(command: string) {
		let endpoint = '/rest/commands';
		let method = 'POST';
		const headers = {
			Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
			'Content-Type': 'application/json',
			Accept: '*/*'
		};

		const body = JSON.stringify({ commands: command }); // Преобразование параметров в JSON

		const response = await fetch(endpoint, {
			method,
			headers,
			body // Добавление тела запроса
		});

		if (!response.ok) {
			throw new Error(`HTTP error! status: ${response.status}`);
		} else {
			await new Promise((resolve) => setTimeout(resolve, 1000)); // 1 секунда
			await onStatusUpdate();
		}

		return response; // Предполагается, что ответ в формате JSON
	}

	// Реактивные вычисления
	const { stopDisabled, pauseResumeDisabled, nextStageDisabled, resumeShow, isWaiting } = $derived({
		stopDisabled: !status || status.stage === 'waiting',
		pauseResumeDisabled: !status || ['waiting', 'tp1_waiting'].includes(status.stage),
		nextStageDisabled: status && (status.stage == 'waiting' || status.stage == 'finished'),
		resumeShow: status?.status === 'paused',
		isWaiting: status?.stage === 'waiting'
	});
</script>

<div class="flex flex-row flex-wrap justify-center items-center gap-2">
	{#if isWaiting}
		<div class="inline-flex flex-col items-center gap-1">
			<button
				class="btn btn-primary inline-flex items-center justify-center gap-2"
				onclick={() => confirmModalStart()}
			>
				<Play class="h-6 w-6 sm:h-5 sm:w-5" />
				<span class="hidden sm:inline">Начать</span>
			</button>
			<span class="text-xs text-gray-400 dark:text-gray-300 font-light text-center w-full">
				Будет доступно в следующем релизе
			</span>
		</div>
	{:else}
		<!-- Остальные кнопки видны во всех режимах кроме waiting -->
		<button class="btn btn-primary" disabled={stopDisabled} onclick={() => confirmModalStop()}>
			<Stop class="h-6 w-6 sm:h-5 sm:w-5" />
			<span class="hidden sm:inline">Прервать</span>
		</button>

		<!-- Кнопка "Продолжить" или "Пауза" -->
		{#if resumeShow}
			<button
				disabled={pauseResumeDisabled}
				class="btn btn-primary inline-flex items-center justify-center gap-2"
				onclick={() => confirmModalResume()}
			>
				<Play class="h-6 w-6 sm:h-5 sm:w-5" />
				<span class="hidden sm:inline">Продолжить</span>
			</button>
		{:else}
			<button
				disabled={pauseResumeDisabled}
				class="btn btn-primary inline-flex items-center justify-center gap-2"
				onclick={() => confirmModalPause()}
			>
				<Pause class="h-6 w-6 sm:h-5 sm:w-5" />
				<span class="hidden sm:inline">Пауза</span>
			</button>
		{/if}

		<!-- Кнопка "Следующий этап" -->
		<button
			class="btn btn-primary inline-flex items-center justify-center gap-2"
			disabled={nextStageDisabled}
			onclick={() => confirmModalNext()}
		>
			<Next class="h-6 w-6 sm:h-5 sm:w-5" />
			<span class="hidden sm:inline">Следующий этап</span>
		</button>
	{/if}
</div>
