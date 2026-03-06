# Калькулятор ректификации

<!DOCTYPE html>
<html lang="ru">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Калькулятор ректификации</title>
    </head>
    <body>
        <div class="editor-layout" id="app">
             <div class="calculator-subtitle">
                Калькулятор расчитывает рекомендуемые настройки на основании реальной мощности колонны.
                Это не чудо-таблетка и все вопросы она не решит. Используйте его как отправную точку и адаптируйте настройки под свое оборудование.
            </div>
            <div class="openconnect-buttons" style="margin-top: 2rem;">
                <a href="https://t.me/demoncat_home/9547" class="openconnect-btn openconnect-btn-secondary" target="_blank">
                    💬 Обсуждение в Telegram
                </a>
            </div>

            <div class="card">
                <div class="card-header">
                    <h3>Параметры процесса</h3>
                </div>
                <div class="settings-section">
                    <div class="input-group">
                        <span class="input-label">Мощность, кВт</span><label>
                        <input data-path="powerKw" class="input-field" type="number" step="0.1" value="1">
                    </label>
                    </div>
                    <div class="input-group">
                        <span class="input-label">Объем заливки, л</span><label>
                        <input data-path="volumeL" class="input-field" type="number" step="0.5" value="18">
                    </label>
                    </div>
                    <div class="input-group">
                        <span class="input-label">Крепость, %</span><label>
                        <input data-path="strengthVol" class="input-field" type="number" value="40">
                    </label>
                    </div>
                    <div class="input-group">
                        <span class="input-label">Стабилизация, мин</span><label>
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
                <details class="fraction-details" id="details-heads" open>
                    <summary>
                        <div class="summary-content">
                            <span>Головы</span>
                        </div>
                    </summary>
                    <div class="fraction-body">
                        <div class="settings-col">
                            <div class="input-group">
                                <span class="input-label">% от АС</span>
                                <input data-path="heads.percent" class="input-field" type="number" value="3">
                            </div>
                            <div class="input-group"><span class="input-label">Клапан (мл/ч)</span>
                                <input data-path="ssvcSettings.valve_bw[0]" class="input-field" type="number">
                            </div>
                            <div class="sub-settings-title" style="margin-top:10px">Режим расчета</div>
                            <div class="input-group"><span class="input-label">Количество переиспарений</span>
                                <input data-path="heads.targetCycles" class="input-field" type="number" step="0.1" value="2" oninput="handleHeadsCyclesInput(event)">
                            </div>
                            <div class="input-group"><span class="input-label">Желаемая скорость, мл/ч</span>
                                <input data-path="heads.targetFlowMlh" class="input-field" type="number" oninput="handleHeadsFlowInput(event)">
                            </div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Период SSVC (сек)</span>
                                <input data-path="ssvcSettings.heads.1" class="input-field" type="number">
                            </div>
                            <div class="sub-settings-title" style="margin-top:10px">Сброс и снижение</div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Продолжительность сброса (сек)</span>
                                <input data-path="ssvcSettings.release_timer" class="input-field" type="number">
                            </div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Время сброса (сек)</span>
                                <input data-path="ssvcSettings.release_speed" class="input-field" type="number">
                            </div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Время открытия в конце этапа (сек)</span>
                                <input data-path="ssvcSettings.heads_final" class="input-field" type="number">
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
                            <div class="analytics-item">
                                <span class="analytics-label">Скорость отбора</span>
                                <span class="analytics-value" data-result="analytics.flows.heads">0 мл/ч</span>
                            </div>

                            <div class="analytics-item">
                                <span class="analytics-label">Скорость сброса</span>
                                <span class="analytics-value" data-result="analytics.flows.heads_release">0 мл/ч</span>
                            </div>
                            <div class="analytics-item">
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
                            <div class="input-group">
                                <span class="input-label">% от АС</span>
                                <input data-path="late_heads.percent" class="input-field" type="number" value="7">
                            </div>
                            <div class="input-group">
                                <span class="input-label">Клапан (мл/ч)</span>
                                <input data-path="virtual_bw_late" class="input-field" type="number">
                            </div>
                            <div class="input-group">
                                <span class="input-label">Желаемая скорость, мл/ч</span>
                                <input data-path="late_heads.targetFlowMlh" class="input-field" type="number" placeholder="Авто (10%)">
                            </div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Период SSVC (сек)</span>
                                <input data-path="ssvcSettings.late_heads[1]" class="input-field" type="number">
                            </div>
                        </div>
                        <div class="results-col">
                            <div class="analytics-item">
                                <span class="analytics-label">Объем</span>
                                <span class="analytics-value" data-result="analytics.fractions.lateHeadsMl">0 мл</span>
                            </div>
                            <div class="analytics-item ">
                                <span class="analytics-label">Скорость</span>
                                <span class="analytics-value" data-result="analytics.flows.late_heads">0 мл/ч</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Флегмовое число</span>
                                <span class="analytics-value" data-result="analytics.phlegmatic.late_heads">0</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Время этапа</span>
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
                                <input data-path="ssvcSettings.hearts_finish_temp" class="input-field" type="number" value="80" oninput="handleHeartsTempInput(event)">
                            </div>
                            <div class="input-group">
                                <span class="input-label">Отбор (% от АС)</span>
                                <input data-path="hearts.percent" class="input-field" type="number" value="80" oninput="handleHeartsPercentInput(event)">
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
                            <div class="analytics-item ">
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
                                <input type="checkbox" data-path="tails.enabled" onclick="event.stopPropagation()">
                            </div>
                        </div>
                    </summary>
                    <div class="fraction-body">
                        <div class="settings-col">
                            <div class="input-group"><span class="input-label">% от АС</span>
                                <input data-path="tails.percent" class="input-field" type="number" value="5">
                            </div>
                            <div class="input-group" style="margin-top:10px"><span class="input-label">Период SSVC (сек)</span>
                                <input data-path="ssvcSettings.tails[1]" class="input-field" type="number" value="10">

                            </div>
                            <div class="sub-settings-title" style="margin-top:10px">Скорость и оборудование</div>
                            <div class="input-group" style="margin-top:10px">
                                <span class="input-label">Клапан, мл/ч</span>
                                <input data-path="virtual_bw_tails" class="input-field" type="number">

                            </div>
                            <div class="input-group"><span class="input-label">Начальная скорость отбора, мл/ч</span>
                                <input data-path="tails.targetFlowMlh" class="input-field" type="number" placeholder="Максимум">
                            </div>
                        </div>
                        <div class="results-col">
                            <div class="analytics-item">
                                <span class="analytics-label">Объем</span>
                                <span class="analytics-value" data-result="analytics.fractions.tailsMl">0 мл</span>
                            </div>
                            <div class="analytics-item ">
                                <span class="analytics-label">Скорость</span>
                                <span class="analytics-value" data-result="analytics.flows.tails">0 мл/ч</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Флегмовое число</span>
                                <span class="analytics-value" data-result="analytics.phlegmatic.tails">0</span>
                            </div>
                            <div class="analytics-item">
                                <span class="analytics-label">Время этапа</span>
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
