import { page } from '$app/state';
import { user } from '$lib/stores/user';
import { get } from 'svelte/store';
import type { AlarmThresholdsState, TemperatureResponse, ZoneName } from '$lib/types/Sensors';
import type {
	SendCommandResponse,
	SsvcOpenConnectMessage,
	SsvcSettings,
	SsvcTelemetryMessage,
	SubsystemsState,
	TelegramConfig
} from '$lib/types/ssvc';


// ======================== API-функции ======================== //

/**
 * Получение настроек SSVC
 */
export async function fetchSettings(): Promise<SsvcSettings | null> {
	const response = await apiFetch<{ settings: SsvcSettings }>('/rest/settings');
	return response.success ? response.data.settings : null;
}

/**
 * Обновление отдельного поля в настройках
 */
export async function updateSetting(field: string, value: unknown): Promise<boolean> {
	const url = `/rest/settings`;
	const body = { [field]: value };
	const response = await apiFetch<unknown>(url, 'PUT', body);
	return response.success;
}

/**
 * Сохранение всех настроек SSVC на сервере
 */
export async function saveSettings(settings: SsvcSettings): Promise<boolean> {
	const url = '/rest/settings';
	const response = await apiFetch<unknown>(url, 'PUT', settings);
	console.log('saveSettings response:', response);
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
	return {
		token: response.data.token || '',
		chat_id: response.data.chat_id || '',
	};
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

export async function setSubsystemState(states: Record<string, boolean>): Promise<boolean> {
	const response = await apiFetch<{ success: boolean }>(
		'/rest/subsystem',
		'PUT',
		states
	);
	return true;
	// return response?.success ?? false;
}

export async function getSubsystemState(): Promise<SubsystemsState | null> {
	const response = await apiFetch<SubsystemsState>(
		'/rest/subsystem',
		'GET'
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

/**
 * Получение только статуса
 */
export async function fetchStatus(): Promise<SsvcOpenConnectMessage | null> {
	const response = await apiFetch<SsvcOpenConnectMessage>('/rest/telemetry?mode=status', 'GET');
	if (!response.success) {
		console.error('Ошибка при получении телеметрии:', response.error);
	}
	return response.success ? response.data : null;

}

/**
 * Отправка команды с автоматическим обновлением статуса
 */
export async function sendTelemetryCommand(command: string): Promise<ApiResponse<unknown>> {

	return await apiFetch<unknown>('/rest/commands', 'POST', { commands: command });
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
async function apiFetch<T>(url: string, method: string = 'GET', body?: unknown): Promise<any> {
	try {
		const response = await fetch(url, {
			method,
			headers: getAuthHeaders(),
			body: body ? JSON.stringify(body) : undefined,
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
