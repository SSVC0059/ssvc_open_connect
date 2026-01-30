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
            --orange-100: #ffe8cc;
            --orange-200: #ffd8a8;
            --orange-400: #ffb366;
            --orange-600: #f79009;
            --orange-800: #b56900;
            --gray-50: #f8f9fa;
            --border-radius: 0.25rem;
            --border-radius-lg: 0.5rem;
            --shadow-sm: 0 1px 2px 0 rgb(0 0 0 / 0.05);
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            background-color: #f1f3f5;
            padding: 1rem;
            margin: 0 auto;
        }
        .editor-layout {
            display: flex;
            flex-direction: column;
            gap: 1rem;
        }
        .card {
            background-color: var(--surface-color);
            border-radius: var(--border-radius-lg);
            padding: 1.25rem;
            border: 1px solid var(--border-color);
        }
        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1rem;
        }
        .card-title {
            font-size: 1.125rem;
            font-weight: 600;
            color: var(--primary-800);
            margin: 0;
        }
        .settings-section {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
            gap: 0.5rem 1.5rem;
        }
        .reset-all-btn {
            font-size: 0.8rem;
            font-weight: 500;
            color: var(--text-muted-color);
            background: none;
            border: 1px solid var(--border-color);
            padding: 0.3rem 0.7rem;
            border-radius: var(--border-radius);
            cursor: pointer;
            transition: all 0.2s;
        }
        .reset-all-btn:hover {
            color: var(--primary-600);
            border-color: var(--primary-300);
        }
        .input-group {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 0.5rem 0;
        }
        .input-label {
            font-size: 0.9rem;
            color: var(--text-color);
            margin-right: 1rem;
        }
        .input-field {
            width: 6rem;
            padding: 0.3rem 0.5rem;
            border: 1px solid var(--border-color);
            border-radius: var(--border-radius);
            text-align: right;
        }
        .fraction-details {
            border-bottom: 1px solid var(--border-color);
        }
        .fraction-details:last-of-type {
            border-bottom: none;
        }
        .fraction-details summary {
            list-style: none;
            cursor: pointer;
            padding: 0;
            font-weight: 600;
            display: block;
            transition: background-color 0.2s;
        }
        .fraction-details summary:hover {
            background-color: var(--surface-hover-color);
        }
        .fraction-details summary::-webkit-details-marker {
            display: none;
        }
        .fraction-details .summary-content {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 0.75rem 0.25rem;
            border-radius: var(--border-radius);
        }
        .fraction-details .summary-content.active {
            color: var(--primary-800);
        }
        .fraction-details .summary-controls {
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }
        .fraction-details .chevron {
            transition: transform 0.2s ease-in-out;
            color: var(--text-muted-color);
            width: 1.25rem;
            height: 1.25rem;
        }
        .fraction-details[open] > summary .chevron {
            transform: rotate(180deg);
        }
        .fraction-body {
            background-color: var(--gray-50);
            border-top: 1px solid var(--border-color);
            padding: 1rem;
            display: grid;
            grid-template-columns: 1fr;
            gap: 1.5rem;
        }
        @media (min-width: 768px) {
            .fraction-body {
                grid-template-columns: 2fr 1fr;
            }
        }
        .settings-col, .results-col {
            display: flex;
            flex-direction: column;
            gap: 0.75rem;
        }
        .results-col {
            background-color: var(--surface-color);
            border: 1px dashed var(--primary-300);
            padding: 1rem;
            border-radius: var(--border-radius);
        }
        .col-title {
            font-size: 1rem;
            font-weight: 600;
            color: var(--primary-700);
            margin: 0 0 0.25rem;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 0.5rem;
        }
        .sub-settings-grid {
            display: grid;
            grid-template-columns: 1fr;
            gap: 1.5rem;
            margin-top: 0.5rem;
            border-top: 1px solid var(--border-color);
            padding-top: 1rem;
        }
        @media (min-width: 768px) {
            .sub-settings-grid {
                grid-template-columns: 1fr 1fr;
            }
        }
        .sub-settings-group {
            display: flex;
            flex-direction: column;
            gap: 0.25rem;
        }
        .sub-settings-title {
            font-size: 0.8rem;
            font-weight: 600;
            color: var(--primary-500);
            text-transform: uppercase;
            margin: 0 0 0.5rem;
        }
        .final-analytics-card {
            border-color: var(--blue-300);
            background-color: var(--blue-50);
        }
        .final-analytics-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 1rem;
        }
        .analytics-item {
            display: flex;
            flex-direction: column;
            background-color: var(--surface-alt-color);
            padding: 0.5rem 0.65rem;
            border-radius: var(--border-radius);
        }
        .analytics-label {
            font-size: 0.75rem;
            color: var(--text-muted-color);
            margin-bottom: 0.15rem;
        }
        .value-container {
            display: flex;
            align-items: baseline;
            gap: 0.4rem;
        }
        .analytics-value {
            font-size: 1rem;
            font-weight: 600;
            color: var(--primary-700);
        }
        .analytics-value.large {
            font-size: 1.15rem;
        }
        .initial-value {
            font-size: 0.75rem;
            color: var(--text-muted-color);
            font-weight: 400;
            text-decoration: line-through;
        }
    </style>
