import { page } from '$app/state';
import { user } from '$lib/stores/user';
import { get } from 'svelte/store';
import type { SensorsData, ZoneName } from '$lib/types/OCSettings';
import type { SsvcSettings } from "$lib/types/models"
import type { TelegramConfig, SubsystemsState } from '$lib/types/api';

// ======================== Базовые утилиты ======================== //
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
async function apiFetch<T>(url: string, method: string = 'GET', body?: unknown): Promise<ApiResponse<T>> {
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
	const formattedValue = Array.isArray(value) ? `[${value.join(',')}]` : String(value);
	const url = `/rest/settings?${field}=${encodeURIComponent(formattedValue)}`;
	const response = await apiFetch<unknown>(url, 'PUT');
	return response.success;
}

/**
 * Получение термодатчиков по зонам
 */
export async function fetchSensorsByZone(): Promise<Record<ZoneName, string[]> | null> {
	const response = await apiFetch<{ zones: Record<ZoneName, string[]> }>('/rest/sensors/zone');
	return response.success ? response.data.zones : null;
}

/**
 * Получение температур датчиков по зонам
 */
export async function fetchSensorsTemperatureByZone(): Promise<SensorsData | null> {
	const response = await apiFetch<SensorsData>('/rest/sensors/temperatures');
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
		'/rest/subsystem/state',
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