// src/lib/ssvcHelper.ts
import type { SsvcOpenConnectMessage } from '$lib/types/models';
import { get } from 'svelte/store';
import { page } from '$app/stores';
import { user } from '$lib/stores/user';
import {
	ssvcEventStore,
	ssvcSettingsStore,
	ssvcTelemetryStore,
	commandState,
	ssvcCurrentVersionStore
} from '$lib/stores/ssvcOpenConnect';
import { notifications } from '$lib/components/toasts/notifications'; // Импортируйте тип, если он в отдельном файле
import type { CommandState } from '$lib/types/models'

export let ssvcStopStatus: SsvcOpenConnectMessage;
export let ssvcPauseStatus: SsvcOpenConnectMessage;
export let ssvcResumeStatus: SsvcOpenConnectMessage;
export let ssvcVersionStatus: SsvcOpenConnectMessage;
export let ssvcSystemSettingStatus: SsvcOpenConnectMessage;

let isEventSet: boolean = false;

export function processIncomingDataHandler(data: SsvcOpenConnectMessage) {
	if (data.type === 'response') {
		// TODO Тут по идее нужно проверять, что ответ пришел на ту команду которую мы ждем..
		let oldState: CommandState;

		// Подписка на хранилище для получения текущего значения перед изменением
		const unsubscribe = commandState.subscribe((currentState) => {
			oldState = currentState; // Сохраняем текущее значение в oldState
			// Если мы ждали команду и пришел ответ от SSVC, то стоит об этом сообщить
			if (oldState.isWaiting && oldState.command === currentState.command) {
				notifications.info(`Ответ на команду ${currentState.command} получен`, 5000);
			}
		});
		unsubscribe();

		if ( data.request) {
			commandState.set(
				{
					isWaiting: false,
					message: "",
					command: data.request
				}
			);
			if (data.request == "GET_SETTINGS") {
				if (data.settings) {
					ssvcSettingsStore.set(data.settings);
				}
			}else if (data.request == "VERSION" && data.version) {
				ssvcCurrentVersionStore.set(data.version)
			}
		}

	} else {
		ssvcTelemetryStore.set(data);
	}
	if (data.event) {
		ssvcEventStore.set(data.event)
		isEventSet = true
	}
	if (isEventSet && !data.event) {
		isEventSet = false
		ssvcEventStore.set("")
	}
}

export function processTelemetryData(data: SsvcOpenConnectMessage): SsvcOpenConnectMessage {
	return  data;
}

export function getStageDescription(stage:any): any {
	switch (stage) {
		case "waiting":
			return "Дежурный режим"
		case "tp1_waiting":
			return "Ожидание нагрева колонны"
		case "delayed_start":
			return "Ожидание нагрева колонны"
		case "heads":
			return "Отбор голов"
		case "hearts":
			return "Отбор тела"
		case "tails":
			return "Отбор хвостов"
		default:
			return `Не известный режим: ${stage}`

	}
}

export function countdownToSeconds(countdown: string): number {
	const [hours, minutes, seconds] = countdown.split(":").map(Number);
	return hours * 3600 + minutes * 60 + seconds;
}

export function getDescriptionEvent<T extends string | null>(event: T): { description: string; errorCode: number } {
	let errorCode = 0; // Код ошибки по умолчанию

	const description = (() => {
		switch (event) {
			case "heads_finished":
				return "Завершен этап Головы";
			case "hearts_finished":
				return "Завершен этап Тело";
			case "tails_finished":
				return "Завершен этап Хвосты";
			case "ds_error":
				errorCode = 1;
				return "Ошибка датчика температуры";
			case "ds_error_stop":
				errorCode = 1;
				return "Выключение оборудования (реле) из-за ошибки датчика";
			case "stabilization_limit":
				errorCode = 1;
				return "Превышен лимит времени стабилизации";
			case "remote_stop":
				errorCode = 2;
				return "Получена удаленная команда остановки, процесс остановлен";
			case "manually_closed":
				errorCode = 2;
				return "Включено ручное управление клапаном текущего этапа, клапан закрыт";
			case "manually_opened":
				errorCode = 2;
				return "Включено ручное управление клапаном текущего этапа, клапан открыт";
			default:
				errorCode = 1;
				return `Неизвестная команда ${event}`;
		}
	})();

	return { description, errorCode };
}

