// store.js
import { writable } from 'svelte/store';

// Глобальное состояние для отслеживания активной секции
export const activeSection = writable('telemetry'); // Начальная секция — 'telemetry'

export let ssvcMessage