import { page } from '$app/state';
import { user } from '$lib/stores/user';
import { get } from 'svelte/store';
import type { AlarmThresholdsState, TemperatureResponse } from '$lib/types/Sensors';
import type {
	OcHardwareConfigResponse,
	SendCommandResponse,
	RelayRuleMetadata,
	SsvcOpenConnectInfo,
	SsvcOpenConnectMessage,
	SsvcLogStatus,
	SsvcSettings,
	SubsystemsState,
	TelegramConfig,
	VkConfig,
	VkPeersResponse
} from '$lib/types/ssvc';


// ======================== API-функции ======================== //

/**
 * Получение настроек SSVC
 */
export async function fetchSettings(): Promise<SsvcSettings | undefined> {
	const response = await apiFetch<{ ssvcSettings: SsvcSettings }>('/rest/settings');
	return response.success ? response.data.ssvcSettings : undefined;
}

let infoPromise: Promise<SsvcOpenConnectInfo | undefined> | null = null;

/**
 * Получение информации о версии SSVC и OpenConnect SsvcOpenConnectInfo с кешированием.
 */
export function getInfo(): Promise<SsvcOpenConnectInfo | undefined> {
	if (!infoPromise) {
		infoPromise = (async () => {
			try {
				const response = await apiFetch<SsvcOpenConnectInfo>('/rest/oc/info');
				if (response.success) {
					return response.data;
				}
				// Сбрасываем промис в случае ошибки, чтобы разрешить повторную попытку
				infoPromise = null;
				return undefined;
			} catch (error) {
				// Также сбрасываем при исключении
				infoPromise = null;
				throw error;
			}
		})();
	}
	return infoPromise;
}

/**
 * Сброс кеша getInfo().
 *
 * Нужен при переподключении к контроллеру: версия SSVC и набор доступных
 * возможностей API могли измениться (например, контроллер заменили или
 * обновили прошивку), поэтому закешированный ответ больше не актуален.
 */
export function resetInfoCache(): void {
	infoPromise = null;
}

/**
 * Обновление отдельного поля в настройках
 */
export async function updateSetting(field: string, value: unknown): Promise<boolean> {
	const url = `/rest/settings`;
	const body = { [field]: value };
	console.log(body)
	const response = await apiFetch<unknown>(url, 'PUT', body);
	return response.success;
}

/**
 * Сохранение всех настроек SSVC на сервере
 */
export async function saveSettings(settings: SsvcSettings | undefined): Promise<boolean> {
	console.log('Attempting to save settings:', settings);
	const url = '/rest/settings';
	console.log(`Sending PUT request to ${url}`);

	const response = await apiFetch<unknown>(url, 'PUT', settings);

	console.log('saveSettings response:', response);

	if (response.success) {
		console.log('Settings saved successfully.');
	} else {
		console.error('Failed to save settings:', response.error);
	}

	return response.success;
}

/**
 * Получение температур датчиков по зонам
 */
export async function fetchSensorsTemperatureByZone(): Promise<TemperatureResponse | null> {
	const response = await apiFetch<TemperatureResponse>('/rest/sensor');
	return response.success ? response.data : null;
}

/**
 * Обновление зоны датчика
 */
export async function updateSensorZone(address: string, zone: string): Promise<boolean> {
	const url = `/rest/sensors/zone?address=${encodeURIComponent(address)}&zone=${encodeURIComponent(zone)}`;
	const response = await apiFetch<unknown>(url, 'PUT');
	return response.success;
}

/**
 * Обновление порогов датчика
 */
export async function updateSensorThresholds(
	address: string,
	warning: number,
	critical: number,
): Promise<boolean> {
	const url = `/rest/sensors/thresholds?address=${encodeURIComponent(address)}&warning=${warning}&critical=${critical}`;
	const response = await apiFetch<unknown>(url, 'PUT');
	return response.success;
}

/**
 * Получение настроек Telegram (токен и chat ID)
 */
export async function getTelegramSettings(): Promise< TelegramConfig| null> {
	const url = `/rest/telegram/config`;
	const response = await apiFetch<TelegramConfig>(url, 'GET');
	if (!response.success || !response.success) {
		console.error('Ошибка загрузки настроек Telegram');
		return null;
	}
	return response.data;
}

/**
 * Сохранение настроек Telegram
 * @param settings Объект с настройками { botToken: string; chatId: string }
 * @returns true при успешном сохранении, false при ошибке
 */
