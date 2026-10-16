<script lang="ts">
	import { modals, onBeforeClose } from 'svelte-modals';
	import { focusTrap } from '$lib/actions/focusTrap';
	import { fly } from 'svelte/transition';
	import { pingDevice, requestRestart } from '$lib/api/ssvcApi';
	import Cancel from '~icons/tabler/x';
	import Power from '~icons/tabler/reload';
	import Check from '~icons/tabler/check';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import Loader from '~icons/tabler/loader-2';

	interface Props {
		isOpen?: boolean;
		title?: string;
		message?: string;
		/** Контроллер вернулся на связь: родитель перечитывает свои данные. */
		onFinished?: () => void | Promise<void>;
		onCancel?: () => void;
	}

	let {
		isOpen = false,
		title = 'Перезагрузка устройства',
		message = 'После изменения настроек нужна перезагрузка контроллера. Перезагрузить сейчас?',
		onFinished,
		onCancel = () => modals.close()
	}: Props = $props();

	/**
	 * confirm    — ждём решения, команда ещё не отправлена;
	 * restarting — команда ушла, контроллер ещё отвечает (уходит в перезапуск);
	 * waiting    — связи нет, ждём возврата;
	 * done       — контроллер снова отвечает; timeout — не дождались; error — команда отклонена.
	 */
	type Phase = 'confirm' | 'restarting' | 'waiting' | 'done' | 'timeout' | 'error';

	let phase = $state<Phase>('confirm');
	let errorText = $state('');
	let elapsedMs = $state(0);

	/** Ожидаемая длительность перезагрузки: до неё шкала заполняется, дальше ждёт на месте. */
	const EXPECTED_REBOOT_MS = 20_000;
	/** Сколько ждём ответа, прежде чем признать, что контроллер не вернулся. */
	const WAIT_TIMEOUT_MS = 90_000;
	const PROBE_INTERVAL_MS = 1000;
	const TICK_MS = 250;
	/** Доля шкалы, до которой она доходит сама: 100% показываем только по факту возврата. */
	const PROGRESS_CAP = 92;

	let tickTimer: ReturnType<typeof setInterval> | undefined;
	let startedAt = 0;
	/** Команда отправляется один раз, даже если пользователь дважды нажмёт кнопку. */
	let started = false;
	/** Компонент снят со страницы: ожидание прекращаем, состояние не трогаем. */
	let cancelled = false;

	const active = $derived(phase === 'restarting' || phase === 'waiting');
	const progress = $derived.by(() => {
		if (phase === 'confirm') return 0;
		if (phase === 'done') return 100;
		const ramp = 6 + (PROGRESS_CAP - 6) * Math.min(1, elapsedMs / EXPECTED_REBOOT_MS);
		return Math.min(PROGRESS_CAP, Math.round(ramp));
	});

	const statusText = $derived.by(() => {
		if (phase === 'restarting') return 'Команда отправлена. Ждём перезапуска контроллера…';
		if (phase === 'waiting') return 'Контроллер перезагружается. Ждём восстановления связи…';
		if (phase === 'done') return 'Контроллер снова на связи. Настройки применены.';
		if (phase === 'timeout')
			return 'Контроллер не ответил за отведённое время. Проверьте питание и сеть, затем обновите страницу.';
		return errorText || 'Команда перезагрузки отклонена контроллером.';
	});

	const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

	function stopTimers() {
		if (tickTimer) clearInterval(tickTimer);
		tickTimer = undefined;
	}

	/**
	 * Успешный опрос сразу после команды ещё не означает возврат: контроллер
	 * какое-то время продолжает отвечать, прежде чем уйти в перезапуск. Поэтому
	 * возвратом считаем только ответ после того, как связь уже пропадала.
	 */
	async function startRestart() {
		if (started) return;
		started = true;
		phase = 'restarting';
		elapsedMs = 0;
		startedAt = Date.now();
		tickTimer = setInterval(() => {
			elapsedMs = Date.now() - startedAt;
		}, TICK_MS);

		const outcome = await requestRestart();
		if (!outcome.ok) {
			stopTimers();
			errorText = outcome.error ?? '';
			phase = 'error';
			return;
		}

		let wentDown = false;
		while (Date.now() - startedAt < WAIT_TIMEOUT_MS) {
			await sleep(PROBE_INTERVAL_MS);
			if (cancelled) return;
			if (await pingDevice()) {
				if (wentDown) {
					stopTimers();
					phase = 'done';
					void onFinished?.();
					return;
				}
				continue;
			}
			wentDown = true;
			phase = 'waiting';
		}

		stopTimers();
		phase = 'timeout';
	}

	// Во время перезагрузки окно не закрываем: состояние контроллера ещё неизвестно.
	onBeforeClose(() => !active);

	// Уход со страницы (переход, перезагрузка вкладки) прекращает ожидание:
	// опрос перезагружающегося контроллера после этого не нужен.
	$effect(() => () => {
		cancelled = true;
		stopTimers();
	});
