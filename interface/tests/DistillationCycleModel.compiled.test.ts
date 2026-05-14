import { describe, it, expect, beforeAll, vi } from 'vitest';
import { JSDOM } from 'jsdom';
import fs from 'fs';
import path from 'path';

/** Минимальная разметка как на странице MkDocs: корень `.editor-layout#app`. */
function buildCalculatorFixtureHtml(): string {
	return `
            <!DOCTYPE html>
            <html>
            <head>
                <meta charset="utf-8">
                <title>Calculator Test</title>
            </head>
            <body>
                <div class="editor-layout" id="app">
                <h1>Calculator Test</h1>
                <button type="button" id="reset-all-btn">Сбросить</button>

                <input type="number" data-path="volumeL" value="18">
                <input type="number" data-path="strengthVol" value="40">

                <input type="number" data-path="powerKw" value="2.5">
                <input type="number" data-path="stabilizationMin" value="20">
                <input type="checkbox" data-path="heads.enabled" checked>
                <input type="number" data-path="heads.percent" value="3">
                <input type="number" data-path="heads.targetCycles" value="2">
                <input type="checkbox" data-path="late_heads.enabled" checked>
                <input type="number" data-path="late_heads.percent" value="7">
                <input type="number" data-path="hearts.percent" value="75">
                <input type="checkbox" data-path="tails.enabled" checked>
                <input type="number" data-path="tails.percent" value="15">

                <div data-result="analytics.totalAS"></div>
                <div data-result="analytics.fractions.heartsMl"></div>
                <div id="fraction-warning"></div>
                </div>
            </body>
            </html>
        `;
}

describe('Compiled DistillationCycleModel Test', () => {
	let dom: JSDOM;
	let window: JSDOM['window'];
	let document: JSDOM['window']['document'];

	beforeAll(() => {
		const calculatorScriptPath = path.resolve(__dirname, '../../docs/calculator/rectification_calculator.js');

		if (!fs.existsSync(calculatorScriptPath)) {
			throw new Error(`Compiled calculator not found at ${calculatorScriptPath}. Please run the build process first.`);
		}

		const calculatorScript = fs.readFileSync(calculatorScriptPath, 'utf-8');

		dom = new JSDOM(buildCalculatorFixtureHtml(), {
			runScripts: 'dangerously',
			resources: 'usable',
			url: 'file:///' + __dirname
		});

		window = dom.window;
		document = window.document;

		vi.spyOn(console, 'error').mockImplementation((...args) => {
			console.log('console.error called in JSDOM:', ...args);
		});

		const scriptEl = document.createElement('script');
		scriptEl.textContent = calculatorScript;
		document.body.appendChild(scriptEl);
	});

	it('should calculate total Absolute Spirit correctly based on default values', () => {
		const totalASElement = document.querySelector('[data-result="analytics.totalAS"]');
		expect(totalASElement!.textContent).toBe('7200 мл');
	});

	it('should update calculation when an input value changes', () => {
		const volumeInput = document.querySelector<HTMLInputElement>('[data-path="volumeL"]');
		const totalASElement = document.querySelector('[data-result="analytics.totalAS"]');

		volumeInput!.value = '25';
		const inputEvent = new window.Event('input', { bubbles: true });
		volumeInput!.dispatchEvent(inputEvent);

		expect(totalASElement!.textContent).toBe('10000 мл');
		expect(console.error).not.toHaveBeenCalled();
	});

	it('should reset to defaults when reset-all-btn is clicked', () => {
		const volumeInput = document.querySelector<HTMLInputElement>('[data-path="volumeL"]');
		const totalASElement = document.querySelector('[data-result="analytics.totalAS"]');
		const resetBtn = document.getElementById('reset-all-btn');

		volumeInput!.value = '25';
		volumeInput!.dispatchEvent(new window.Event('input', { bubbles: true }));
		expect(totalASElement!.textContent).toBe('10000 мл');

		resetBtn!.dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
		expect(volumeInput!.value).toBe('18');
		expect(totalASElement!.textContent).toBe('7200 мл');
	});
});

describe('MkDocs calculator page contract', () => {
	it('docs/calculator/index.md should expose data-result and model-aligned data-path keys', () => {
		const idxPath = path.resolve(__dirname, '../../docs/calculator/index.md');
		const idx = fs.readFileSync(idxPath, 'utf-8');
		expect(idx).toContain('data-result="analytics.totalAS"');
		expect(idx).toContain('data-path="late_heads.enabled"');
		expect(idx).toContain('data-path="virtual_bw_late"');
		expect(idx).toContain('data-path="hearts.percent"');
		expect(idx).not.toContain('data-path="subHeads.');
		expect(idx).not.toContain('data-path="body.');
	});
});

describe('Calculator bootstrap when DOMContentLoaded has already fired', () => {
	it('should still populate analytics when script runs after complete parse', () => {
		const calculatorScriptPath = path.resolve(__dirname, '../../docs/calculator/rectification_calculator.js');
		const calculatorScript = fs.readFileSync(calculatorScriptPath, 'utf-8');

		const lateDom = new JSDOM(buildCalculatorFixtureHtml(), {
			runScripts: 'dangerously',
			resources: 'usable',
			url: 'file:///' + __dirname
		});
		const { window: w } = lateDom;
		Object.defineProperty(w.document, 'readyState', { value: 'complete', configurable: true });

		const s = w.document.createElement('script');
		s.textContent = calculatorScript;
		w.document.body.appendChild(s);

		const totalAS = w.document.querySelector('[data-result="analytics.totalAS"]');
		expect(totalAS?.textContent).toBe('7200 мл');
	});
});

describe('Material document$ integration', () => {
	it('should subscribe once and re-init when document$ emits', () => {
		const calculatorScriptPath = path.resolve(__dirname, '../../docs/calculator/rectification_calculator.js');
		const calculatorScript = fs.readFileSync(calculatorScriptPath, 'utf-8');

		const subscribers: Array<() => void> = [];

		const d = new JSDOM(
			`<!DOCTYPE html><html><body><div class="editor-layout" id="app">
			<input type="number" data-path="volumeL" value="18">
			<input type="number" data-path="strengthVol" value="40">
			<input type="number" data-path="powerKw" value="2.5">
			<input type="number" data-path="stabilizationMin" value="20">
			<input type="checkbox" data-path="heads.enabled" checked>
			<input type="number" data-path="heads.percent" value="3">
			<input type="number" data-path="heads.targetCycles" value="2">
			<input type="checkbox" data-path="late_heads.enabled" checked>
			<input type="number" data-path="late_heads.percent" value="7">
			<input type="number" data-path="hearts.percent" value="75">
			<input type="checkbox" data-path="tails.enabled" checked>
			<input type="number" data-path="tails.percent" value="15">
			<span data-result="analytics.totalAS"></span>
		</div></body></html>`,
			{
				runScripts: 'dangerously',
				url: 'file:///' + __dirname
			}
		);
		const win = d.window;
		(win as unknown as { document$: { subscribe: (fn: () => void) => void } }).document$ = {
			subscribe(fn: () => void) {
				subscribers.push(fn);
			}
		};

		const s = win.document.createElement('script');
		s.textContent = calculatorScript;
		win.document.body.appendChild(s);

		expect(subscribers.length).toBe(1);
		subscribers[0]();
		expect(win.document.querySelector('[data-result="analytics.totalAS"]')?.textContent).toBe('7200 мл');
	});
});
