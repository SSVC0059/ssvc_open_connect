// Этот файл является точкой входа для сборки автономного калькулятора.
// @ts-nocheck
import { deepClone } from '$lib/utils/deepMerge.ts';
import { DistillationCycleModel, createDefaultProfile } from '$lib/actions/DistillationCycleModel.ts';
import {
	minHysteresisFor,
	predecLabel,
	predecState
} from '$lib/actions/rectificationRules.ts';

const CALC_ROOT_SELECTOR = '.editor-layout#app';

/** Поля SSVC, которые модель пересчитывает и которые нужно показывать в инпутах (не только в analytics). */
const SSVC_COMPUTED_INPUT_PATHS = [
	'ssvcSettings.heads.0',
	'ssvcSettings.late_heads.0',
	'ssvcSettings.hearts.0',
	'ssvcSettings.tails.0'
];

const model = new DistillationCycleModel();

/** База дефолтов для текущей страницы калькулятора (пересоздаётся при каждом init). */
let activeDefaultParams = null;

/** Текущий updateUI с замыканием на activeDefaultParams; вызывается из делегированного input. */
let runUpdate = () => {};

function getCalcRoot() {
	return document.querySelector(CALC_ROOT_SELECTOR);
}

function getValueByPath(obj, path) {
	return path.split(/[.\[\]]+/).filter(Boolean).reduce((acc, key) => acc?.[key], obj);
}

function setValueByPath(obj, path, value) {
	const keys = path.split(/[.\[\]]+/).filter(Boolean);
	const lastKey = keys.pop();
	const lastObj = keys.reduce((acc, key) => {
		if (acc[key] == null || typeof acc[key] !== 'object') acc[key] = {};
		return acc[key];
	}, obj);
	lastObj[lastKey] = value;
}

const formatTime = (s) => {
	if (!s || s < 0 || isNaN(s)) return '00:00:00';
	return new Date(s * 1000).toISOString().substring(11, 19);
};

function buildFreshDefaults() {
	const p = deepClone(createDefaultProfile());
	p.virtual_bw_late = 7500;
	p.virtual_bw_tails = 7000;
	return p;
}

function isCalculatorRoute() {
	return /\/calculator(\/|$)/i.test(location.pathname);
}

/**
 * Material instant navigation не вызывает DOMContentLoaded; инициализация идёт через document$.
 * На медленных сетях (GitHub Pages) document$ или выполнение скрипта может опережать/отставать от
 * вставки разметки — даём несколько кадров на появление .editor-layout#app.
 */
function deferredInit() {
	if (getCalcRoot()) {
		initCalculator();
		return;
	}
	if (!isCalculatorRoute()) return;

	let frames = 0;
	const maxFrames = 40;
	const tick = () => {
		requestAnimationFrame(() => {
			if (getCalcRoot()) {
				initCalculator();
				return;
			}
			if (++frames < maxFrames) tick();
		});
	};
	tick();
}

