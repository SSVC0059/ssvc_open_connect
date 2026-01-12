---
hide:
  - navigation
---

# Калькулятор ректификации


<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Калькулятор ректификации</title>
    <style>
        :root {
            --surface-color: #ffffff;
            --surface-alt-color: #f8f9fa;
            --surface-hover-color: #f1f3f5;
            --border-color: #dee2e6;
            --text-color: #212529;
            --text-muted-color: #6c757d;
            --primary-800: #1c4b82;
            --primary-700: #225a9a;
            --primary-600: #296dc8;
            --primary-500: #337dcc;
            --primary-300: #a5d8ff;
            --primary-100: #e7f5ff;
            --blue-50: #e7f5ff;
            --blue-300: #91c9ff;
            --orange-50: #fff4e6;
            --orange-100: #updateUIffe8cc;
            --orange-200: #ffd8a8;
            --orange-400: #ffb366;
            --orange-600: #f79009;
            --orange-800: #b56900;
            --gray-50: #f8f9fa;
            --red-100: #fff5f5;
            --red-300: #ffc9c9;
            --red-600: #fa5252;
            --border-radius: 0.25rem;
            --border-radius-lg: 0.5rem;
            --shadow-sm: 0 1px 2px 0 rgb(0 0 0 / 0.05);
        }

        .editor-layout { display: flex; flex-direction: column; gap: 1rem; }
        .card { background-color: var(--surface-color); border-radius: var(--border-radius-lg); padding: 1.25rem; border: 1px solid var(--border-color); }
        .card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem; flex-wrap: wrap; }
        .card-title { font-size: 1.125rem; font-weight: 600; color: var(--primary-800); margin: 0; }
        .settings-section { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 0.5rem 1.5rem; }
        .input-group { display: flex; justify-content: space-between; align-items: center; padding: 0.4rem 0; }
        .input-label { font-size: 0.9rem; color: var(--text-color); margin-right: 1rem; }
        .input-field { width: 6rem; padding: 0.3rem 0.5rem; border: 1px solid var(--border-color); border-radius: var(--border-radius); text-align: right; }

        .fraction-details { border-bottom: 1px solid var(--border-color); }
        .fraction-details summary { list-style: none; cursor: pointer; padding: 0.75rem 0.25rem; font-weight: 600; }
        .summary-content { display: flex; justify-content: space-between; align-items: center; }
        .fraction-body { background-color: var(--gray-50); border-top: 1px solid var(--border-color); padding: 1rem; display: grid; grid-template-columns: 1fr; gap: 1.5rem; }
        @media (min-width: 768px) { .fraction-body { grid-template-columns: 1.5fr 1fr; } }

        .results-col { background-color: var(--surface-color); border: 1px dashed var(--primary-300); padding: 1rem; border-radius: var(--border-radius); display: flex; flex-direction: column; gap: 0.5rem; }
        .analytics-item { display: flex; justify-content: space-between; align-items: center; padding: 0.4rem 0.6rem; background: var(--surface-alt-color); border-radius: 4px; }
        .analytics-label { font-size: 0.8rem; color: var(--text-muted-color); }
        .analytics-value { font-size: 0.95rem; font-weight: 600; color: var(--primary-700); }
        .highlight .analytics-value { color: var(--orange-600); }

        .ssvc-result-card { margin-top: 0.5rem; padding: 0.75rem; background: var(--primary-100); border: 1px solid var(--blue-300); border-radius: 6px; }
        .sub-settings-title { font-size: 0.75rem; font-weight: 600; color: var(--primary-500); text-transform: uppercase; margin-bottom: 0.5rem; border-bottom: 1px solid var(--primary-300); }

        .final-analytics-card { border-color: var(--blue-300) !important; background-color: var(--primary-100) !important; }
        .final-analytics-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 1rem; }
        .chevron { width: 1.25rem; height: 1.25rem; transition: transform 0.2s; }
        details[open] .chevron { transform: rotate(180deg); }

        #fraction-warning {
            display: none;
            width: 100%;
            margin-top: 0.5rem;
            color: var(--red-600);
            font-size: 0.9rem;
            font-weight: 600;
            background-color: var(--red-100);
            padding: 0.5rem 1rem;
            border-radius: var(--border-radius);
            border: 1px solid var(--red-300);
            text-align: center;
        }
        .actions-panel { display: flex; justify-content: flex-end; padding: 1rem 0; }
        .btn-primary {
            padding: 0.6rem 1.5rem;
            border-radius: var(--border-radius);
            font-weight: 600;
            cursor: pointer;
            background-color: var(--primary-500);
            color: var(--surface-color);
            border: 1px solid var(--primary-500);
            transition: background-color 0.2s;
        }
        .btn-primary:hover { background-color: var(--primary-600); }
        .btn-primary:disabled {
            background-color: #6c757d;
            border-color: #6c757d;
            cursor: not-allowed;
        }
    </style>
