// Этот файл является точкой входа для сборки автономного калькулятора.
// @ts-nocheck
import { deepClone } from '$lib/utils/deepMerge.ts';
import { DistillationCycleModel, createDefaultProfile } from '$lib/actions/DistillationCycleModel.ts';

const CALC_ROOT_SELECTOR = '.editor-layout#app';

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

let documentSubscribed = false;

function initCalculator() {
	const root = getCalcRoot();
	if (!root) return;

	activeDefaultParams = buildFreshDefaults();

	const formatResultText = (path, value) => {
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

			r.querySelectorAll('[data-result]').forEach((el) => {
				const path = el.dataset.result;
				const value = getValueByPath(profile, path);
				if (value !== null && value !== undefined && !isNaN(value)) {
					el.textContent = formatResultText(path, value);
				} else {
					el.textContent = '';
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

function scheduleBoot() {
	bindDelegatedInputOnce();

	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', () => initCalculator());
	} else {
		initCalculator();
	}

	if (!documentSubscribed && typeof document$ !== 'undefined' && document$.subscribe) {
		documentSubscribed = true;
		document$.subscribe(() => {
			initCalculator();
		});
	}
}

scheduleBoot();