</head>
<body>

<div class="editor-layout" id="app">
    <!-- ======================================================================= -->
    <!-- ОБЩИЕ ПАРАМЕТРЫ                                                         -->
    <!-- ======================================================================= -->
    <div class="card">
        <div class="card-header">
            <h3 class="card-title">Параметры процесса</h3>
            <button id="reset-all-btn" class="reset-all-btn" title="Сбросить все настройки">Сбросить</button>
        </div>
        <div class="settings-section">
            <div class="input-group">
                <span class="input-label">Мощность, кВт</span>
                <input data-path="powerKw" class="input-field" type="number" step="0.1" />
            </div>
            <div class="input-group">
                <span class="input-label">Объем заливки, л</span>
                <input data-path="volumeL" class="input-field" type="number" step="0.5" />
            </div>
            <div class="input-group">
                <span class="input-label">Крепость, %</span>
                <input data-path="strengthVol" class="input-field" type="number" />
            </div>
            <div class="input-group">
                <span class="input-label">Стабилизация, мин</span>
                <input data-path="stabilizationMin" class="input-field" type="number" />
            </div>
        </div>
    </div>

    <!-- ======================================================================= -->
    <!-- НАСТРОЙКИ ФРАКЦИЙ                                                       -->
    <!-- ======================================================================= -->
    <div class="card">
        <h3 class="card-title" style="margin-bottom: 1rem;">Настройки отбора по фракциям</h3>

        <!-- Головы -->
        <details class="fraction-details" id="details-heads" open>
            <summary>
                <div class="summary-content" id="summary-heads">
                    <span>Головы</span>
                    <div class="summary-controls">
                        <input type="checkbox" data-path="heads.enabled" onclick="event.stopPropagation()" />
                        <svg class="chevron" xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m6 9 6 6 6-6"/></svg>
                    </div>
                </div>
            </summary>
            <div class="fraction-body fraction-settings">
                <div class="settings-col">
                    <h4 class="col-title">Параметры отбора</h4>
                    <div class="input-group"><span class="input-label">% от АС</span><input data-path="heads.percent" class="input-field" type="number" /></div>
                    <div class="input-group"><span class="input-label">Клапан, мл/ч</span><input data-path="heads.valveMlh" class="input-field" type="number" /></div>
                    <div class="input-group"><span class="input-label">Период ШИМ</span><input data-path="heads.pwmPeriod" class="input-field" type="number" /></div>
                    <div class="sub-settings-grid">
                        <div class="sub-settings-group">
                            <h5 class="sub-settings-title">Режим расчета (укажите одно)</h5>
                            <div class="input-group"><span class="input-label">Целевое кол-во циклов</span><input data-path="heads.targetCycles" class="input-field" type="number" placeholder="Авто (2)" step="0.1" oninput="handleHeadsCyclesInput(event)" /></div>
                            <div class="input-group"><span class="input-label">Желаемая скорость, мл/ч</span><input data-path="heads.targetFlowMlh" class="input-field" type="number" placeholder="Авто" oninput="handleHeadsFlowInput(event)" /></div>
                        </div>
                        <div class="sub-settings-group ssvc-grid">
                            <h5 class="sub-settings-title">Настройки SSVC</h5>
                            <div id="ssvc-heads" class="input-group"><span class="input-label">Период, с</span><input type="number" data-path="heads.period" class="input-field" step="1"></div>
                            <div class="input-group"><span class="input-label">Открытие, мс</span><input type="number" data-path="heads.openTime" class="input-field" step="0.1"></div>
                        </div>
                    </div>
                </div>
                <div class="results-col">
                    <h4 class="col-title">Расчетные значения</h4>
                    <div id="an-fractions-headsMl" class="analytics-item"><span class="analytics-label">Объем</span><div class="value-container"><span class="analytics-value"></span></div></div>
                    <div id="an-flows-heads" class="analytics-item"><span class="analytics-label">Скорость</span><div class="value-container"><span class="analytics-value"></span><span class="initial-value"></span></div></div>
                    <div id="an-timers-heads_timer" class="analytics-item"><span class="analytics-label">Время</span><div class="value-container"><span class="analytics-value"></span></div></div>
                    <div id="an-oneCycleTime" class="analytics-item"><span class="analytics-label">Время переиспарения</span><div class="value-container"><span class="analytics-value"></span></div></div>
                </div>
            </div>
        </details>

        <!-- Подголовники -->
        <details class="fraction-details" id="details-subHeads">
            <summary>
                <div class="summary-content" id="summary-subHeads">
                    <span>Подголовники</span>
                    <div class="summary-controls">
                        <input type="checkbox" data-path="subHeads.enabled" onclick="event.stopPropagation()" />
                        <svg class="chevron" xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m6 9 6 6 6-6"/></svg>
                    </div>
                </div>
            </summary>
            <div class="fraction-body fraction-settings">
                <div class="settings-col">
                    <h4 class="col-title">Параметры отбора</h4>
                    <div class="input-group"><span class="input-label">% от АС</span><input data-path="subHeads.percent" class="input-field" type="number" /></div>
                    <div class="input-group"><span class="input-label">Клапан, мл/ч</span><input data-path="subHeads.valveMlh" class="input-field" type="number" /></div>
                    <div class="input-group"><span class="input-label">Период ШИМ</span><input data-path="subHeads.pwmPeriod" class="input-field" type="number" /></div>
                    <div class="sub-settings-grid">
                        <div class="sub-settings-group">
                            <h5 class="sub-settings-title">Режим расчета</h5>
                            <div class="input-group"><span class="input-label">Желаемая скорость, мл/ч</span><input data-path="subHeads.targetFlowMlh" class="input-field" type="number" placeholder="Авто" /></div>
                        </div>
                        <div class="sub-settings-group ssvc-grid">
                            <h5 class="sub-settings-title">Настройки SSVC</h5>
                            <div id="ssvc-subHeads" class="input-group"><span class="input-label">Период, с</span><input type="number" data-path="subHeads.period" class="input-field" step="1"></div>
                            <div class="input-group"><span class="input-label">Открытие, мс</span><input type="number" data-path="subHeads.openTime" class="input-field" step="0.1"></div>
                        </div>
                    </div>
                </div>
                <div class="results-col">
                    <h4 class="col-title">Расчетные значения</h4>
                    <div id="an-fractions-subHeadsMl" class="analytics-item"><span class="analytics-label">Объем</span><div class="value-container"><span class="analytics-value"></span></div></div>
                    <div id="an-flows-subHeads" class="analytics-item"><span class="analytics-label">Скорость</span><div class="value-container"><span class="analytics-value"></span><span class="initial-value"></span></div></div>
                    <div id="an-timers-subHeads_timer" class="analytics-item"><span class="analytics-label">Время</span><div class="value-container"><span class="analytics-value"></span></div></div>
                </div>
            </div>
        </details>

        <!-- Тело -->
        <details class="fraction-details" open>
            <summary>
                <div class="summary-content active">
                    <span>Тело</span>
                    <svg class="chevron" xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m6 9 6 6 6-6"/></svg>
                </div>
            </summary>
            <div class="fraction-body fraction-settings">
                <div class="settings-col">
                    <h4 class="col-title">Параметры отбора</h4>
                    <div class="input-group"><span class="input-label">Снижение скорости, %</span><input data-path="body.decrementPercent" class="input-field" type="number" /></div>
                    <div class="input-group"><span class="input-label">Клапан, мл/ч</span><input data-path="body.valveMlh" class="input-field" type="number" /></div>
                    <div class="input-group"><span class="input-label">Период ШИМ</span><input data-path="body.pwmPeriod" class="input-field" type="number" /></div>
                    <div class="sub-settings-grid">
                        <div class="sub-settings-group">
                            <h5 class="sub-settings-title">Начальная скорость (укажите одно)</h5>
                            <div class="input-group"><span class="input-label">По умолчанию, мл/ч</span><input data-path="body.initialSpeedMlh" class="input-field" type="number" placeholder="1800" oninput="handleBodyInitialSpeedInput(event)" /></div>
                            <div class="input-group"><span class="input-label">Желаемая, мл/ч</span><input data-path="body.targetFlowMlh" class="input-field" type="number" placeholder="Авто" oninput="handleBodyTargetFlowInput(event)" /></div>
                        </div>
                        <div class="sub-settings-group ssvc-grid">
                            <h5 class="sub-settings-title">Настройки SSVC</h5>
                            <div id="ssvc-body" class="input-group"><span class="input-label">Период, с</span><input type="number" data-path="body.period" class="input-field" step="1"></div>
                            <div class="input-group"><span class="input-label">Открытие, мс</span><input type="number" data-path="body.openTime" class="input-field" step="0.1"></div>
                        </div>
                    </div>
                </div>
                <div class="results-col">
                    <h4 class="col-title">Расчетные значения</h4>
                    <div id="an-fractions-bodyMl" class="analytics-item"><span class="analytics-label">Объем</span><div class="value-container"><span class="analytics-value"></span></div></div>
                    <div id="an-flows-body" class="analytics-item"><span class="analytics-label">Скорость</span><div class="value-container"><span class="analytics-value"></span><span class="initial-value"></span></div></div>
                    <div id="an-timers-body_timer" class="analytics-item"><span class="analytics-label">Время</span><div class="value-container"><span class="analytics-value"></span></div></div>
                </div>
            </div>
        </details>

        <!-- Хвосты -->
        <details class="fraction-details" id="details-tails">
            <summary>
                <div class="summary-content" id="summary-tails">
                    <span>Хвосты</span>
                    <div class="summary-controls">
                        <input type="checkbox" data-path="tails.enabled" onclick="event.stopPropagation()" />
                        <svg class="chevron" xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m6 9 6 6 6-6"/></svg>
                    </div>
                </div>
            </summary>
            <div class="fraction-body fraction-settings">
                <div class="settings-col">
                    <h4 class="col-title">Параметры отбора</h4>
                    <div class="input-group"><span class="input-label">% от АС</span><input data-path="tails.percent" class="input-field" type="number" /></div>
                    <div class="input-group"><span class="input-label">Клапан, мл/ч</span><input data-path="tails.valveMlh" class="input-field" type="number" /></div>
                    <div class="input-group"><span class="input-label">Период ШИМ</span><input data-path="tails.pwmPeriod" class="input-field" type="number" /></div>
                    <div class="sub-settings-grid">
                        <div class="sub-settings-group">
                            <h5 class="sub-settings-title">Режим расчета</h5>
                            <div class="input-group"><span class="input-label">Желаемая скорость, мл/ч</span><input data-path="tails.targetFlowMlh" class="input-field" type="number" placeholder="Авто" /></div>
                        </div>
                        <div class="sub-settings-group ssvc-grid">
                            <h5 class="sub-settings-title">Настройки SSVC</h5>
                            <div id="ssvc-tails" class="input-group"><span class="input-label">Период, с</span><input type="number" data-path="tails.period" class="input-field" step="1"></div>
                            <div class="input-group"><span class="input-label">Открытие, мс</span><input type="number" data-path="tails.openTime" class="input-field" step="0.1"></div>
                        </div>
                    </div>
                </div>
                <div class="results-col">
                    <h4 class="col-title">Расчетные значения</h4>
                    <div id="an-fractions-tailsMl" class="analytics-item"><span class="analytics-label">Объем</span><div class="value-container"><span class="analytics-value"></span></div></div>
                    <div id="an-flows-tails" class="analytics-item"><span class="analytics-label">Скорость</span><div class="value-container"><span class="analytics-value"></span><span class="initial-value"></span></div></div>
                    <div id="an-timers-tails_timer" class="analytics-item"><span class="analytics-label">Время</span><div class="value-container"><span class="analytics-value"></span></div></div>
                </div>
            </div>
        </details>
    </div>

    <!-- ======================================================================= -->
    <!-- ИТОГОВАЯ АНАЛИТИКА                                                      -->
    <!-- ======================================================================= -->
    <div class="card final-analytics-card">
        <h3 class="card-title">Итоговая аналитика процесса</h3>
        <div class="final-analytics-grid">
            <div id="an-totalAS" class="analytics-item"><span class="analytics-label">Общий АС</span><div class="value-container"><span class="analytics-value"></span></div></div>
            <div id="an-boilingTemp" class="analytics-item"><span class="analytics-label">T° кипения</span><div class="value-container"><span class="analytics-value"></span></div></div>
            <div id="an-residueMl" class="analytics-item"><span class="analytics-label">Остаток в кубе</span><div class="value-container"><span class="analytics-value"></span></div></div>
            <div id="an-total_process_timer" class="analytics-item"><span class="analytics-label">Общее время</span><div class="value-container"><span class="analytics-value large"></span></div></div>
        </div>
    </div>
