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
	import Download from '~icons/tabler/download';

	// Локальное состояние операции отделено от backend-снапшота: устаревший
	// «receiving» с контроллера не должен перекрывать уже показанную ошибку.
	type LocalLogState = 'idle' | 'starting' | 'waiting' | 'downloading' | 'done' | 'error' | 'cancelled';

	let logStatus = $state<SsvcLogStatus | null>(null);
	let logError = $state('');
	let localState = $state<LocalLogState>('idle');
	// Снапшот возможностей устройства из /rest/oc/info (см. SsvcApiCapabilities).
	let apiFeatures = $state<OcApiFeature[]>([]);
	/** Возможности устройства прочитаны (или запрос упал): только после этого решаем про автозагрузку. */
	let featuresLoaded = $state(false);
	/** Автозагрузка списка выполняется один раз при входе на страницу. */
	let autoLoadDone = false;

	// Список и передача файла — разные операции, поэтому индикаторы прогресса не общие:
	// «идёт получение списка» и «скачивается файл 72.CSV» должны читаться по-разному.
	let listLoading = $state(false);
	let activeFile = $state<string | null>(null);
	/** Передачу можно прервать: в этот момент кнопка меняется на «Отменить». */
	const transferInProgress = $derived(localState === 'starting' || localState === 'waiting');

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
		cancelLocalRequest();
		const requestId = ++logRequestId;
		logAbort = new AbortController();
		const signal = logAbort.signal;

		// Явное обновление: refresh=1 действительно запрашивает список заново,
		// а не отдаёт ранее полученный.
		logError = '';
		logStatus = null;
		localState = 'starting';
		listLoading = true;

		try {
			const result = await refreshLogList(signal);
			if (requestId !== logRequestId) return;

			if (!result.ok) {
				logError = logErrorText(result.code) || result.message;
				localState = 'error';
				return;
			}

			logStatus = result.data;
			if (result.data.status === 'error') {
				logError = logErrorText(result.data.error) || 'Ошибка получения списка журналов';
				localState = 'error';
				return;
			}
			if (result.data.status !== 'receiving') {
				localState = 'idle';
				return;
			}

			// Список ещё запрашивается (backend ответил 202): ждём LIST/ERROR,
			// иначе поздно пришедший список не отобразится без повторного клика.
			localState = 'waiting';
			const listDeadlineAt = Date.now() + LOG_IDLE_TIMEOUT_MS;

			for (;;) {
				await new Promise((resolve) => setTimeout(resolve, 500));
				if (requestId !== logRequestId) return;

				const status = await fetchLogStatus(signal, { statusOnly: true });
				if (requestId !== logRequestId) return;

				if (!status.ok) {
					logError = logErrorText(status.code) || status.message;
					localState = 'error';
					return;
				}

				logStatus = status.data;
				if (status.data.status === 'error') {
					logError = logErrorText(status.data.error) || 'Ошибка получения списка журналов';
					localState = 'error';
					return;
				}
				if (status.data.status !== 'receiving') {
					localState = 'idle';
					return;
				}
				if (Date.now() > listDeadlineAt) {
					logError = logErrorText('timeout') || 'Превышено время ожидания списка журналов';
					localState = 'error';
					return;
				}
			}
		} catch (error) {
			if (requestId !== logRequestId) return;
			logError = error instanceof Error ? error.message : 'Ошибка получения списка журналов';
			localState = 'error';
		} finally {
			if (requestId === logRequestId) {
				logAbort = null;
				listLoading = false;
			}
		}
	};

	const requestLog = async (file: string) => {
		const processId = Number(file.replace('.CSV', ''));
		if (!Number.isInteger(processId) || processId <= 0) return;

		cancelLocalRequest();
		const requestId = ++logRequestId;
		logAbort = new AbortController();
		const signal = logAbort.signal;

		logError = '';
		localState = 'starting';
		activeFile = file;

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
			if (requestId === logRequestId) {
				logAbort = null;
				activeFile = null;
			}
		}
	};

	const cancelLog = async () => {
		cancelLocalRequest();
		localState = 'cancelled';
		logStatus = null;
		listLoading = false;
		activeFile = null;
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
			})
			.finally(() => {
				featuresLoaded = true;
			});
	});

	// Список журналов нужен сразу при входе на страницу, а не после клика:
	// ждём только снапшот возможностей, чтобы не дёргать неподдерживаемый API.
	$effect(() => {
		if (autoLoadDone || !featuresLoaded || !logsSupported) return;
		autoLoadDone = true;
		void loadLogs();
	});
</script>

