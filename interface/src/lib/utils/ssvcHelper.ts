// src/lib/ssvcHelper.ts
import type { commonType, RectificationStatus, SsvcOpenConnectMessage } from '$lib/types/models';
import { get } from 'svelte/store';
import { page } from '$app/stores';
import { user } from '$lib/stores/user';
import {
	ssvcEventStore,
	ssvcTelemetryStore,
	commandState,
	temperatureStore
} from '$lib/stores/ssvcOpenConnect';

import { notifications } from '$lib/components/toasts/notifications'; // Импортируйте тип, если он в отдельном файле
import type { CommandState } from '$lib/types/models'

let isEventSet: boolean = false;

const MAX_ENTRIES = 100; // Максимальное количество записей


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

export function getStageDescription(stage:string): string {
	switch (stage) {
		case "waiting":
			return "Дежурный режим"
		case "tp1_waiting":
			return "Ожидание нагрева колонны"
		case "delayed_start":
			return "Стабилизация колонны"
		case "heads":
			return "Отбор голов"
		case "late_heads":
			return "Отбор подголовников"
		case "hearts":
			return "Отбор тела"
		case "tails":
			return "Отбор хвостов"
		default:
			return `Ошибка получения данных`

	}
}

const timeout_command_response = 5000;

function getSsvsMessageCommandNoResponse(commandName: string) {
	return `SSVC не ответил на команду ${commandName} в течении ${timeout_command_response/1000} секунд`
}

function updateTemperatureData(common: commonType) {
	const currentTime = new Date().toLocaleTimeString();
	temperatureStore.update((store) => {
		return {
			tp1: [...store.tp1, common.tp1].slice(-MAX_ENTRIES), // Оставляем последние 100 записей
			tp2: [...store.tp2, common.tp2].slice(-MAX_ENTRIES),
			time: [...store.time, currentTime ].slice(-MAX_ENTRIES),
		};
	});
}

export function processTelemetryData(data: SsvcOpenConnectMessage): SsvcOpenConnectMessage {
	updateTemperatureData(data.common)
	return  data;
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
		const timeout = startCommandTimeout(commandName, getSsvsMessageCommandNoResponse(commandName));
		manageSubscription(commandState, timeout);
	} else {
		commandState.set({
			isWaiting: false,
			message: 'Ошибка сервера при обработке команды',
			command: commandName
		});
	}
};


// export const sendCommandsRequest = async (commandName: string) => {
// 	const currentPage = get(page); // Извлекаем текущее состояние `page`.
// 	const currentUser = get(user); // Извлекаем текущее состояние `user`.
// 	try {
// 		commandState.set({ isWaiting: true, message: '', command: commandName });
// 		const url = new URL(`/rest/commands`, window.location.origin);  // Указываем полный путь, если нужно
// 		const response = await fetch(url, {
// 			method: 'POST',
// 			headers: {
// 				Authorization: currentPage.data.features.security ? 'Bearer ' + currentUser.bearer_token : 'Basic',
// 				'Content-Type': 'application/json'
// 			},
// 			body: JSON.stringify({ commands: commandName })
// 		});
// 		responseCommandHandeler(response, commandName);
// 		await response.json();
// 	} catch (error) {
// 		console.log('Error:', error);
// 	}
// };

export function formatSecondsToHHMMSS(seconds: number): string {
	const hours = Math.floor(seconds / 3600);
	seconds %= 3600;
	const minutes = Math.floor(seconds / 60);
	const remainingSeconds = seconds % 60;

	if (hours > 0) {
		// Если есть часы, возвращаем полный формат ЧЧ:ММ:СС
		return `${hours}:${minutes.toString().padStart(2, '0')}:${remainingSeconds.toString().padStart(2, '0')}`;
	} else {
		// Если часов нет, возвращаем формат ММ:СС
		return `${minutes}:${remainingSeconds.toString().padStart(2, '0')}`;
	}
}

// export async function postStop() {
// 	const commandName = "STOP"
// 	await sendCommandsRequest(commandName);
// }
//
// export async function postPause() {
// 	const commandName = "PAUSE"
// 	await sendCommandsRequest(commandName);
// }
//
// export async function postResume() {
// 	const commandName = "RESUME"
// 	await sendCommandsRequest(commandName);
// }
//
// export async function postNext() {
// 	const commandName = "NEXT"
// 	await sendCommandsRequest(commandName);
// }
//
// export async function postVersion() {
// 	const commandName = "VERSION"
// 	await sendCommandsRequest(commandName);
// }
//
// export async function sendCommandGetSettings() {
// 	const commandName = "GET_SETTINGS"
// 	await sendCommandsRequest(commandName);
// }
