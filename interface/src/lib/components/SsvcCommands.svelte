<script lang="ts">
	import Stop from '~icons/tabler/player-stop-filled'
	import Pause from '~icons/tabler/player-pause-filled'
	import Play from '~icons/tabler/player-play-filled'
	import { user } from '$lib/stores/user';
	import { page } from '$app/stores';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import { closeModal, openModal } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';

	function confirmModalStop() {
		openModal(ConfirmDialog, {
			title: 'Прерывание процесса',
			message: 'Вы точно хотите прервать ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				closeModal();
				sendRequest("stop");
			}
		});
	}

	function confirmModalPause() {
		openModal(ConfirmDialog, {
			title: 'Приостановка процесса',
			message: 'Вы точно хотите приостановить ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				closeModal();
				sendRequest("pause");
			}
		});
	}

	function confirmModalResume() {
		openModal(ConfirmDialog, {
			title: 'Продолжение процесса процесса',
			message: 'Вы точно хотите продолжить ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				closeModal();
				sendRequest("resume");
			}
		});
	}

	function confirmModalNext() {
		openModal(ConfirmDialog, {
			title: 'Переход на следующий этап',
			message: 'Перейди на следующий этап?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				closeModal();
				sendRequest("resume");
			}
		});
	}

	async function sendRequest(command: string) {
		let endpoint = '/rest/ssvcCommands';
		let method = 'POST';
		const headers = {
			Authorization: $page.data.features.security
				? 'Bearer ' + $user.bearer_token
				: 'Basic',
			'Content-Type': 'application/json',
			'Accept': '*/*'
		};

		const body = JSON.stringify({ commands: command });// Преобразование параметров в JSON

		const response = await fetch(endpoint, {
			method,
			headers,
			body // Добавление тела запроса
		});

		if (!response.ok) {
			throw new Error(`HTTP error! status: ${response.status}`);
		}

		console.log(response)

		return await response.json(); // Предполагается, что ответ в формате JSON
	}

</script>

<div class="flex flex-col sm:flex-row sm:flex-wrap sm:justify-between gap-2">
	<div class="flex-grow hidden sm:block" />
	<button class="btn btn-primary inline-flex items-center justify-center sm:justify-start" on:click={() => confirmModalStop()}>
		<Stop class="mr-2 h-5 w-5" />
		<span>Прервать</span>
	</button>
	<button class="btn btn-primary inline-flex items-center justify-center sm:justify-start" on:click={() => confirmModalPause()}>
		<Pause class="mr-2 h-5 w-5" />
		<span>Пауза</span>
	</button>
	<button class="btn btn-primary inline-flex items-center justify-center sm:justify-start" on:click={() => confirmModalResume()}>
		<Play class="mr-2 h-5 w-5" />
		<span>Продолжить</span>
	</button>
	<button class="btn btn-primary <Check /> Головы items-center justify-center sm:justify-start" on:click={() => confirmModalNext()}>
		<Play class="mr-2 h-5 w-5" />
		<span>Следующий этап</span>
	</button>
</div>
