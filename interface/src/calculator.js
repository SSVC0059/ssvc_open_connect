// Этот файл является точкой входа для сборки автономного калькулятора.
// @ts-nocheck
import { deepClone } from '$lib/utils/deepMerge.ts';
import { DistillationCycleModel } from '$lib/actions/DistillationCycleModel.ts';

document.addEventListener('DOMContentLoaded', () => {
	const model = new DistillationCycleModel();

	// Начальные значения согласно вашей структуре [cite: 2025-12-26]
	const defaultParams = {
		powerKw: 2.5,
		volumeL: 18,
		strengthVol: 40,
		stabilizationMin: 30,
		heads: { enabled: true, percent: 3, targetCycles: 2, targetFlowMlh: 0 },
		late_heads: { enabled: true, percent: 7, targetFlowMlh: 250 },
		hearts: { percent: 75, targetFlowMlh: 2500 },
		tails: { enabled: true, percent: 15, targetFlowMlh: 2500 },
		ssvcSettings: {
			valve_bw: [7000, 12000, 0],
			heads: [0, 360],
			late_heads: [0, 250],
			hearts: [0, 5],
			tails: [0, 2],
			release_timer: 0,
			release_speed: 0,
			heads_final: 0,
			hearts_finish_temp: 0
		},
		virtual_bw_late: 7500,
		virtual_bw_tails: 7000
	};

	// Вспомогательные функции
	const formatTime = (s) => {
		if (!s || s < 0 || isNaN(s)) return "00:00:00";
		return new Date(s * 1000).toISOString().substr(11, 8);
	};

	const getValueByPath = (obj, path) => {
		return path.split(/[.\[\]]+/).filter(Boolean).reduce((acc, key) => acc?.[key], obj);
	};

	const setValueByPath = (obj, path, value) => {
		const keys = path.split(/[.\[\]]+/).filter(Boolean);
		const lastKey = keys.pop();
		const lastObj = keys.reduce((acc, key) => acc[key] = acc[key] || {}, obj);
		lastObj[lastKey] = value;
	};

	function updateUI() {
		const currentParams = deepClone(defaultParams);

		// Сбор данных из всех инпутов с data-path
		document.querySelectorAll('[data-path]').forEach(el => {
			let val = el.type === 'checkbox' ? el.checked : parseFloat(el.value);
			if (el.type !== 'checkbox' && isNaN(val)) val = getValueByPath(defaultParams, el.dataset.path);
			setValueByPath(currentParams, el.dataset.path, val);
		});

		// Расчет через модель [cite: 2025-10-16]
		try {
			const profile = model.calculateProcess(currentParams);

			// Вывод результатов в элементы с data-result
			document.querySelectorAll('[data-result]').forEach(el => {
				const path = el.dataset.result;
				const value = getValueByPath(profile, path);

				if (value !== null && value !== undefined && !isNaN(value)) {
					if (path.includes('timer') || path.includes('Time') || path.includes('process')) {
						el.textContent = formatTime(value);
					} else if (path.includes('.0') || path.includes('Ratio') || path.includes('Temp') || path.includes('phlegmatic')) {
						el.textContent = value.toFixed(2) + (path.includes('Temp') ? ' °C' : '');
					} else {
						const unit = (path.includes('Ml') || path.includes('AS')) ? ' мл' :
							path.includes('flows') ? ' мл/ч' :
								path.includes('Fortress') ? ' %' : '';
						el.textContent = Math.round(value) + unit;
					}
				}
			});

			// Валидация суммы фракций
			const totalPercent = (currentParams.heads.enabled ? currentParams.heads.percent : 0) +
				(currentParams.late_heads.enabled ? currentParams.late_heads.percent : 0) +
				(currentParams.tails.enabled ? currentParams.tails.percent : 0) +
				currentParams.hearts.percent;

			const warningDiv = document.getElementById('fraction-warning');
			if (warningDiv) {
				warningDiv.style.display = totalPercent > 100 ? 'block' : 'none';
				warningDiv.textContent = `Сумма фракций: ${totalPercent.toFixed(1)}% > 100%`;
			}

		} catch (e) {
			console.error("Ошибка расчета:", e);
		}
	}

	// Инициализация событий
	document.addEventListener('input', (e) => {
		if (e.target.dataset.path) {
			// Взаимное обнуление для логики "или скорость, или циклы/температура"
			if (e.target.dataset.path === 'heads.targetFlowMlh') document.querySelector('[data-path="heads.targetCycles"]').value = '';
			if (e.target.dataset.path === 'heads.targetCycles') document.querySelector('[data-path="heads.targetFlowMlh"]').value = '';
			if (e.target.dataset.path === 'hearts.percent') document.querySelector('[data-path="ssvcSettings.hearts_finish_temp"]').value = '';
			if (e.target.dataset.path === 'ssvcSettings.hearts_finish_temp') document.querySelector('[data-path="hearts.percent"]').value = '';

			updateUI();
		}
	});

	// Первичная установка значений в UI
	document.querySelectorAll('[data-path]').forEach(el => {
		const val = getValueByPath(defaultParams, el.dataset.path);
		if (val !== undefined) {
			if (el.type === 'checkbox') el.checked = val;
			else el.value = val;
		}
	});

	updateUI();
});