export async function saveTelegramSettings(settings: TelegramConfig): Promise<boolean> {
	// Отправляем оба параметра в одном запросе
	const response = await apiFetch<{ success: boolean }>(
		'/rest/telegram/config',
		'PUT',
		{
			token: settings.token,
			chat_id: settings.chat_id
		}
	);

	if (!response.success) {
		console.error('Ошибка сохранения настроек Telegram:', response.error);
		return false;
	}

	return true;
}

export async function getVkSettings(): Promise<VkConfig | null> {
	const response = await apiFetch<VkConfig>('/rest/vk/config', 'GET');
	if (!response.success) {
		return null;
	}
	return response.data;
}

export async function saveVkSettings(settings: VkConfig): Promise<boolean> {
	const response = await apiFetch<{ result?: boolean }>('/rest/vk/config', 'PUT', settings);
	if (!response.success) {
		console.error('Ошибка сохранения VK:', response.error);
		return false;
	}
	return true;
}

/** Recent VK dialog peer_id list (messages.getConversations). Uses saved token unless overridden. */
export async function fetchVkConversationPeers(
	access_token?: string,
	api_version?: string
): Promise<VkPeersResponse | null> {
	const body: Record<string, string> = {};
	if (access_token) {
		body.access_token = access_token;
	}
	if (api_version) {
		body.api_version = api_version;
	}
	const response = await apiFetch<VkPeersResponse>('/rest/vk/peers', 'POST', body);
	if (!response.success) {
		return null;
	}
	return response.data;
}

export async function setSubsystemState(states: Record<string, boolean>): Promise<boolean> {
	const response = await apiFetch<{ success: boolean }>(
		'/rest/subsystem',
		'PUT',
		states
	);
	// Backend replies HTTP 200 with { success, errors } even when a subsystem failed to
	// change, so response.data is the whole body, not a boolean. Check data.success
	// explicitly so a real toggle failure is never masked as success.
	return response.success ? response.data.success === true : false;
}

export async function getSubsystemState(): Promise<SubsystemsState | null> {
	const response = await apiFetch<SubsystemsState>(
		'/rest/subsystem',
		'GET'
	);
	return response.success ? response.data : null;
}

export async function fetchHardwareConfig(): Promise<OcHardwareConfigResponse | null> {
	const response = await apiFetch<OcHardwareConfigResponse>('/rest/oc/hardware-config', 'GET');
	return response.success ? response.data : null;
}

export async function putHardwareConfig(
	partial: Partial<
		Omit<OcHardwareConfigResponse, 'capabilities' | 'needsReboot' | 'schemaVersion'>
	>
): Promise<OcHardwareConfigResponse | null> {
	const response = await apiFetch<OcHardwareConfigResponse>(
		'/rest/oc/hardware-config',
		'PUT',
		partial
	);
	return response.success ? response.data : null;
}

/**
 * Отправка команды на сервер
 */
/**
 * Отправка команды на сервер
 */
export async function sendCommand(command: string): Promise<SendCommandResponse> {
	const response = await apiFetch<SendCommandResponse>(
		'/rest/commands',
		'POST',
		{ commands: command }
	);

	if (!response.success) {
		console.error('Ошибка выполнения команды:', response.error);
	}

	return response;
}

// ======================== Телеметрия ======================== //

/**
 * Получение полной телеметрии
 */
export async function fetchTelemetry(): Promise<SsvcOpenConnectMessage | null> {
	const response = await apiFetch<SsvcOpenConnectMessage>('/rest/telemetry', 'GET');
	if (!response.success) {
		console.error('Ошибка при получении телеметрии:', response.error);
	}
	return response.success ? response.data : null;
}

export async function fetchRelayRuleMetadata(): Promise<RelayRuleMetadata | null> {
	const response = await apiFetch<RelayRuleMetadata>('/rest/oc/relay/metadata', 'GET');
	return response.success ? response.data : null;
}


/**
 * Отправка команды с автоматическим обновлением статуса
 */
export async function sendTelemetryCommand(command: string): Promise<ApiResponse<unknown>> {

	return await apiFetch<unknown>('/rest/commands', 'POST', { commands: command });
}

/** Результат запроса статуса журнала: данные контроллера или сохранённая причина отказа. */
export type LogStatusResult =
	| { ok: true; data: SsvcLogStatus }
	| { ok: false; httpStatus: number | null; code: string; message: string };