</head>
<body>

<div class="editor-layout" id="app">
    <div class="card">
        <div class="card-header">
            <h3 class="card-title">Параметры процесса</h3>
        </div>
        <div class="settings-section">
            <div class="input-group"><span class="input-label">Мощность, кВт</span><label>
                <input data-path="powerKw" class="input-field" type="number" step="0.1" value="1">
            </label></div>
            <div class="input-group"><span class="input-label">Объем заливки, л</span><label>
                <input data-path="volumeL" class="input-field" type="number" step="0.5" value="18">
            </label></div>
            <div class="input-group"><span class="input-label">Крепость, %</span><label>
                <input data-path="strengthVol" class="input-field" type="number" value="40">
            </label></div>
            <div class="input-group"><span class="input-label">Стабилизация, мин</span><label>
                <input data-path="stabilizationMin" class="input-field" type="number" value="30">
            </label></div>
        </div>
    </div>

    <div class="card">
        <div class="card-header">
            <h3 class="card-title">Настройки отбора</h3>
            <div id="fraction-warning"></div>
        </div>

<!--        Головы       -->
        <details class="fraction-details" id="details-heads" open>
            <summary>
                <div class="summary-content">
                    <span>Головы</span>
                    <div style="display:flex; gap:10px; align-items:center;">
                        <label>
                            <input type="checkbox" data-path="heads.enabled" checked onclick="event.stopPropagation()">
                        </label>
                        <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 9l6 6 6-6"/></svg>
                    </div>
                </div>
            </summary>
            <div class="fraction-body">
                <div class="settings-col">
                    <div class="input-group"><span class="input-label">% от АС</span><label>
                        <input data-path="heads.percent" class="input-field" type="number" value="3">
                    </label></div>
                    <div class="input-group"><span class="input-label">Клапан (мл/ч)</span><label>
                        <input data-path="ssvcSettings.valve_bw[0]" class="input-field" type="number">
                    </label></div>
                    <div class="sub-settings-title" style="margin-top:10px">Режим расчета</div>
                    <div class="input-group"><span class="input-label">Циклы</span><label>
                        <input data-path="heads.targetCycles" class="input-field" type="number" step="0.1" value="2" oninput="handleHeadsCyclesInput(event)">
                    </label></div>
                    <div class="input-group"><span class="input-label">Желаемая скорость, мл/ч</span><label>
                        <input data-path="heads.targetFlowMlh" class="input-field" type="number" oninput="handleHeadsFlowInput(event)">
                    </label></div>
                    <div class="input-group" style="margin-top:10px"><span class="input-label">Период SSVC (сек)</span><label>
                        <input data-path="ssvcSettings.heads[1]" class="input-field" type="number" value="10">
                    </label></div>
                </div>
                <div class="results-col">
                    <div class="analytics-item"><span class="analytics-label">Объем</span><span class="analytics-value" data-result="analytics.fractions.headsMl">0 мл</span></div>
                    <div class="analytics-item highlight"><span class="analytics-label">Скорость</span><span class="analytics-value" data-result="analytics.flows.heads">0 мл/ч</span></div>
                    <div class="analytics-item"><span class="analytics-label">Флегмовое число</span><span class="analytics-value" data-result="analytics.refluxRatio.heads">0</span></div>
                    <div class="analytics-item"><span class="analytics-label">Время этапа</span><span class="analytics-value" data-result="analytics.timers.heads">00:00:00</span></div>
                    <div class="analytics-item"><span class="analytics-label">Время переиспарения</span><span class="analytics-value" data-result="analytics.oneCycleTime">00:00:00</span></div>
                    <div class="ssvc-result-card">
                        <div class="sub-settings-title">Клапан SSVC</div>
                        <div class="analytics-item"><span class="analytics-label">Время</span><span class="analytics-value" data-result="ssvcSettings.heads.0">0.00 сек</span></div>
                        <div class="analytics-item"><span class="analytics-label">Период</span><span class="analytics-value" data-result="ssvcSettings.heads.1">0.00 сек</span></div>

                    </div>
                </div>
            </div>
        </details>

        <details class="fraction-details" id="details-subHeads" open>
            <summary>
                <div class="summary-content">
                    <span>Подголовники</span>
                    <div style="display:flex; gap:10px; align-items:center;">
                        <label>
                            <input type="checkbox" data-path="late_heads.enabled" onclick="event.stopPropagation()">
                        </label>
                        <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 9l6 6 6-6"/></svg>
                    </div>
                </div>
            </summary>
            <div class="fraction-body">
                <div class="settings-col">
                    <div class="input-group"><span class="input-label">% от АС</span>
                        <input data-path="late_heads.percent" class="input-field" type="number" value="7">
                    </div>
                    <div class="input-group"><span class="input-label">Клапан (мл/ч)</span>
                        <input data-path="ssvcSettings.valve_bw[2]" class="input-field" type="number">
                    </div>
                    <div class="input-group"><span class="input-label">Желаемая скорость, мл/ч</span>
                        <input data-path="late_heads.targetFlowMlh" class="input-field" type="number" placeholder="Авто (10%)">
                    </div>
                    <div class="input-group" style="margin-top:10px"><span class="input-label">Период SSVC (сек)</span>
                        <input data-path="ssvcSettings.late_heads[1]" class="input-field" type="number">
                    </div>
                </div>
                <div class="results-col">
                    <div class="analytics-item"><span class="analytics-label">Объем</span>
                        <span class="analytics-value" data-result="analytics.fractions.lateHeadsMl">0 мл</span>
                    </div>
                    <div class="analytics-item highlight"><span class="analytics-label">Скорость</span>
                        <span class="analytics-value" data-result="analytics.flows.late_heads">0 мл/ч</span>
                    </div>
                    <div class="analytics-item"><span class="analytics-label">Флегмовое число</span>
                        <span class="analytics-value" data-result="analytics.refluxRatio.late_heads">0</span>
                    </div>
                    <div class="analytics-item"><span class="analytics-label">Время этапа</span>
                        <span class="analytics-value" data-result="analytics.timers.late_heads">00:00:00</span>
                    </div>
                    <div class="ssvc-result-card">
                        <div class="sub-settings-title">Клапан SSVC</div>
                        <div class="analytics-item"><span class="analytics-label">Время</span><span class="analytics-value" data-result="ssvcSettings.late_heads.0">0.00 сек</span></div>
                        <div class="analytics-item"><span class="analytics-label">Период</span><span class="analytics-value" data-result="ssvcSettings.late_heads.1">0.00 сек</span></div>

                    </div>
                </div>
            </div>
        </details>

        <details class="fraction-details" open>
            <summary>
                <div class="summary-content">
                    <span>Тело</span>
                    <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 9l6 6 6-6"/></svg>
                </div>
            </summary>
            <div class="fraction-body">
                <div class="settings-col">
                    <div class="input-group"><span class="input-label">Отбор (% от АС)</span><input data-path="hearts.percent" class="input-field" type="number" value="80"></div>
                    <div class="input-group"><span class="input-label">Клапан (мл/ч)</span><input data-path="ssvcSettings.valve_bw[1]" class="input-field" type="number" value="1500"></div>
                    <div class="input-group"><span class="input-label">Желаемая скорость (мл/ч)</span><input data-path="hearts.targetFlowMlh" class="input-field" type="number" value="1000"></div>
                    <div class="input-group"><span class="input-label">Период SSVC (сек)</span><input data-path="ssvcSettings.hearts[1]" class="input-field" type="number" value="10"></div>
                </div>
                <div class="results-col">
                    <div class="analytics-item"><span class="analytics-label">Объем</span><span class="analytics-value" data-result="analytics.fractions.heartsMl">0 мл</span></div>
                    <div class="analytics-item highlight"><span class="analytics-label">Скорость</span><span class="analytics-value" data-result="analytics.flows.hearts">0 мл/ч</span></div>
                    <div class="analytics-item"><span class="analytics-label">Флегмовое число</span><span class="analytics-value" data-result="analytics.refluxRatio.hearts">0</span></div>
                    <div class="analytics-item"><span class="analytics-label">Время этапа</span><span class="analytics-value" data-result="analytics.timers.hearts">00:00:00</span></div>
                    <div class="ssvc-result-card">
                        <div class="sub-settings-title">Клапан SSVC</div>
                        <div class="analytics-item"><span class="analytics-label">Время</span><span class="analytics-value" data-result="ssvcSettings.hearts.0">0.00 сек</span></div>
                        <div class="analytics-item"><span class="analytics-label">Период</span><span class="analytics-value" data-result="ssvcSettings.hearts.1">0.00 сек</span></div>
                    </div>
                </div>
            </div>
        </details>