</script>

{#if isOpen}
	<div
		role="dialog"
		aria-modal="true"
		aria-labelledby="reboot-dialog-title"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center backdrop-blur-sm max-sm:p-4"
		transition:fly={{ y: 50, duration: 200 }}
		use:focusTrap
	>
		<div
			class="reboot-dialog pointer-events-auto flex w-full max-w-md flex-col gap-3 rounded-2xl border border-base-300 bg-base-100 p-6 shadow-2xl max-sm:max-w-none"
		>
			<h2 id="reboot-dialog-title" class="text-xl font-bold text-base-content">{title}</h2>
			<div class="divider my-0"></div>

			{#if phase === 'confirm'}
				<p class="text-base text-base-content">{message}</p>
			{:else}
				<div class="flex flex-col gap-3">
					<div class="flex items-center gap-2 text-base-content">
						{#if active}
							<Loader class="h-5 w-5 shrink-0 animate-spin text-primary" aria-hidden="true" />
						{:else if phase === 'done'}
							<Check class="h-5 w-5 shrink-0 text-success" aria-hidden="true" />
						{:else}
							<AlertTriangle class="h-5 w-5 shrink-0 text-warning" aria-hidden="true" />
						{/if}
						<span>{statusText}</span>
					</div>

					<!-- Шкала перезагрузки: заполняется по времени, 100% — по факту ответа контроллера. -->
					<progress
						class="progress progress-primary w-full"
						value={progress}
						max="100"
						aria-label="Прогресс перезагрузки контроллера"
					></progress>
					<div class="flex items-center justify-between text-xs text-base-content/60">
						<span>{progress}%</span>
						<span>прошло {Math.floor(elapsedMs / 1000)} с</span>
					</div>
				</div>
			{/if}

			<div class="divider my-0"></div>
			<div class="flex justify-end gap-2 max-sm:flex-col-reverse">
				{#if phase === 'confirm'}
					<button
						type="button"
						class="btn btn-primary inline-flex min-h-11 items-center justify-center gap-2 max-sm:w-full sm:min-h-0"
						onclick={() => {
							onCancel();
							modals.close();
						}}
					>
						<Cancel class="h-5 w-5 shrink-0" aria-hidden="true" />
						<span>Позже</span>
					</button>
					<button
						type="button"
						class="btn btn-warning text-warning-content inline-flex min-h-11 items-center justify-center gap-2 max-sm:w-full sm:min-h-0"
						onclick={startRestart}
					>
						<Power class="h-5 w-5 shrink-0" aria-hidden="true" />
						<span>Перезагрузить</span>
					</button>
				{:else if !active}
					<button
						type="button"
						class="btn btn-primary inline-flex min-h-11 items-center justify-center gap-2 max-sm:w-full sm:min-h-0"
						onclick={() => modals.close()}
					>
						<span class="text-center">Продолжить</span>
					</button>
				{/if}
			</div>
		</div>
	</div>
{/if}