/** Результат старта загрузки: 202 (запущено), 200 text/csv (готов сразу) или ошибка. */
export type LogDownloadStart =
	| { kind: 'accepted'; status: SsvcLogStatus | null }
	| { kind: 'ready'; blob: Blob }
	| { kind: 'error'; httpStatus: number | null; code: string; message: string };

// Таймауты GET_LOG-пути: статус отвечает быстро, тело CSV может идти долго.
// Клиентский idle timeout (LOG_IDLE_TIMEOUT_MS) больше backend-ового TRANSFER_TIMEOUT_MS
// (10 с), чтобы ошибку контроллера успел сообщить сам backend.
const LOG_STATUS_TIMEOUT_MS = 5000;
const LOG_CSV_TIMEOUT_MS = 60000;
export const LOG_IDLE_TIMEOUT_MS = 15000;

const isAbortError = (error: unknown): boolean =>
	error instanceof Error && (error.name === 'AbortError' || error.name === 'TimeoutError');

/**
 * fetch с собственным таймаутом и поддержкой внешнего AbortSignal.
 *
 * Общий apiFetch намеренно не меняется: у него другое поведение ошибок,
 * а GET_LOG-путь обязан отличать таймаут от HTTP-отказа.
 */
async function fetchLogResponse(
	url: string,
	init: RequestInit,
	timeoutMs: number,
	signal?: AbortSignal
): Promise<Response> {
	const controller = new AbortController();
	const timer = setTimeout(
		() => controller.abort(new DOMException('timeout', 'TimeoutError')),
		timeoutMs
	);
	const abortFromCaller = () => controller.abort(new DOMException('cancelled', 'AbortError'));
	if (signal) {
		if (signal.aborted) {
			abortFromCaller();
		} else {
			signal.addEventListener('abort', abortFromCaller, { once: true });
		}
	}
	try {
		return await fetch(url, { ...init, signal: controller.signal });
	} finally {
		clearTimeout(timer);
		signal?.removeEventListener('abort', abortFromCaller);
	}
}

/** Код и сообщение отказа backend ({"error":"..."}), иначе HTTP-статус. */
async function readLogErrorPayload(response: Response): Promise<{ code: string; message: string }> {
	try {
		const payload = await response.json();
		if (payload && typeof payload.error === 'string') {
			return { code: payload.error, message: payload.error };
		}
	} catch {
		// Тело не JSON — ограничиваемся HTTP-статусом.
	}
	return { code: '', message: `HTTP ${response.status}: ${response.statusText}` };
}

async function requestLogStatus(url: string, signal?: AbortSignal): Promise<LogStatusResult> {
	try {
		const response = await fetchLogResponse(
			url,
			{ method: 'GET', headers: getAuthHeaders(), cache: 'no-store' },
			LOG_STATUS_TIMEOUT_MS,
			signal
		);
		if (!response.ok) {
			return {
				ok: false,
				httpStatus: response.status,
				...(await readLogErrorPayload(response))
			};
		}
		return { ok: true, data: (await response.json()) as SsvcLogStatus };
	} catch (error) {
		return {
			ok: false,
			httpStatus: null,
			code: isAbortError(error) ? 'aborted' : 'network_error',
			message: isAbortError(error) ? 'Запрос статуса журнала прерван' : 'Нет связи с контроллером'
		};
	}
}

/**
 * Опрос статуса журнала.
 *
 * statusOnly=1 обязателен для polling: без него GET из состояния IDLE сам
 * запускает LIST, и опрос превращается в побочный запрос списка.
 */
export async function fetchLogStatus(
	signal?: AbortSignal,
	options: { statusOnly?: boolean } = {}
): Promise<LogStatusResult> {
	return requestLogStatus(options.statusOnly ? '/rest/logs?statusOnly=1' : '/rest/logs', signal);
}

/**
 * Явное обновление списка журналов: refresh=1 запрашивает LIST заново даже при
 * уже полученном списке. В активной файловой сессии backend отвечает 409.
 */
export async function refreshLogList(signal?: AbortSignal): Promise<LogStatusResult> {
	return requestLogStatus('/rest/logs?refresh=1', signal);
}

/**
 * Первый запрос журнала. Различает готовый CSV (200) и запущенную передачу (202),
 * чтобы готовый файл сохранялся сразу и polling не запускался.
 */