<!--    Хвосты -->

        <details class="fraction-details" id="details-tails" open>
            <summary>
                <div class="summary-content">
                    <span>Хвосты (Tails)</span>
                    <div style="display:flex; gap:10px; align-items:center;">
                        <label>
                            <input type="checkbox" data-path="tails.enabled" onclick="event.stopPropagation()">
                        </label>
                        <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 9l6 6 6-6"/></svg>
                    </div>
                </div>
            </summary>
            <div class="fraction-body">
                <div class="settings-col">
                    <div class="input-group"><span class="input-label">% от АС</span>
                        <label>
                            <input data-path="tails.percent" class="input-field" type="number" value="5">
                        </label>
                    </div>
                    <div class="input-group"><span class="input-label">Скорость (мл/ч)</span>
                        <label>
                            <input data-path="tails.targetFlowMlh" class="input-field" type="number" placeholder="Максимум">
                        </label>
                    </div>
                    <div class="input-group" style="margin-top:10px"><span class="input-label">Период SSVC (сек)</span>
                        <label>
                            <input data-path="ssvcSettings.tails[1]" class="input-field" type="number" value="10">
                        </label>
                    </div>
                </div>
                <div class="results-col">
                    <div class="analytics-item"><span class="analytics-label">Объем</span>
                        <span class="analytics-value" data-result="analytics.fractions.tailsMl">0 мл</span>
                    </div>
                    <div class="analytics-item highlight"><span class="analytics-label">Скорость</span>
                        <span class="analytics-value" data-result="analytics.flows.tails">0 мл/ч</span>
                    </div>
                    <div class="analytics-item"><span class="analytics-label">Флегмовое число</span>
                        <span class="analytics-value" data-result="analytics.refluxRatio.tails">0</span>
                    </div>
                    <div class="analytics-item"><span class="analytics-label">Время этапа</span>
                        <span class="analytics-value" data-result="analytics.timers.tails">00:00:00</span>
                    </div>
                    <div class="ssvc-result-card">
                        <div class="sub-settings-title">Клапан SSVC</div>
                        <div class="analytics-item"><span class="analytics-label">Время</span><span class="analytics-value" data-result="ssvcSettings.tails.0">0.00 сек</span></div>
                        <div class="analytics-item"><span class="analytics-label">Период</span><span class="analytics-value" data-result="ssvcSettings.tails.1">0.00 сек</span></div>
                    </div>
                </div>
            </div>
        </details>
    </div>

    <div class="card final-analytics-card">
        <h3 class="card-title">Итоговая аналитика</h3>
        <div class="final-analytics-grid">
            <div class="analytics-item"><span class="analytics-label">Общий АС</span><span class="analytics-value" data-result="analytics.totalAS">0 мл</span></div>
            <div class="analytics-item"><span class="analytics-label">T° кипения</span><span class="analytics-value" data-result="analytics.boilingTemp">0 °C</span></div>
            <div class="analytics-item"><span class="analytics-label">Остаток в кубе</span><span class="analytics-value" data-result="analytics.residueMl">0 мл</span></div>
            <div class="analytics-item"><span class="analytics-label">Общее время</span><span class="analytics-value" data-result="analytics.timers.total_process">00:00:00</span></div>
        </div>
    </div>
