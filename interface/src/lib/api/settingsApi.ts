import { page } from '$app/state';
import { user } from '$lib/stores/user';
import { get } from 'svelte/store';
import type { SsvcSettings } from '$lib/types/models';
import type { SensorsData, ZoneName } from '$lib/types/OCSettings';

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
		Accept: '*/*'
	};
}

/**
 * Универсальный fetch-запрос с поддержкой GET
 */
async function apiFetch<T>(url: string, method: string = 'GET'): Promise<T | null> {
	try {
		const response = await fetch(url, {
			method,
			headers: getAuthHeaders()
		});

		const text = await response.text();
		if (!text) return null;

		return JSON.parse(text);
	} catch (error) {
		console.error(`API error on ${method} ${url}:`, error);
		return null;
	}
}

/**
 * Получение настроек SSVC
 */
export async function fetchSettings(): Promise<SsvcSettings | null> {
	const data = await apiFetch<{ settings: SsvcSettings }>('/rest/settings');
	return data?.settings || null;
}

/**
 * Обновление отдельного поля в настройках
 */
export async function updateSetting(field: string, value: unknown): Promise<boolean> {
	try {
		const formattedValue = Array.isArray(value)
			? `[${value.join(',')}]`
			: encodeURIComponent(String(value));

		const url = `/rest/settings?${field}=${formattedValue}`;

		const response = await fetch(url, {
			method: 'PUT',
			headers: getAuthHeaders()
		});

		return response.ok;
	} catch (error) {
		console.error('Ошибка сохранения:', error);
		return false;
	}
}

/**
 * Получение термодатчиков по зонам
 */

export async function fetchSensorsByZone(): Promise<Record<ZoneName, string[]> | null> {
	try {
		const response = await fetch('/rest/sensors/zone');
		const data = await response.json();
		console.log('API Response:', data);
		return data?.zones || null;
	} catch (err) {
		console.error('Fetch error:', err);
		return null;
	}
}

export async function fetchSensorsTemperatureByZone(): Promise<SensorsData | undefined> {
	try {
		const response = await fetch('/rest/sensors/temperatures');
		const data = await response.json();
		return data || null;
	} catch (err) {
		console.error('Fetch error:', err);
		return undefined;
	}
}

/**
 * Update sensor zone
 * @param address Sensor address
 * @param zone New zone
 */
export async function updateSensorZone(address: string, zone: string): Promise<boolean> {
	try {
		const url = `/rest/sensors/zone?address=${encodeURIComponent(address)}&zone=${encodeURIComponent(zone)}`;
		const response = await fetch(url, {
			method: 'PUT',
			headers: getAuthHeaders()
		});

		if (!response.ok) {
			throw new Error(`HTTP error! status: ${response.status}`);
		}

		return true;
	} catch (error) {
		console.error('Error updating sensor zone:', error);
		return false;
	}
}

/**
 * Update sensor thresholds
 * @param address Sensor address
 * @param warning Warning threshold temperature
 * @param critical Critical threshold temperature
 */
export async function updateSensorThresholds(
	address: string,
	warning: number,
	critical: number
): Promise<boolean> {
	try {
		const url = `/rest/sensors/thresholds?address=${encodeURIComponent(address)}&warning=${warning}&critical=${critical}`;
		const response = await fetch(url, {
			method: 'PUT',
			headers: getAuthHeaders()
		});

		return response.ok;
	} catch (error) {
		console.error('Error updating sensor thresholds:', error);
		return false;
	}
}

export const getTelegramSettings = async (): Promise<{ botToken: string; chatId: string }> => {
	try {
		const [tokenResponse, chatIdResponse] = await Promise.all([
			fetch(`/rest/telegram/token`),
			fetch(`/rest/telegram/chat_id`)
		]);

		if (!tokenResponse.ok || !chatIdResponse.ok) {
			throw new Error('Не удалось загрузить настройки Telegram');
		}

		const tokenData = await tokenResponse.json();
		const chatIdData = await chatIdResponse.json();

		return {
			botToken: tokenData.token || '',
			chatId: chatIdData.chat_id || ''
		};
	} catch (err) {
		console.error('Ошибка загрузки настроек Telegram:', err);
		throw new Error('Ошибка соединения с сервером');
	}
};

export const saveTelegramSettings = async (settings: { botToken: string; chatId: string }) => {
	try {
		const [tokenResponse, chatIdResponse] = await Promise.all([
			fetch(`/rest/telegram/token`, {
				method: 'PUT',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ token: settings.botToken })
			}),
			fetch(`/rest/telegram/chat_id`, {
				method: 'PUT',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ chat_id: settings.chatId })
			})
		]);

		if (!tokenResponse.ok || !chatIdResponse.ok) {
			throw new Error('Не удалось сохранить настройки Telegram');
		}
	} catch (err) {
		console.error('Ошибка сохранения настроек Telegram:', err);
		throw new Error('Ошибка соединения с сервером');
	}
};
