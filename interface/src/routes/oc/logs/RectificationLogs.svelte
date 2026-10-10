<script lang="ts">
	import {
		fetchLogStatus,
		refreshLogList,
		startLogDownload,
		downloadLog,
		cancelLogDownload,
		getInfo,
		LOG_IDLE_TIMEOUT_MS
	} from '$lib/api/ssvcApi';
	import type { SsvcLogStatus, OcApiFeature } from '$lib/types/ssvc';

	// Локальное состояние операции отделено от backend-снапшота: устаревший
	// «receiving» с контроллера не должен перекрывать уже показанную ошибку.
	type LocalLogState = 'idle' | 'starting' | 'waiting' | 'downloading' | 'done' | 'error' | 'cancelled';

	let logStatus = $state<SsvcLogStatus | null>(null);
	let logError = $state('');
	let localState = $state<LocalLogState>('idle');
	// Снапшот возможностей устройства из /rest/oc/info (см. SsvcApiCapabilities).
	let apiFeatures = $state<OcApiFeature[]>([]);

	// Журналы (GET_LOG) доступны с UART API 1.10 и только в дежурном режиме.
	// Решение принимает возможность get_log: сравнение версии как числа
	// сломало бы 1.10 (в JS это 1.1, то есть меньше 1.7).
	const logsFeature = $derived(apiFeatures.find((feature) => feature.name === 'get_log'));
	// Пока снапшот не загружен, элементы не блокируем — как и раньше при неизвестной версии.
	const logsSupported = $derived(apiFeatures.length === 0 || logsFeature?.available === true);
	const logsMinApi = $derived(logsFeature?.min_api ?? '1.10');

	const LOG_ERROR_MESSAGES: Record<string, string> = {
		timeout: 'Превышено время ожидания передачи журнала',
		err_num: 'Контроллер сообщил о неверном номере процесса',
		err_mnt: 'Ошибка монтирования файловой системы на контроллере',
		err_nf: 'Файл журнала не найден на контроллере',
		err_int: 'Внутренняя ошибка передачи на контроллере',
		log_transfer_busy: 'Уже передаётся другой журнал',
		api_version_unsupported: 'Версия API SSVC не поддерживает журналы'
	};

	const logErrorText = (code?: string | null): string =>
		code ? LOG_ERROR_MESSAGES[code] ?? code : '';

	let logRequestId = 0;
	let logAbort: AbortController | null = null;

	// Инвалидирует текущую операцию и прерывает её сетевые запросы.
	const cancelLocalRequest = () => {
		logRequestId += 1;
		logAbort?.abort();
		logAbort = null;
	};

	// Отложенный revoke: браузер может начать скачивание уже после клика.
	const saveBlob = (blob: Blob, fileName: string) => {
		const url = URL.createObjectURL(blob);
		const anchor = document.createElement('a');
		anchor.href = url;
		anchor.download = fileName;
		anchor.click();
		setTimeout(() => URL.revokeObjectURL(url), 60_000);
	};

	const loadLogs = async () => {
		// Явное обновление: refresh=1 действительно запрашивает список заново,
		// а не отдаёт ранее полученный.
		logError = '';
		const result = await refreshLogList();
		if (result.ok) {
			logStatus = result.data;
			localState = 'idle';
			return;
		}
		logError = logErrorText(result.code) || result.message;
		localState = 'error';
	};

	const requestLog = async (file: string) => {
		const processId = Number(file.replace('.CSV', ''));
		if (!Number.isInteger(processId) || processId <= 0) return;

		cancelLocalRequest();
		const requestId = ++logRequestId;
		logAbort = new AbortController();
		const signal = logAbort.signal;

		logError = '';
		logStatus = null;
		localState = 'starting';

		try {
			const start = await startLogDownload(processId, signal);
			if (requestId !== logRequestId) return;

			if (start.kind === 'error') {
				logError = logErrorText(start.code) || start.message;
				localState = 'error';
				return;
			}

			if (start.kind === 'ready') {
				localState = 'downloading';
				saveBlob(start.blob, file);
				localState = 'done';
				return;
			}

			logStatus = start.status;
			localState = 'waiting';

			// Опрос идёт, пока приходит прогресс: фиксированного лимита попыток нет,
			// поэтому длинная передача не обрывается на 30-м опросе.
			let lastProgressAt = Date.now();
			let lastStatus = start.status?.status ?? '';
			let lastReceived = start.status?.received ?? 0;

			for (;;) {
				await new Promise((resolve) => setTimeout(resolve, 500));
				if (requestId !== logRequestId) return;

				// statusOnly: опрос статуса не запускает LIST как побочный эффект.
				const result = await fetchLogStatus(signal, { statusOnly: true });
				if (requestId !== logRequestId) return;

				if (!result.ok) {
					logError = logErrorText(result.code) || result.message;
					localState = 'error';
					return;
				}

				logStatus = result.data;
				if (result.data.status !== lastStatus || result.data.received !== lastReceived) {
					lastStatus = result.data.status;
					lastReceived = result.data.received;
					lastProgressAt = Date.now();
				}

				if (result.data.status === 'completed') {
					localState = 'downloading';
					const blob = await downloadLog(processId, signal);
					if (requestId !== logRequestId) return;
					saveBlob(blob, file);
					localState = 'done';
					return;
				}

				if (result.data.status === 'error') {
					logError = logErrorText(result.data.error) || 'Ошибка передачи журнала';
					localState = 'error';
					return;
				}

				if (Date.now() - lastProgressAt > LOG_IDLE_TIMEOUT_MS) {
					logError = logErrorText('timeout') || 'Превышено время ожидания передачи журнала';
					localState = 'error';
					return;
				}
			}
		} catch (error) {
			if (requestId !== logRequestId) return;
			logError = error instanceof Error ? error.message : 'Ошибка загрузки журнала';
			localState = 'error';
		} finally {
			if (requestId === logRequestId) logAbort = null;
		}
	};

	const cancelLog = async () => {
		cancelLocalRequest();
		localState = 'cancelled';
		logStatus = null;
		try {
			const ok = await cancelLogDownload();
			if (!ok) {
				logError = 'Не удалось отменить передачу на контроллере';
			}
		} catch (error) {
			logError = error instanceof Error ? error.message : 'Не удалось отменить загрузку';
		}
	};

	// Уход со страницы прерывает локальные запросы: поздний ответ не сохранит файл.
	$effect(() => () => {
		cancelLocalRequest();
	});

	$effect(() => {
		getInfo()
			.then((info) => {
				apiFeatures = info?.oc.api_features ?? [];
			})
			.catch(() => {
				apiFeatures = [];
			});
	});
