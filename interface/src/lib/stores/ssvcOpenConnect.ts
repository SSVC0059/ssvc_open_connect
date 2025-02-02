
import type {RectificationStatus, CommandState, SsvcOpenConnectMessage} from '$lib/types/models';
import { type Writable, writable } from 'svelte/store';
import { processIncomingDataHandler } from '$lib/utils/ssvcHelper';

export const ssvcTelemetryStore = writable<SsvcOpenConnectMessage | null>(null);
export const rectificationStatusStore = writable<RectificationStatus | null>(null);
export const ssvcEventStore = writable<string | null>(null);
export const needUpdateRectificationStatusStore = writable<boolean>(false) ;


export const commandState: Writable<CommandState> = writable({
    isWaiting: false,
    message: "",
    command: ""
});

export const temperatureStore = writable({
    tp1: [] as number[], // массив данных для tp1
    tp2: [] as number[], // массив данных для tp2
    time: [] as string[], // временные метки
});

// Функция для обработки данных в хранилище
export function processIncomingData(data: SsvcOpenConnectMessage) {
    processIncomingDataHandler(data)
}