</div>

<script src="rectification_calculator.js"></script>
<script>
    const model = new DistillationCycleModel();

    // Значения по умолчанию согласно вашей структуре
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
            valve_bw: [7000, 12000, 7000],
            heads: [0, 360],
            late_heads: [0, 250],
            hearts: [0, 5],
            tails: [0, 2]
        }
    };

    function formatTime(seconds) {
        if (!seconds || seconds < 0 || isNaN(seconds)) return "00:00:00";
        const h = Math.floor(seconds / 3600).toString().padStart(2, '0');
        const m = Math.floor((seconds % 3600) / 60).toString().padStart(2, '0');
        const s = Math.floor(seconds % 60).toString().padStart(2, '0');
        return `${h}:${m}:${s}`;
    }

    function getValueByPath(obj, path) {
        if (!path) return null;
        const keys = path.split(/[.\[\]]+/).filter(Boolean);
        return keys.reduce((acc, key) => (acc && acc[key] !== undefined) ? acc[key] : null, obj);
    }

    function setValueByPath(obj, path, value) {
        const keys = path.split(/[.\[\]]+/).filter(Boolean);
        let current = obj;
        for (let i = 0; i < keys.length - 1; i++) {
            if (current[keys[i]] === undefined) current[keys[i]] = {};
            current = current[keys[i]];
        }
        current[keys[keys.length - 1]] = value;
    }

    // Заполнение полей ввода из объекта
    function setParamsToUI(obj) {
        document.querySelectorAll('[data-path]').forEach(el => {
            const value = getValueByPath(obj, el.dataset.path);
            if (value !== null && value !== undefined) {
                if (el.type === 'checkbox') {
                    el.checked = Boolean(value);
                } else {
                    el.value = value;
                }
            }
        });
    }

    function updateUI() {
        // Создаем чистый объект для сбора данных
        const currentParams = JSON.parse(JSON.stringify(defaultParams));

        // Сбор актуальных данных из всех input
        document.querySelectorAll('[data-path]').forEach(el => {
            const val = el.type === 'checkbox' ? el.checked : parseFloat(el.value || 0);
            setValueByPath(currentParams, el.dataset.path, val);
        });

        // --- Проверка суммы фракций ---
        let totalPercent = 0;
        if (currentParams.heads.enabled) totalPercent += currentParams.heads.percent || 0;
        if (currentParams.late_heads.enabled) totalPercent += currentParams.late_heads.percent || 0;
        if (currentParams.tails.enabled) totalPercent += currentParams.tails.percent || 0;
        totalPercent += currentParams.hearts.percent || 0; // Тело не имеет выключателя

        const warningDiv = document.getElementById('fraction-warning');
        const saveButton = document.getElementById('save-button');
        const isInvalid = totalPercent > 100;

        warningDiv.style.display = isInvalid ? 'block' : 'none';
        if (isInvalid) {
            warningDiv.textContent = `Сумма фракций (АС): ${totalPercent.toFixed(1)}% - не может быть больше 100%`;
        }
        
        // --- Конец проверки ---

        // Расчет через модель
        const profile = model.calculateProcess(currentParams, true);

        // Вывод результатов в элементы с data-result
        document.querySelectorAll('[data-result]').forEach(el => {
            const path = el.dataset.result;
            const value = getValueByPath(profile, path);

            if (value !== null && value !== undefined) {
                // Если в пути есть Time, timer или process — форматируем как 00:00:00
                if (path.toLowerCase().includes('time') || path.toLowerCase().includes('timer') || path.toLowerCase().includes('process')) {
                    el.textContent = formatTime(value);
                }
                else if (path.includes('.0') || path.includes('refluxRatio') || path.includes('boilingTemp')) {
                    el.textContent = value.toFixed(2) + (path.includes('Temp') ? ' °C' : '');
                }
                else {
                    const unit = (path.includes('Ml') || path.includes('AS')) ? ' мл' : (path.includes('flows') ? ' мл/ч' : '');
                    el.textContent = Math.round(value) + unit;
                }
            }
        });
    }

    function handleHeadsFlowInput(e) {
        if (e.target.value) document.querySelector('[data-path="heads.targetCycles"]').value = '';
        updateUI();
    }
    function handleHeadsCyclesInput(e) {
        if (e.target.value) document.querySelector('[data-path="heads.targetFlowMlh"]').value = '';
        updateUI();
    }

    // Инициализация событий
    document.addEventListener('input', (e) => {
        if (e.target.dataset.path) updateUI();
    });

    window.onload = () => {
        setParamsToUI(defaultParams); // 1. Заполнили поля из константы
        updateUI();                   // 2. Сделали первый расчет
    };
</script>

</body>
</html>