export async function startLogDownload(
	processId: number,
	signal?: AbortSignal
): Promise<LogDownloadStart> {
	try {
		const response = await fetchLogResponse(
			`/rest/logs/${processId}`,
			{ method: 'GET', headers: getAuthHeaders(), cache: 'no-store' },
			LOG_CSV_TIMEOUT_MS,
			signal
		);

		const contentType = response.headers.get('content-type') ?? '';
		if (response.ok && contentType.startsWith('text/csv')) {
			return { kind: 'ready', blob: await response.blob() };
		}

		if (response.status === 202) {
			let status: SsvcLogStatus | null = null;
			try {
				status = (await response.json()) as SsvcLogStatus;
			} catch {
				status = null;
			}
			return { kind: 'accepted', status };
		}

		if (response.ok) {
			return {
				kind: 'error',
				httpStatus: response.status,
				code: '',
				message: 'Контроллер вернул не CSV'
			};
		}

		return {
			kind: 'error',
			httpStatus: response.status,
			...(await readLogErrorPayload(response))
		};
	} catch (error) {
		return {
			kind: 'error',
			httpStatus: null,
			code: isAbortError(error) ? 'aborted' : 'network_error',
			message: isAbortError(error) ? 'Запрос журнала прерван' : 'Нет связи с контроллером'
		};
	}
}

export async function downloadLog(processId: number, signal?: AbortSignal): Promise<Blob> {
	const response = await fetchLogResponse(
		`/rest/logs/${processId}`,
		{ method: 'GET', headers: getAuthHeaders(), cache: 'no-store' },
		LOG_CSV_TIMEOUT_MS,
		signal
	);
	if (!response.ok || response.headers.get('content-type')?.startsWith('text/csv') !== true) {
		throw new Error(`Не удалось скачать журнал: HTTP ${response.status}`);
	}
	return response.blob();
}

/**
 * Отмена текущей передачи журнала (сброс состояния на OpenConnect).
 */
export async function cancelLogDownload(signal?: AbortSignal): Promise<boolean> {
	try {
		const response = await fetchLogResponse(
			'/rest/logs',
			{ method: 'DELETE', headers: getAuthHeaders(), cache: 'no-store' },
			LOG_STATUS_TIMEOUT_MS,
			signal
		);
		return response.ok;
	} catch {
		return false;
	}
}

/**
 * Получение всех настроек порогов тревоги.
 */
export async function fetchAlarmThresholds(): Promise<AlarmThresholdsState | null> {
	const response = await apiFetch<AlarmThresholdsState>('/rest/alarms');
	if (!response.success) {
		console.error('Ошибка при получении порогов:', response.error);
	}
	return response.success ? response.data : null;
}


/**
 * Обновление настроек порогов тревоги (отправка полной структуры).
 * В StatefulService это соответствует методу POST.
 *
 * @param state Полный объект AlarmThresholdsState для отправки.
 */
export async function updateAlarmThresholds(state: AlarmThresholdsState): Promise<boolean> {
	const response = await apiFetch<AlarmThresholdsState>('/rest/alarms', 'POST', state);
	return response.success;
}

// ======================== Базовые функции ======================== //
type ApiResponse<T> = { success: true; data: T } | { success: false; error: string };

/**
 * Возвращает заголовки авторизации
 */
function getAuthHeaders(): HeadersInit {
	const currentUser = get(user);

	const authHeader =
		page.data.features.security && currentUser.bearer_token
			? `Bearer ${currentUser.bearer_token}`
			: 'Basic';

	return {
		Authorization: authHeader,
		'Content-Type': 'application/json',
		Accept: '*/*',
	};
}


/**
 * Универсальный fetch-запрос с обработкой ошибок
 */
export async function apiFetch<T>(url: string, method: string = 'GET', body?: unknown): Promise<any> {
	try {
		const response = await fetch(url, {
			method,
			headers: getAuthHeaders(),
			body: body ? JSON.stringify(body) : undefined,
			// Свежие данные с контроллера (избегаем устаревшего кэша GET после POST)
			cache: method === 'GET' ? 'no-store' : 'default'
		});

		if (!response.ok) {
			return { success: false, error: `HTTP ${response.status}: ${response.statusText}` };
		}

		const text = await response.text();
		const data = text ? JSON.parse(text) : null;

		return { success: true, data };
	} catch (error) {
		console.error(`API error (${method} ${url}):`, error);
		return { success: false, error: error instanceof Error ? error.message : 'Unknown error' };
	}
}
