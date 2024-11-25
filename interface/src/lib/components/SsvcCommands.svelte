<script lang="ts">
	import Stop from '~icons/tabler/player-stop-filled'
	import Pause from '~icons/tabler/player-pause-filled'
	import Play from '~icons/tabler/player-play-filled'
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import { closeModal, openModal } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import { postStop, postPause, postResume} from '$lib/utils/ssvcHelper';

	function confirmStop() {
		openModal(ConfirmDialog, {
			title: 'Прерывание процесса',
			message: 'Вы точно хотите прервать ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				closeModal();
				postStop();
			}
		});
	}

	function confirmPause() {
		openModal(ConfirmDialog, {
			title: 'Остановка процесса',
			message: 'Вы точно хотите остановить ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				closeModal();
				postPause();
			}
		});
	}

	function confirmResume() {
		openModal(ConfirmDialog, {
			title: 'Продолжить процесс',
			message: 'Вы точно хотите продолжить ректификацию?',
			labels: {
				cancel: { label: 'Нет', icon: Cancel },
				confirm: { label: 'Да', icon: Check }
			},
			onConfirm: () => {
				closeModal();
				postResume();
			}
		});
	}

</script>

<div class="flex flex-col sm:flex-row sm:flex-wrap sm:justify-between gap-2">
	<div class="flex-grow hidden sm:block" />
	<button class="btn btn-primary inline-flex items-center justify-center sm:justify-start" on:click={confirmStop}>
		<Stop class="mr-2 h-5 w-5" />
		<span>Прервать</span>
	</button>
	<button class="btn btn-primary inline-flex items-center justify-center sm:justify-start" on:click={confirmPause}>
		<Pause class="mr-2 h-5 w-5" />
		<span>Пауза</span>
	</button>
	<button class="btn btn-primary inline-flex items-center justify-center sm:justify-start" on:click={confirmResume}>
		<Play class="mr-2 h-5 w-5" />
		<span>Продолжить</span>
	</button>
</div>
