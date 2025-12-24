<script lang="ts">
	import type { RectStatus, SendCommandResponse } from '$lib/types/ssvc';

	import Stop from '~icons/tabler/player-stop-filled';
	import Pause from '~icons/tabler/player-pause-filled';
	import Allert from '~icons/tabler/alert-triangle';
	import Play from '~icons/tabler/player-play-filled';
	import Next from '~icons/tabler/player-track-next'; // Иконка для кнопки Start
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import StartWizard from '$lib/components/StartWizard/StartWizard.svelte';
	import ChangeStageModal from '$lib/components/Telemetry/ChangeStageModal.svelte'
	import { sendCommand } from '$lib/api/ssvcApi'
	import ProfileSelector from '$lib/components/profiles/ProfileSelector.svelte';

	const { status } = $props<{
		status: RectStatus | undefined;
	}>();

	let showWizard = $state(false);

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
		modals.open(StartWizard, {
			onConfirm: () => {
				modals.close();
				sendRequest('start');
			}
		});
	}

	function changeStageModal() {
		modals.open(ChangeStageModal, {
			onConfirm: () => {
				modals.close();
			},
			onCancel: () => {
				modals.close();
				console.log('Настройки этапа отменены');
				// Можно добавить дополнительные действия при отмене
			}
		});
	}

	async function sendRequest(command: string) {

		const response: SendCommandResponse = await sendCommand(command)

		if (!response.success) {
			throw new Error(`HTTP error! status: ${response.message}`);
		} else {
			await new Promise((resolve) => setTimeout(resolve, 1000)); // 1 секунда

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

<button class="emergency-stop"
				onclick={() => sendRequest('stop')}
>
	<Allert/>
	<span>АВАРИЙНАЯ ОСТАНОВКА</span>
</button>

<div class="profile-selector-container">
	<ProfileSelector />
</div>

<!-- Power Controls -->
<div class="power-controls">

	<button class="control-btn power-btn"
					disabled={stopDisabled}
					onclick={() => confirmModalStop()}
	>
		<div class="icon">
			<Stop data-feather="play"/>
		</div>
		<span class="btn-label">Прервать</span>
	</button>

	<button
		class="control-btn start-btn"
		onclick={() => isWaiting ? confirmModalStart() : (resumeShow ? confirmModalResume() : confirmModalPause())}
		hidden={!isWaiting ? false : !resumeShow && !isWaiting}
	>
		<div class="icon">
			{#if isWaiting}
				<Play data-feather="play"/>
			{:else if resumeShow}
				<Play data-feather="play"/>
			{:else}
				<Pause data-feather="play"/>
			{/if}
		</div>
		<span class="btn-label">
        {isWaiting ? 'Старт' : (resumeShow ? 'Продолжить' : 'Пауза')}
    </span>
	</button>

	<button class="control-btn pause-btn"
					onclick={() => confirmModalNext()}
					disabled={stopDisabled}
	>
		<div class="icon">
			<Next data-feather="play"/>
		</div>
		<span class="btn-label">Далее</span>
	</button>
</div>

<button class="control-btn change-btn"
				onclick={changeStageModal}
				disabled={isWaiting}
>
	<span>Настройки этапа</span>
</button>


<StartWizard isOpen={showWizard}/>

<style lang="scss">

  @mixin transition-all {
    transition: var(--transition);
  }

  .profile-selector-container {
	margin-bottom: 1rem;
  }

  .emergency-stop {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 2rem;

    background-color: var(--red-600);
    color: var(--white);
    font-weight: 700;
    padding: 1rem 1rem;
    border-radius: 0.75rem;

    transform: scale(1);
    @include transition-all;

    &:hover {
      background-color: var(--red-700);
      transform: scale(1.05);
    }

  }

  .change-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 2rem;

    background-color: var(--primary-700);
    color: var(--white);
    font-weight: 700;
    padding: 1rem 1rem;
    border-radius: 0.75rem;

    &:hover {
      background-color: var(--primary-800);

    }
  }

  .power-controls {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
    gap: 0.5rem;

    .control-btn {
      padding: 0.8rem 0.5rem; /* увеличил вертикальные отступы */
      border-radius: 0.5rem;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 0.5rem; /* добавил отступ между иконкой и текстом */
      min-height: 80px; /* минимальная высота для красоты */
      @include transition-all;

      .icon {
        height: 24px; /* фиксированная высота для иконки */
        width: 24px; /* фиксированная ширина для иконки */
        display: flex;
        align-items: center;
        justify-content: center;

        svg {
          height: 100%;
          width: 100%;
        }
      }

      .btn-label {
        font-size: 1.2rem; /* немного уменьшил размер шрифта */
        text-align: center;
        line-height: 1.2;
      }

      &.power-btn {
        background-color: var(--blue-500);
        color: var(--white);

        &:hover {
          background-color: var(--blue-600);
        }
      }

      &.start-btn {
        background-color: var(--green-500);
        color: var(--white);

        &:hover {
          background-color: var(--green-600);
        }
      }

      &.pause-btn {
        background-color: var(--yellow-500);
        color: var(--white);

        &:hover {
          background-color: var(--yellow-600);
        }
      }
    }
  }

  button[disabled] {
    cursor: not-allowed;
    opacity: 0.4;
    filter: grayscale(0.3);
    border: 2px dashed var(--gray-400);
    color: var(--gray-500) !important;
  }
</style>