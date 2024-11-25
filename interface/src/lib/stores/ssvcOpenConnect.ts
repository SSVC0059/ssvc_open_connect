// @ts-ignore

import type { CommandState, SsvcOpenConnectMessage, SsvcSettings } from '$lib/types/models';
import { type Writable, writable } from 'svelte/store';
import { processIncomingDataHandler } from '$lib/utils/ssvcHelper';

export const ssvcTelemetryStore = writable<SsvcOpenConnectMessage | null>(null);

export const ssvcSettingsStore = writable<SsvcSettings | null>(null);

export const ssvcEventStore = writable<string | null>(null);

export const ssvcCurrentVersionStore = writable<string>("");

export const commandState: Writable<CommandState> = writable({
    isWaiting: false,
    message: "",
    command: ""
});

// Функция для обработки данных в хранилище
export function processIncomingData(data: SsvcOpenConnectMessage) {
    processIncomingDataHandler(data)
}

