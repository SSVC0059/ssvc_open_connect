import { describe, it, expect, beforeAll, vi } from 'vitest';
import { JSDOM } from 'jsdom';
import fs from 'fs';
import path from 'path';

describe('Compiled DistillationCycleModel Test', () => {
    let dom: JSDOM;
    let window: JSDOM['window'];
    let document: JSDOM['window']['document'];

    beforeAll(() => {
        const calculatorScriptPath = path.resolve(__dirname, '../../docs/calculator/rectification_calculator.js');
        
        // Проверяем, существует ли скомпилированный файл
        if (!fs.existsSync(calculatorScriptPath)) {
            throw new Error(`Compiled calculator not found at ${calculatorScriptPath}. Please run the build process first.`);
        }
        
        const calculatorScript = fs.readFileSync(calculatorScriptPath, 'utf-8');

        // Создаем базовую HTML-структуру, которую ожидает скрипт
        const html = `
            <!DOCTYPE html>
            <html>
            <head>
                <meta charset="utf-8">
                <title>Calculator Test</title>
            </head>
            <body>
                <h1>Calculator Test</h1>
                
                <input type="number" data-path="volumeL" value="18">
                <input type="number" data-path="strengthVol" value="40">
                
                <!-- Добавляем другие поля с дефолтными значениями, чтобы избежать ошибок -->
                <input type="number" data-path="powerKw" value="2.5">
                <input type="number" data-path="stabilizationMin" value="30">
                <input type="checkbox" data-path="heads.enabled" checked>
                <input type="number" data-path="heads.percent" value="3">
                <input type="number" data-path="heads.targetCycles" value="2">
                <input type="checkbox" data-path="late_heads.enabled" checked>
                <input type="number" data-path="late_heads.percent" value="7">
                <input type="number" data-path="hearts.percent" value="75">
                <input type="checkbox" data-path="tails.enabled" checked>
                <input type="number" data-path="tails.percent" value="15">

                <!-- Элементы для вывода результатов -->
                <div data-result="analytics.totalAS"></div>
                <div data-result="analytics.fractions.heartsMl"></div>
                <div id="fraction-warning"></div>
            </body>
            </html>
        `;

        dom = new JSDOM(html, {
            runScripts: "dangerously",
            resources: "usable",
            url: "file:///" + __dirname, // Необходимо для работы модулей, если они есть
        });

        window = dom.window;
        document = window.document;

        // Мокируем console.error, чтобы видеть ошибки в тестах
        vi.spyOn(console, 'error').mockImplementation((...args) => {
            console.log('console.error called in JSDOM:', ...args);
        });

        // Исполняем скрипт калькулятора в контексте JSDOM
        const scriptEl = document.createElement("script");
        scriptEl.textContent = calculatorScript;
        document.body.appendChild(scriptEl);
        
        // JSDOM эмулирует DOMContentLoaded при вставке скрипта.
    });

    it('should calculate total Absolute Spirit correctly based on default values', () => {
        const totalASElement = document.querySelector('[data-result="analytics.totalAS"]');
        
        // Скрипт calculator.js использует volumeL: 18 по умолчанию.
        // Ожидаемый результат: 18 л * 40% = 7.2 л АС = 7200 мл.
        expect(totalASElement!.textContent).toBe('7200 мл');
    });

    it('should update calculation when an input value changes', () => {
        const volumeInput = document.querySelector<HTMLInputElement>('[data-path="volumeL"]');
        const totalASElement = document.querySelector('[data-result="analytics.totalAS"]');

        // Меняем значение в поле ввода
        volumeInput!.value = '25'; // Новое значение для проверки
        
        // Симулируем событие 'input', которое триггерит пересчет в calculator.js
        const inputEvent = new window.Event('input', { bubbles: true });
        volumeInput!.dispatchEvent(inputEvent);

        // Ожидаемый результат: 25 л * 40% = 10 л АС = 10000 мл.
        expect(totalASElement!.textContent).toBe('10000 мл');
        expect(console.error).not.toHaveBeenCalled();
    });
});
