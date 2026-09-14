<script lang="ts">
	import {
		fetchLogStatus,
		startLogDownload,
		downloadLog,
		cancelLogDownload,
		getInfo
	} from '$lib/api/ssvcApi';
	import type { SsvcLogStatus, OcApiFeature } from '$lib/types/ssvc';

	let logStatus = $state<SsvcLogStatus | null>(null);
	let logError = $state('');
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

	const loadLogs = async () => {
		try {
			logStatus = await fetchLogStatus();
		} catch (error) {
			logError = error instanceof Error ? error.message : 'Ошибка получения списка журналов';
		}
	};

	const requestLog = async (file: string) => {
		const processId = Number(file.replace('.CSV', ''));
		if (!Number.isInteger(processId) || processId <= 0) return;
		const requestId = ++logRequestId;
		logError = '';
		try {
			if (!(await startLogDownload(processId))) {
				logError = 'Не удалось начать загрузку журнала';
				return;
			}
			for (let attempt = 0; attempt < 30; attempt += 1) {
				await new Promise((resolve) => setTimeout(resolve, 500));
				if (requestId !== logRequestId) return; // загрузка отменена пользователем
				logStatus = await fetchLogStatus();
				if (logStatus?.status === 'completed') {
					const blob = await downloadLog(processId);
					const url = URL.createObjectURL(blob);
					const anchor = document.createElement('a');
					anchor.href = url;
					anchor.download = file;
					anchor.click();
					URL.revokeObjectURL(url);
					return;
				}
				if (logStatus?.status === 'error') {
					logError = logErrorText(logStatus.error) || 'Ошибка передачи журнала';
					return;
				}
			}
			logError = 'Истекло время ожидания журнала';
		} catch (error) {
			logError = error instanceof Error ? error.message : 'Ошибка загрузки журнала';
		}
	};

	const cancelLog = async () => {
		logRequestId += 1;
		try {
			await cancelLogDownload();
			logStatus = await fetchLogStatus();
		} catch (error) {
			logError = error instanceof Error ? error.message : 'Не удалось отменить загрузку';
		}
	};

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
			{#if logStatus?.status === 'receiving'}
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
		{#if logStatus?.status === 'receiving'}
			<p>Загрузка: {logStatus.received} / {logStatus.total}</p>
		{:else if logStatus?.status === 'error' || logError}
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