<div class="logs-page">
	<div class="glassmorphism panel logs-panel">
		<div class="logs-header">
			<h3 class="section-title settings-section-title">Журналы ректификации</h3>
			<div class="logs-actions">
				{#if transferInProgress}
					<button class="btn btn-sm btn-outline" type="button" onclick={cancelLog}>Отменить</button>
				{:else}
					<button
						class="btn btn-sm btn-primary"
						type="button"
						onclick={loadLogs}
						disabled={!logsSupported || listLoading}
					>
						{#if listLoading}
							<span class="loading loading-spinner loading-xs" aria-hidden="true"></span>
						{/if}
						Обновить
					</button>
				{/if}
			</div>
		</div>

		{#if !logsSupported}
			<p class="log-hint">Для журналов требуется версия API SSVC {logsMinApi} или новее.</p>
		{/if}

		{#if listLoading}
			<div class="logs-status" role="status" aria-live="polite">
				<span class="loading loading-spinner loading-md text-primary" aria-hidden="true"></span>
				<span>Получение списка журналов…</span>
			</div>
		{:else if localState === 'waiting' || localState === 'downloading'}
			<div class="logs-status" role="status" aria-live="polite">
				<span class="loading loading-spinner loading-md text-primary" aria-hidden="true"></span>
				<span>
					{#if activeFile}
						Загрузка журнала {activeFile}
					{:else}
						Получение списка журналов…
					{/if}
					{#if (logStatus?.total ?? 0) > 0}
						: {logStatus?.received ?? 0} / {logStatus?.total ?? 0}
					{/if}
				</span>
			</div>
			{#if activeFile && (logStatus?.total ?? 0) > 0}
				<progress
					class="progress progress-primary w-full"
					value={logStatus?.received ?? 0}
					max={logStatus?.total ?? 0}
				></progress>
			{/if}
		{:else if localState === 'done'}
			<p class="log-hint">Журнал сохранён.</p>
		{:else if localState === 'cancelled'}
			<p class="log-hint">Загрузка отменена.</p>
		{:else if localState === 'error' || logError || logStatus?.status === 'error'}
			<p class="log-error"
				>{logError || logErrorText(logStatus?.error) || 'Ошибка передачи журнала'}</p
			>
		{:else if logStatus?.files}
			<div class="logs-list-head">
				<span class="logs-count">Доступно журналов: {logStatus.files.length}</span>
				<span class="log-hint">Нажмите на файл, чтобы скачать его</span>
			</div>
			{#if logStatus.files.length === 0}
				<p class="log-hint">На контроллере нет сохранённых журналов.</p>
			{:else}
				<ul class="logs-grid">
					{#each logStatus.files as file (file)}
						<li>
							<button
								class="log-file"
								class:log-file-active={activeFile === file}
								type="button"
								title="Скачать {file}"
								onclick={() => requestLog(file)}
							>
								{#if activeFile === file}
									<span class="loading loading-spinner loading-xs" aria-hidden="true"></span>
								{:else}
									<Download class="h-4 w-4 shrink-0 opacity-70" aria-hidden="true" />
								{/if}
								<span class="log-file-name">{file}</span>
							</button>
						</li>
					{/each}
				</ul>
			{/if}
		{:else}
			<p class="log-hint">Список журналов пока не получен.</p>
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

		.logs-actions {
			display: inline-flex;
			align-items: center;
			gap: 0.5rem;
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

		.logs-status {
			display: flex;
			align-items: center;
			gap: 0.6rem;
			margin: 0.5rem 0;
			font-size: 0.92rem;
		}

		.logs-list-head {
			display: flex;
			align-items: baseline;
			justify-content: space-between;
			gap: 0.75rem;
			flex-wrap: wrap;
			margin-top: 0.5rem;
		}

		.logs-count {
			font-weight: 600;
			font-size: 0.92rem;
		}

		/**
		 * Файлов может быть много (десятки), поэтому список — сетка с переносом,
		 * а не одна строка: колонки подстраиваются под ширину экрана, а длинный
		 * список прокручивается внутри панели и не растягивает страницу.
		 */
		.logs-grid {
			display: grid;
			grid-template-columns: repeat(auto-fill, minmax(9rem, 1fr));
			gap: 0.5rem;
			list-style: none;
			margin: 0.5rem 0 0;
			padding: 0;
			max-height: 22rem;
			overflow-y: auto;
		}

		.log-file {
			display: flex;
			align-items: center;
			gap: 0.5rem;
			width: 100%;
			padding: 0.45rem 0.6rem;
			border: 1px solid var(--border-color, var(--color-base-300));
			border-radius: 0.5rem;
			background: transparent;
			color: inherit;
			text-align: left;
			cursor: pointer;
			transition:
				border-color 0.15s ease,
				background-color 0.15s ease;
		}

		/* Токены daisyUI 5: короткие имена (--p, --b3) в теме не определены. */
		.log-file:hover:not(:disabled) {
			border-color: color-mix(in srgb, var(--color-primary) 60%, transparent);
			background: color-mix(in srgb, var(--color-primary) 8%, transparent);
		}

		.log-file:focus-visible {
			outline: 2px solid var(--color-primary);
			outline-offset: 2px;
		}

		.log-file-active {
			border-color: var(--color-primary);
			background: color-mix(in srgb, var(--color-primary) 12%, transparent);
		}

		.log-file:disabled {
			opacity: 0.5;
			cursor: not-allowed;
		}

		.log-file-name {
			font-weight: 600;
			font-size: 0.9rem;
			font-variant-numeric: tabular-nums;
		}
	}
</style>
