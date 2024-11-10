// store.js
import { writable } from 'svelte/store';

// Глобальное состояние для отслеживания активной секции
export const activeSection = writable('telemetry'); // Начальная секция — 'telemetry'

export let ssvcMessage

// Функция для отправки POST-запроса
export async function sendPostRequest(command) {
    activeSection.set(command)
    await sendPostRequestSilent(command)
}

// Функция для отправки POST-запроса
export async function sendPostRequestSilent(command) {
    try {
        let url = '/command'
        console.log(url);
        const response = await fetch(`${url}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ command: command })
        });

        if (response.ok) {
            const data = await response.json();
            console.log("Ответ от сервера:", data);
        } else {
            console.error("Ошибка при выполнении запроса:", response.status);
        }
    } catch (error) {
        console.error("Ошибка сети:", error);
    }
}