</div>

<script src="rectification_calculator.js"></script>
<script>
    // Mutual exclusion handlers for Heads
    function handleHeadsFlowInput(e) {
        if (e.target.value) {
            const targetCyclesInput = document.querySelector('[data-path="heads.targetCycles"]');
            if (targetCyclesInput) {
                targetCyclesInput.value = '';
                // Manually trigger input event to notify the calculator
                targetCyclesInput.dispatchEvent(new Event('input', { bubbles: true }));
            }
        }
    }

    function handleHeadsCyclesInput(e) {
        if (e.target.value) {
            const targetFlowMlhInput = document.querySelector('[data-path="heads.targetFlowMlh"]');
            if (targetFlowMlhInput) {
                targetFlowMlhInput.value = '';
                targetFlowMlhInput.dispatchEvent(new Event('input', { bubbles: true }));
            }
        }
    }

    // Mutual exclusion handlers for Body
    function handleBodyInitialSpeedInput(e) {
        if (e.target.value) {
            const targetFlowMlhInput = document.querySelector('[data-path="body.targetFlowMlh"]');
            if (targetFlowMlhInput) {
                targetFlowMlhInput.value = '';
                targetFlowMlhInput.dispatchEvent(new Event('input', { bubbles: true }));
            }
        }
    }

    function handleBodyTargetFlowInput(e) {
        if (e.target.value) {
            const initialSpeedMlhInput = document.querySelector('[data-path="body.initialSpeedMlh"]');
            if (initialSpeedMlhInput) {
                initialSpeedMlhInput.value = '';
                initialSpeedMlhInput.dispatchEvent(new Event('input', { bubbles: true }));
            }
        }
    }
</script>

</body>
</html>