</script>

<div class="logs-page">
	<div class="glassmorphism panel logs-panel">
		<div class="logs-header">
			<h3 class="section-title settings-section-title">Журналы ректификации</h3>
			{#if localState === 'starting' || localState === 'waiting'}
				<button class="log-button" type="button" onclick={cancelLog}>Отменить</button>
			{:else}
				<button class="log-button" type="button" onclick={loadLogs} disabled={!logsSupported}
					>Обновить</button
				>
			{/if}
		</div>
		{#if !logsSupported}
			<p class="log-hint">Для журналов требуется версия API SSVC {logsMinApi} или новее.</p>
		{/if}
		{#if localState === 'waiting' || localState === 'downloading'}
			<p>Загрузка: {logStatus?.received ?? 0} / {logStatus?.total ?? 0}</p>
		{:else if localState === 'starting'}
			<p>Запрос журнала…</p>
		{:else if localState === 'done'}
			<p>Журнал сохранён.</p>
		{:else if localState === 'cancelled'}
			<p>Загрузка отменена.</p>
		{:else if localState === 'error' || logError || logStatus?.status === 'error'}
			<p class="log-error"
				>{logError || logErrorText(logStatus?.error) || 'Ошибка передачи журнала'}</p
			>
		{:else if logStatus?.files}
			<div class="logs-list">
				{#each logStatus.files as file}
					<button class="log-button" type="button" onclick={() => requestLog(file)}
						>{file}</button
					>
				{/each}
			</div>
		{:else}
			<p>Нажмите «Обновить», чтобы получить список журналов.</p>
		{/if}
	</div>
</div>

<style lang="scss">
	@use '$lib/styles/base/variables' as v;
	@use '$lib/styles/base/mixins' as *;

	.logs-page {
		padding: 1rem;

		.glassmorphism {
			@include glassmorphism;
			padding: 1rem;
			box-sizing: border-box;
		}
	}

	.logs-panel {
		.logs-header {
			display: flex;
			align-items: center;
			justify-content: space-between;
			gap: 1rem;
			flex-wrap: wrap;
		}

		.log-hint {
			font-size: 0.85rem;
			opacity: 0.75;
			margin: 0.25rem 0;
		}

		.log-error {
			color: var(--error-color, #c62828);
			font-weight: 600;
		}

		.logs-list {
			display: flex;
			flex-wrap: wrap;
			gap: 0.5rem;
			margin-top: 0.5rem;
		}

		.log-button:disabled {
			opacity: 0.5;
			cursor: not-allowed;
		}
	}
</style>