let timeout_command_response = 5000;

function getSsvsMessageCommandNoResponse(commandName: string) {
	return `SSVC не ответил на команду ${commandName} в течении ${timeout_command_response/1000} секунд`
}

function startCommandTimeout(commandName: string, errorMessage: string) {
	return setTimeout(() => {
		commandState.set({
			isWaiting: false,
			command: commandName,
			message: errorMessage,
		});
	}, timeout_command_response);
}

function manageSubscription(stateStore: typeof commandState, timeoutId: ReturnType<typeof setTimeout>) {
	const unsubscribe = stateStore.subscribe((state) => {
		if (!state.isWaiting) {
			clearTimeout(timeoutId); // Останавливаем таймер
			// Отписываемся с небольшим отложением, чтобы избежать ошибки
			setTimeout(() => {
				unsubscribe(); // Отписка, отложенная во времени
			}, 0);
		}
	});
}

const responseCommandHandeler = (response: Response, commandName: string) => {
	if (response.status === 200) {
		let timeout = startCommandTimeout(commandName, getSsvsMessageCommandNoResponse(commandName));
		manageSubscription(commandState, timeout);
	} else {
		commandState.set({
			isWaiting: false,
			message: 'Ошибка сервера при обработке команды',
			command: commandName
		});
	}
};

export const sendPostRequest = async (commandName: string, responseData: SsvcOpenConnectMessage) => {
	const currentPage = get(page); // Извлекаем текущее состояние `page`.
	const currentUser = get(user); // Извлекаем текущее состояние `user`.
	try {
		commandState.set({ isWaiting: true, message: '', command: commandName });
		const url = new URL(`/rest/openConnect`, window.location.origin);  // Указываем полный путь, если нужно
		const response = await fetch(url, {
			method: 'POST',
			headers: {
				Authorization: currentPage.data.features.security ? 'Bearer ' + currentUser.bearer_token : 'Basic',
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ commands: commandName })
		});
		responseCommandHandeler(response, commandName);
		responseData = await response.json();
	} catch (error) {
		console.log('Error:', error);
	}
};

export async function postStop() {
	let commandName = "STOP"
	await sendPostRequest(commandName, ssvcStopStatus);
}

export async function postPause() {
	let commandName = "PAUSE"
	await sendPostRequest(commandName, ssvcVersionStatus);
}

export async function postResume() {
	let commandName = "RESUME"
	await sendPostRequest(commandName, ssvcPauseStatus);
}

export async function postVersion() {
	let commandName = "VERSION"
	await sendPostRequest(commandName, ssvcResumeStatus);
}

export async function getSystemSetting() {
	let commandName = "GET_SETTINGS"
	await sendPostRequest(commandName, ssvcSystemSettingStatus);
}

export function isVersionSupported(currentVersion: string, minSupportedVersion: string): boolean {
	// Функция для разбора версии на числа
	const parseVersion = (version: string) => version.split('.').map(Number);

	// Разбиваем версии на массивы чисел
	const current = parseVersion(currentVersion);
	const minSupported = parseVersion(minSupportedVersion);

	// Сравниваем каждую часть версии по порядку (основная версия, минорная версия, патч)
	for (let i = 0; i < Math.max(current.length, minSupported.length); i++) {
		const currentPart = current[i] || 0;  // Если части не хватает, считаем ее 0
		const minPart = minSupported[i] || 0;

		if (currentPart < minPart) {
			return false;  // Текущая версия меньше, чем минимальная поддерживаемая
		} else if (currentPart > minPart) {
			return true;  // Текущая версия больше или равна минимальной поддерживаемой
		}
	}

	return true; // Версии равны
}