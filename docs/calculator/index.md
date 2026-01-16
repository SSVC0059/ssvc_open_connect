---
hide:
  - navigation
---

# Калькулятор ректификации

!!! info
    Калькулятор расчитывает рекомендуемые настройки на основании реальной мощности колонны.
    Это не чудо-таблетка и все вопросы она не решит. Используйте его как отправную точку и адаптируйте настройки под свое оборудование 
    
    
    Обсуждение функционала калькулятора в TG группе -> [ТУТ](https://t.me/demoncat_home/9547)

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
                --red-100: #fff5f5;
                --red-300: #ffc9c9;
                --red-600: #fa5252;
                --border-radius: 0.25rem;
                --border-radius-lg: 0.5rem;
                --shadow-sm: 0 1px 2px 0 rgb(0 0 0 / 0.05);
            }
            
            body {
                margin: 0;
                padding: 0.5rem;
                font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
                font-size: 14px;
                line-height: 1.4;
                background-color: var(--gray-50);
                min-height: 100vh;
                box-sizing: border-box;
            }
            
            .editor-layout { 
                display: flex; 
                flex-direction: column; 
                gap: 0.75rem; 
                max-width: 1200px;
                margin: 0 auto;
            }
            
            .card { 
                background-color: var(--surface-color); 
                border-radius: var(--border-radius-lg); 
                padding: 1rem; 
                border: 1px solid var(--border-color);
                box-shadow: var(--shadow-sm);
            }
            
            .card-header { 
                display: flex; 
                justify-content: space-between; 
                align-items: center; 
                margin-bottom: 1rem; 
                flex-wrap: wrap;
                gap: 0.5rem;
            }
            
            .card-title { 
                font-size: 1rem; 
                font-weight: 600; 
                color: var(--primary-800); 
                margin: 0; 
            }
            
            .settings-section { 
                display: grid; 
                grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); 
                gap: 0.5rem;
            }
            
            @media (max-width: 480px) {
                .settings-section {
                    grid-template-columns: 1fr;
                    gap: 0.2rem;
                }
                
                .card {
                    padding: 0.2rem;
                }
                
                .editor-layout {
                    gap: 0.2rem;
                }
            }
            
            .input-group { 
                display: flex; 
                justify-content: space-between; 
                align-items: center; 
                padding: 0.4rem 0;
                flex-wrap: wrap;
                gap: 0.5rem;
            }
            
            .input-label { 
                font-size: 0.85rem; 
                color: var(--text-color); 
                margin-right: 0.5rem;
                flex: 1;
                min-width: 120px;
            }
            
            .input-field { 
                width: 5.5rem; 
                min-width: 5.5rem;
                padding: 0.35rem 0.5rem; 
                border: 1px solid var(--border-color); 
                border-radius: var(--border-radius); 
                text-align: right;
                font-size: 0.9rem;
                box-sizing: border-box;
            }
            
            @media (max-width: 360px) {
                .input-field {
                    width: 100%;
                    min-width: 0;
                }
                
                .input-group {
                    flex-direction: column;
                    align-items: flex-start;
                    gap: 0.25rem;
                }
                
                .input-label {
                    min-width: auto;
                    width: 100%;
                }
            }
            
            .fraction-details { 
                border-bottom: 1px solid var(--border-color); 
                margin-bottom: 0.5rem;
                position: relative;
            }
            
            /* КОМПЛЕКСНОЕ РЕШЕНИЕ: полностью скрываем стандартный маркер */
            .fraction-details summary {
                display: block; /* Меняем display с inline-block на block */
                list-style: none; /* Убираем стандартную точку/маркер */
                cursor: pointer;
                padding: 0.75rem 0.25rem;
                font-weight: 600;
                position: relative;
            }
            
            /* Для WebKit браузеров */
            .fraction-details summary::-webkit-details-marker {
                display: none !important;
            }
            
            /* Для Firefox и других браузеров */
            .fraction-details summary::marker {
                display: none !important;
                content: '' !important;
            }
            
            /* Дополнительная гарантия для всех браузеров */
            .fraction-details > summary {
                list-style-type: none;
                -webkit-appearance: none;
                -moz-appearance: none;
                appearance: none;
            }
            
            .summary-content { 
                display: flex; 
                justify-content: space-between; 
                align-items: center;
                flex-wrap: wrap;
                gap: 0.5rem;
            }
            
            .fraction-body { 
                background-color: var(--gray-50); 
                border-top: 1px solid var(--border-color); 
                padding: 0.75rem; 
                display: grid; 
                grid-template-columns: 1fr; 
                gap: 1rem;
            }
            
            @media (min-width: 768px) { 
                .fraction-body { 
                    grid-template-columns: 1fr 1fr; 
                    gap: 1rem;
                } 
            }
            
            @media (max-width: 480px) {
                .fraction-body {
                    padding: 0.5rem;
                    gap: 0.2rem;
                }
                
                .sub-settings-title {
                    margin-top: 0.2rem !important;
                }
            }
            
            .results-col { 
                background-color: var(--surface-color); 
                border: 1px dashed var(--primary-300); 
                padding: 0.75rem; 
                border-radius: var(--border-radius); 
                display: flex; 
                flex-direction: column; 
                gap: 0.4rem;
            }
            
            .analytics-item { 
                display: flex; 
                justify-content: space-between; 
                align-items: center; 
                padding: 0.35rem 0.5rem; 
                background: var(--surface-alt-color); 
                border-radius: 4px;
                flex-wrap: wrap;
                gap: 0.5rem;
            }
            
            .analytics-label { 
                font-size: 0.78rem; 
                color: var(--text-muted-color);
                flex: 1;
                min-width: 100px;
            }
            
            .analytics-value { 
                font-size: 0.85rem; 
                font-weight: 600; 
                color: var(--primary-700);
                text-align: right;
                min-width: 80px;
            }
            
            .highlight .analytics-value { 
                color: var(--orange-600); 
            }
            
            /* FIXED: ssvc-result-card без переносов */
            .ssvc-result-card { 
                margin-top: 0.5rem; 
                padding: 0.5rem; 
                background: var(--primary-100); 
                border: 1px solid var(--blue-300); 
                border-radius: 6px;
                white-space: nowrap; /* Запрещаем перенос текста */
                overflow: hidden; /* Скрываем переполнение */
            }
            
            .ssvc-result-card .analytics-item {
                white-space: nowrap; /* Запрещаем перенос в дочерних элементах */
                flex-wrap: nowrap; /* Запрещаем перенос flex-элементов */
                overflow: hidden;
                text-overflow: ellipsis;
            }
            
            .ssvc-result-card .analytics-label,
            .ssvc-result-card .analytics-value {
                white-space: nowrap;
                overflow: hidden;
                text-overflow: ellipsis;
                min-width: auto;
                flex-shrink: 1;
            }
            
            .sub-settings-title { 
                font-size: 0.7rem; 
                font-weight: 600; 
                color: var(--primary-500); 
                text-transform: uppercase; 
                margin-bottom: 0.5rem; 
                border-bottom: 1px solid var(--primary-300);
                padding-bottom: 0.25rem;
                white-space: nowrap; /* Запрещаем перенос заголовка */
                overflow: hidden;
                text-overflow: ellipsis;
            }
            
            .final-analytics-card { 
                border-color: var(--blue-300) !important; 
                background-color: var(--primary-100) !important; 
            }
            
            .final-analytics-grid { 
                display: grid; 
                grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); 
                gap: 0.75rem;
            }
            
            @media (max-width: 480px) {
                .final-analytics-grid {
                    grid-template-columns: 1fr;
                    gap: 0.5rem;
                }
            }
                        
            #fraction-warning {
                display: none;
                width: 100%;
                margin-top: 0.5rem;
                color: var(--red-600);
                font-size: 0.85rem;
                font-weight: 600;
                background-color: var(--red-100);
                padding: 0.4rem 0.75rem;
                border-radius: var(--border-radius);
                border: 1px solid var(--red-300);
                text-align: center;
            }
            
            .actions-panel { 
                display: flex; 
                justify-content: flex-end; 
                padding: 1rem 0; 
            }
            
            .btn-primary {
                padding: 0.5rem 1.25rem;
                border-radius: var(--border-radius);
                font-weight: 600;
                cursor: pointer;
                background-color: var(--primary-500);
                color: var(--surface-color);
                border: 1px solid var(--primary-500);
                transition: background-color 0.2s;
                font-size: 0.9rem;
            }
            
            .btn-primary:hover { 
                background-color: var(--primary-600); 
            }
            
            .btn-primary:disabled {
                background-color: #6c757d;
                border-color: #6c757d;
                cursor: not-allowed;
            }
            
            /* Добавляем отступы для контента внутри summary */
            .summary-content > span {
                flex: 1;
            }
            
            /* Улучшаем отображение чекбоксов на мобильных */
            input[type="checkbox"] {
                width: 0.8rem;
                margin: 0;
            }
            
            /* Для очень маленьких экранов - специальные правки для ssvc-result-card */
            @media (max-width: 320px) {
                body {
                    padding: 0.25rem;
                    font-size: 13px;
                }
                
                .card {
                    padding: 0.5rem;
                }
                
                .input-field {
                    padding: 0.25rem 0.4rem;
                }
                
                .analytics-item {
                    padding: 0.25rem 0.4rem;
                }
                
                .ssvc-result-card {
                    padding: 0.4rem;
                }
                
                .ssvc-result-card .analytics-label {
                    font-size: 0.72rem;
                    min-width: 50px;
                }
                
                .ssvc-result-card .analytics-value {
                    font-size: 0.78rem;
                    min-width: 60px;
                }
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
                    </label>
                    </div>
                    <div class="input-group"><span class="input-label">Объем заливки, л</span><label>
                        <input data-path="volumeL" class="input-field" type="number" step="0.5" value="18">
                    </label>
                    </div>
                    <div class="input-group"><span class="input-label">Крепость, %</span><label>
                        <input data-path="strengthVol" class="input-field" type="number" value="40">
                    </label>
                    </div>
                    <div class="input-group"><span class="input-label">Стабилизация, мин</span><label>
                        <input data-path="stabilizationMin" class="input-field" type="number" value="30">
                    </label>
                    </div>
                </div>
            </div>
            <div class="card">
                <div class="card-header">
                    <h3 class="card-title">Настройки отбора</h3>
                    <div id="fraction-warning"></div>
                </div>
                <!--        Головы       -->
                <details class="fraction-details" id="details-heads">
                    <summary>
                        <div class="summary-content">
                            <span>Головы</span>
                            <div style="display:flex; gap:10px; align-items:center;">
                                <label>
                                    <input type="checkbox" data-path="heads.enabled" checked onclick="event.stopPropagation()">
                                </label>
                            </div>
                        </div>
                    </summary>
                    <div class="fraction-body">
                        <div class="settings-col">
                            <div class="input-group"><span class="input-label">% от АС</span>
                                <label>
                                    <input data-path="heads.percent" class="input-field" type="number" value="3">
                                </label>
                            </div>
                            <div class="input-group"><span class="input-label">Клапан (мл/ч)</span>
                                <label>
                                    <input data-path="ssvcSettings.valve_bw[0]" class="input-field" type="number">
                                </label>
                            </div>
                            <div class="sub-settings-title" style="margin-top:10px">Режим расчета</div>
                            <div class="input-group"><span class="input-label">Количество переиспарений</span>
                                <label>
                                    <input data-path="heads.targetCycles" class="input-field" type="number" step="0.1" value="2" oninput="handleHeadsCyclesInput(event)">
                                </label>
                            </div>
                            <div class="input-group"><span class="input-label">Желаемая скорость, мл/ч</span>
                                <label>
                                    <input data-path="heads.targetFlowMlh" class="input-field" type="number" oninput="handleHeadsFlowInput(event)">
                                </label>
                            </div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Период SSVC (сек)</span>
                                <label>
                                    <input data-path="ssvcSettings.heads.1" class="input-field" type="number">
                                </label>
                            </div>
                            <div class="sub-settings-title" style="margin-top:10px">Сброс и снижение</div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Продолжительность сброса (сек)</span>
                                <label>
                                    <input data-path="ssvcSettings.release_timer" class="input-field" type="number">
                                </label>
                            </div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Время сброса (сек)</span>
                                <label>
                                    <input data-path="ssvcSettings.release_speed" class="input-field" type="number">
                                </label>
                            </div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Время открытия в конце этапа (сек)</span>
                                <label>
                                    <input data-path="ssvcSettings.heads_final" class="input-field" type="number">
                                </label>
                            </div>
                            
                        </div>
                        <div class="results-col">
                            <div class="analytics-item">
                                <span class="analytics-label">Объем фракции</span>
                                <span class="analytics-value" data-result="analytics.fractions.headsMl">0 мл</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Объем сброса</span>
                                <span class="analytics-value" data-result="analytics.fractions.releaseMl"></span>
                            </div>
                            <div class="analytics-item highlight">
                                <span class="analytics-label">Скорость отбора</span>
                                <span class="analytics-value" data-result="analytics.flows.heads">0 мл/ч</span>
                            </div>

                            <div class="analytics-item highlight">
                                <span class="analytics-label">Скорость сброса</span>
                                <span class="analytics-value" data-result="analytics.flows.heads_release">0 мл/ч</span>
                            </div>
                            <div class="analytics-item highlight">
                                <span class="analytics-label">Финальная скорость</span>
                                <span class="analytics-value" data-result="analytics.flows.heads_final">0 мл/ч</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Флегмовое число</span>
                                <span class="analytics-value" data-result="analytics.phlegmatic.heads"></span>
                            </div>

                            <div class="analytics-item">
                                <span class="analytics-label">Время этапа</span>
                                <span class="analytics-value" data-result="analytics.timers.heads">00:00:00</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Время переиспарения</span>
                                <span class="analytics-value" data-result="analytics.oneCycleTime">00:00:00</span>
                            </div>
                            <div class="ssvc-result-card">
                                <div class="sub-settings-title">Клапан SSVC</div>
                                <div class="analytics-item">
                                    <span class="analytics-label">Время</span>
                                    <span class="analytics-value" data-result="ssvcSettings.heads.0">0.00 сек</span>
                                </div>
                                <div class="analytics-item">
                                    <span class="analytics-label">Период</span>
                                    <span class="analytics-value" data-result="ssvcSettings.heads.1">0.00 сек</span>
                                </div>
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
                                <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                    <path d="M6 9l6 6 6-6"/>
                                </svg>
                            </div>
                        </div>
                    </summary>
                    <div class="fraction-body">
                        <div class="settings-col">
                            <div class="input-group"><span class="input-label">% от АС</span>
                                <input data-path="late_heads.percent" class="input-field" type="number" value="7">
                            </div>
                            <div class="input-group"><span class="input-label">Клапан (мл/ч)</span>
                                <input data-path="virtual_bw_late" class="input-field" type="number">
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
                                <span class="analytics-value" data-result="analytics.phlegmatic.late_heads">0</span>
                            </div>
                            <div class="analytics-item"><span class="analytics-label">Время этапа</span>
                                <span class="analytics-value" data-result="analytics.timers.late_heads">00:00:00</span>
                            </div>
                            <div class="ssvc-result-card">
                                <div class="sub-settings-title">Клапан SSVC</div>
                                <div class="analytics-item">
                                    <span class="analytics-label">Время</span>
                                    <span class="analytics-value" data-result="ssvcSettings.late_heads.0">0.00 сек</span>
                                </div>
                                <div class="analytics-item">
                                    <span class="analytics-label">Период</span>
                                    <span class="analytics-value" data-result="ssvcSettings.late_heads.1">0.00 сек</span>
                                </div>
                            </div>
                        </div>
                    </div>
                </details>
                <details class="fraction-details" open>
                    <summary>
                        <div class="summary-content">
                            <span>Тело</span>
                        </div>
                    </summary>
                    <div class="fraction-body">
                        <div class="settings-col">
                            <div class="input-group">
                                <span class="input-label">Стоп по Т куба, °C</span>
                                <label>
                                    <input data-path="ssvcSettings.hearts_finish_temp" class="input-field" type="number" value="80" oninput="handleHeartsTempInput(event)">
                                </label>
                            </div>
                            <div class="input-group">
                                <span class="input-label">Отбор (% от АС)</span>
                                <label>
                                    <input data-path="hearts.percent" class="input-field" type="number" value="80" oninput="handleHeartsPercentInput(event)">
                                </label>
                            </div>
                            <div class="input-group">
                                <span class="input-label">Клапан (мл/ч)</span>
                                <input data-path="ssvcSettings.valve_bw[1]" class="input-field" type="number" value="1500">
                            </div>
                            <div class="input-group">
                                <span class="input-label">Желаемая скорость (мл/ч)</span>
                                <input data-path="hearts.targetFlowMlh" class="input-field" type="number" value="1000">
                            </div>
                            <div class="input-group">
                                <span class="input-label">Период SSVC (сек)</span>
                                <input data-path="ssvcSettings.hearts[1]" class="input-field" type="number" value="10">
                            </div>
                        </div>


                        <div class="results-col">
                            <div class="analytics-item">
                                <span class="analytics-label">Объем</span>
                                <span class="analytics-value" data-result="analytics.fractions.heartsMl">0 мл</span>
                            </div>
                            <div class="analytics-item highlight">
                                <span class="analytics-label">Скорость</span>
                                <span class="analytics-value" data-result="analytics.flows.hearts">0 мл/ч</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Флегмовое число</span>
                                <span class="analytics-value" data-result="analytics.phlegmatic.hearts">0</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Время этапа</span>
                                <span class="analytics-value" data-result="analytics.timers.hearts">00:00:00</span>
                            </div>
                            <div class="ssvc-result-card">
                                <div class="sub-settings-title">Клапан SSVC</div>
                                <div class="analytics-item">
                                    <span class="analytics-label">Время</span>
                                    <span class="analytics-value" data-result="ssvcSettings.hearts.0">0.00 сек</span>
                                </div>
                                <div class="analytics-item">
                                    <span class="analytics-label">Период</span>
                                    <span class="analytics-value" data-result="ssvcSettings.hearts.1">0.00 сек</span>
                                </div>
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
                            <div class="input-group" style="margin-top:10px"><span class="input-label">Период SSVC (сек)</span>
                                <label>
                                    <input data-path="ssvcSettings.tails[1]" class="input-field" type="number" value="10">
                                </label>
                            </div>
                            <div class="sub-settings-title" style="margin-top:10px">Скорость и оборудование</div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Клапан, мл/ч</span>
                                <label>
                                    <input data-path="virtual_bw_tails" class="input-field" type="number">
                                </label>
                            </div>
                            <div class="input-group"><span class="input-label">Начальная скорость отбора, мл/ч</span>
                                <label>
                                    <input data-path="tails.targetFlowMlh" class="input-field" type="number" placeholder="Максимум">
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
                                <span class="analytics-value" data-result="analytics.phlegmatic.tails">0</span>
                            </div>
                            <div class="analytics-item"><span class="analytics-label">Время этапа</span>
                                <span class="analytics-value" data-result="analytics.timers.tails">00:00:00</span>
                            </div>
                            <div class="ssvc-result-card">
                                <div class="sub-settings-title">Клапан SSVC</div>
                                <div class="analytics-item">
                                    <span class="analytics-label">Время</span>
                                    <span class="analytics-value" data-result="ssvcSettings.tails.0">0.00 сек</span>
                                </div>
                                <div class="analytics-item">
                                    <span class="analytics-label">Период</span>
                                    <span class="analytics-value" data-result="ssvcSettings.tails.1">0.00 сек</span>
                                </div>
                            </div>
                        </div>
                    </div>
                </details>
            </div>
            <div class="card final-analytics-card">
                <h3 class="card-title">Итоговая аналитика</h3>
                <div class="final-analytics-grid">
                    <div class="analytics-item" id="an-totalAS">
                        <span class="analytics-label">Общий АС</span>
                        <span class="analytics-value" data-result="analytics.totalAS">0 мл</span>
                    </div>
                    <div class="analytics-item" id="an-boilingTemp">
                        <span class="analytics-label">T° кипения</span>
                        <span class="analytics-value" data-result="analytics.boilingTemp">0 °C</span>
                    </div>
                    <div class="analytics-item" id="an-residueMl">
                        <span class="analytics-label">Остаток в кубе</span>
                        <span class="analytics-value" data-result="analytics.residueMl">0 мл</span>
                    </div>
                    <div class="analytics-item" id="an-residualFortress">
                        <span class="analytics-label">Остаточная крепость</span>
                        <span class="analytics-value" data-result="analytics.residualFortress">0 %</span>
                    </div>
                    <div class="analytics-item" id="an-total_process_timer">
                        <span class="analytics-label">Общее время</span>
                        <span class="analytics-value" data-result="analytics.timers.total_process">00:00:00</span>
                    </div>
                </div>
            </div>
        </div>
        <script src="rectification_calculator.js"></script>
    </body>
</html>