function initCalculator() {
	const root = getCalcRoot();
	if (!root) return;

	delete root.dataset.calcReady;
	activeDefaultParams = buildFreshDefaults();

	const formatResultText = (path, value) => {
		if (path.startsWith('analytics.formula')) {
			if (path.endsWith('initialDropPct')) return `${Math.round(value)} %`;
			if (path.endsWith('avgFactor')) return `${Math.round(value * 100)} %`;
			return `${value.toFixed(1)} °C`;
		}
		if (path.includes('timer') || path.includes('Time') || path.includes('process')) {
			return formatTime(value);
		}
		if (
			path.includes('.0') ||
			path.includes('Ratio') ||
			path.includes('Temp') ||
			path.includes('phlegmatic')
		) {
			return value.toFixed(2) + (path.includes('Temp') ? ' °C' : '');
		}
		const unit =
			path.includes('Ml') || path.includes('AS')
				? ' мл'
				: path.includes('flows')
					? ' мл/ч'
					: path.includes('Fortress')
						? ' %'
						: '';
		return Math.round(value) + unit;
	};

	function updateUI() {
		const r = getCalcRoot();
		if (!r || !activeDefaultParams) return;

		const currentParams = deepClone(activeDefaultParams);

		r.querySelectorAll('[data-path]').forEach((el) => {
			let val = el.type === 'checkbox' ? el.checked : parseFloat(el.value);
			if (el.type !== 'checkbox' && isNaN(val)) {
				val = getValueByPath(activeDefaultParams, el.dataset.path);
			}
			setValueByPath(currentParams, el.dataset.path, val);
		});

		try {
			const profile = model.calculateProcess(currentParams);

			// В «Авто 92+» начало формулы фиксировано на 92 °C, а первая Т
			// предекремента нужна только этому режиму — показываем зависимость.
			const formulaStartInput = r.querySelector('[data-path="ssvcSettings.formula_start_temp"]');
			if (formulaStartInput) {
				const auto92 = currentParams.ssvcSettings.formula === 2;
				formulaStartInput.disabled = auto92;
				formulaStartInput.title = auto92
					? 'В режиме «Авто 92+» начало формулы — 92 °C'
					: '';
			}
			const firstPredecInput = r.querySelector('[data-path="planning.firstPredecTd2"]');
			if (firstPredecInput) {
				firstPredecInput.disabled = currentParams.ssvcSettings.formula !== 2;
			}

			r.querySelectorAll('[data-result]').forEach((el) => {
				const path = el.dataset.result;
				const value = getValueByPath(profile, path);
				if (value !== null && value !== undefined && !isNaN(value)) {
					el.textContent = formatResultText(path, value);
				} else {
					el.textContent = '';
				}
			});

			SSVC_COMPUTED_INPUT_PATHS.forEach((path) => {
				const value = getValueByPath(profile, path);
				const input = r.querySelector(`[data-path="${path}"]`);
				if (
					input &&
					input.type !== 'checkbox' &&
					value !== null &&
					value !== undefined &&
					!Number.isNaN(value)
				) {
					input.value = value;
				}
			});

			const totalPercent =
				(currentParams.heads.enabled ? currentParams.heads.percent : 0) +
				(currentParams.late_heads.enabled ? currentParams.late_heads.percent : 0) +
				(currentParams.tails.enabled ? currentParams.tails.percent : 0) +
				currentParams.hearts.percent;

			const warningDiv = document.getElementById('fraction-warning');
			if (warningDiv) {
				warningDiv.style.display = totalPercent > 100 ? 'block' : 'none';
				warningDiv.textContent = `Сумма фракций: ${totalPercent.toFixed(1)}% > 100%`;
			}

			const protectionWarning = document.getElementById('protection-warning');
			if (protectionWarning) {
				const predec = currentParams.ssvcSettings.predec;
				const hyst = currentParams.ssvcSettings.hyst;
				const state = predecState(predec, hyst);
				let message = '';
				if (state === 'hysteresis-too-small') {
					const min = minHysteresisFor(predec);
					const minText =
						min === null ? '—' : min.toFixed(2).replace('.', ',');
					message = `Предекремент не сработает: при ${predecLabel(predec)} нужен гистерезис не меньше ${minText} °C.`;
				} else if (currentParams.ssvcSettings.formula === 2 && predec === 0) {
					message =
						'«Авто 92+» не сработает без предекремента: включите предекремент или выберите обычную формулу.';
				}
				protectionWarning.style.display = message ? 'block' : 'none';
				protectionWarning.textContent = message;
			}
		} catch (e) {
			console.error('Ошибка расчета:', e);
		}
	}

	runUpdate = updateUI;

	root.querySelectorAll('[data-path]').forEach((el) => {
		const val = getValueByPath(activeDefaultParams, el.dataset.path);
		if (val !== undefined && val !== null && !(typeof val === 'number' && Number.isNaN(val))) {
			if (el.type === 'checkbox') el.checked = !!val;
			else el.value = val;
		}
	});

	updateUI();

	// Сигнал для e2e и для отладки: поля уже привязаны к модели.
	root.dataset.calcReady = '1';
}

let delegatedInputBound = false;

function bindDelegatedInputOnce() {
	if (delegatedInputBound) return;
	delegatedInputBound = true;

	document.addEventListener('input', (e) => {
		const t = e.target;
		if (!t?.dataset?.path) return;
		const root = getCalcRoot();
		if (!root?.contains(t)) return;

		// Только «живой» ввод: синтетические события (inline-хендлеры в index.md) не должны очищать парное поле.
		if (e.isTrusted) {
			if (t.dataset.path === 'heads.targetFlowMlh') {
				const c = root.querySelector('[data-path="heads.targetCycles"]');
				if (c) c.value = '';
			}
			if (t.dataset.path === 'heads.targetCycles') {
				const f = root.querySelector('[data-path="heads.targetFlowMlh"]');
				if (f) f.value = '';
			}
			if (t.dataset.path === 'hearts.percent') {
				const h = root.querySelector('[data-path="ssvcSettings.hearts_finish_temp"]');
				if (h) h.value = '';
			}
			if (t.dataset.path === 'ssvcSettings.hearts_finish_temp') {
				const hp = root.querySelector('[data-path="hearts.percent"]');
				if (hp) hp.value = '';
			}
		}

		runUpdate();
	});
}

let resetClickBound = false;

function bindResetButtonOnce() {
	if (resetClickBound) return;
	resetClickBound = true;

	document.addEventListener('click', (e) => {
		const btn = e.target?.closest?.('#reset-all-btn');
		if (!btn) return;
		const root = getCalcRoot();
		if (!root?.contains(btn)) return;
		e.preventDefault();
		initCalculator();
	});
}

function scheduleBoot() {
	bindDelegatedInputOnce();
	bindResetButtonOnce();

	if (typeof document$ !== 'undefined' && document$.subscribe) {
		document$.subscribe(() => {
			deferredInit();
		});
	}

	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', deferredInit);
	} else {
		deferredInit();
	}
}

scheduleBoot();
