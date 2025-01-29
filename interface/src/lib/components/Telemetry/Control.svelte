<script lang="ts">
	import { GradientButton } from 'flowbite-svelte';
	import type { RectificationStatus } from '$lib/types/models';
	import { rectificationStatusStore, needUpdateRectificationStatusStore } from '$lib/stores/ssvcOpenConnect';

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
				sendRequest("next");
			}
		});
	}

	async function sendRequest(command: string) {
		let endpoint = '/rest/commands';
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
		console.log("responce end")
		needUpdateRectificationStatusStore.set(true)
		return response; // Предполагается, что ответ в формате JSON
	}

	let rectificationStatus: RectificationStatus
	let stopDisabled = "";
	let resumeShow = false;
	let pauseResumeDisabled = "";
	let nextStageDisabled = "";


	rectificationStatusStore.subscribe((value) => {
		if (value) {
			rectificationStatus = value;
		}
	});
	
	$: {
		if (rectificationStatus) {
			if (rectificationStatus.response.stage == "waiting" ) {
				stopDisabled = "disabled";
				pauseResumeDisabled = "disabled";
				nextStageDisabled = "disabled"
			} else if (rectificationStatus.response.stage == "tp1_waiting") {
				stopDisabled = "";
				pauseResumeDisabled = "disabled"
				nextStageDisabled = ""
			}else {
				stopDisabled = "";
				pauseResumeDisabled = "";
				nextStageDisabled = ""
			}
			if (rectificationStatus.response.status == "paused") {
				resumeShow = true
				} else {
				resumeShow = false
			}
		}
	}
</script>

<div class="flex flex-row flex-wrap justify-center items-center gap-2">
	<!-- Кнопка "Прервать" -->
	<GradientButton
		disabled={stopDisabled}
		class="btn-primary inline-flex items-center justify-center gap-2"
		on:click={() => confirmModalStop()}
	>
		<Stop class="h-6 w-6 sm:h-5 sm:w-5" />
		<span class="hidden sm:inline">Прервать</span>
	</GradientButton>

	<!-- Кнопка "Продолжить" или "Пауза" -->
	{#if resumeShow}
		<GradientButton
			disabled={pauseResumeDisabled}
			class="btn-primary inline-flex items-center justify-center gap-2"
			on:click={() => confirmModalResume()}
		>
			<Play class="h-6 w-6 sm:h-5 sm:w-5" />
			<span class="hidden sm:inline">Продолжить</span>
		</GradientButton>
	{:else}
		<GradientButton
			disabled={pauseResumeDisabled}
			class="btn-primary inline-flex items-center justify-center gap-2"
			on:click={() => confirmModalPause()}
		>
			<Pause class="h-6 w-6 sm:h-5 sm:w-5" />
			<span class="hidden sm:inline">Пауза</span>
		</GradientButton>
	{/if}

	<!-- Кнопка "Следующий этап" -->
	<GradientButton
		disabled={nextStageDisabled}
		class="btn-primary inline-flex items-center justify-center gap-2"
		on:click={() => confirmModalNext()}
	>
		<Play class="h-6 w-6 sm:h-5 sm:w-5" />
		<span class="hidden sm:inline">Следующий этап</span>
	</GradientButton>
</div>
