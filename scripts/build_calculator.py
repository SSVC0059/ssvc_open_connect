import re
import os

def convert_ts_to_js(content):
    """
    Упрощенный конвертер из TypeScript в JavaScript.
    Удаляет экспорт, импорт и аннотации типов.
    """
    # Удаление export и import
    content = re.sub(r'^(export|import)\s+.*?;?\s*$', '', content, flags=re.MULTILINE)

    # Удаление interface
    content = re.sub(r'^interface\s+\w+\s*\{[^}]*\}\s*', '', content, flags=re.MULTILINE | re.DOTALL)
    
    # Удаление аннотаций типов из аргументов функций и переменных
    # Пример: (arg: number) -> (arg)
    # (arg: number, arg2: string) -> (arg, arg2)
    content = re.sub(r':\s*([\w\[\]<>|&\'"]+)', '', content)
    
    # Удаление модификаторов доступа (public, private, readonly)
    content = re.sub(r'\b(public|private|readonly)\s+', '', content)

    return content.strip()

def main():
    """
    Основная функция для сборки калькулятора.
    """
    # Определение путей относительно расположения скрипта
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    # Исходные файлы
    deep_clone_path = os.path.join(project_root, 'interface', 'src', 'lib', 'utils', 'deepClone.ts')
    model_path = os.path.join(project_root, 'interface', 'src', 'lib', 'actions', 'DistillationCycleModel.ts')
    
    # Конечный файл
    output_path = os.path.join(project_root, 'docs', 'rectification_calculator.js')

    # Чтение и преобразование исходных файлов
    try:
        with open(deep_clone_path, 'r', encoding='utf-8') as f:
            deep_clone_js = convert_ts_to_js(f.read())

        with open(model_path, 'r', encoding='utf-8') as f:
            model_js = convert_ts_to_js(f.read())
    except FileNotFoundError as e:
        print(f"Ошибка: Не удалось найти исходный файл. {e}")
        return

    # Код для управления DOM (UI-логика)
    ui_logic = """
document.addEventListener('DOMContentLoaded', () => {
    // Эта функция formatTime дублируется в модели, но для надежности оставим ее здесь,
    // если вдруг она будет удалена из модели.
    function formatTime(seconds) {
        if (isNaN(seconds) || seconds < 0) return '00:00:00';
        const h = Math.floor(seconds / 3600).toString().padStart(2, '0');
        const m = Math.floor((seconds % 3600) / 60).toString().padStart(2, '0');
        const s = Math.floor(seconds % 60).toString().padStart(2, '0');
        return `${h}:${m}:${s}`;
    }

    const model = new DistillationCycleModel();
    const initialParams = {
        powerKw: 2.5, volumeL: 18, strengthVol: 40, stabilizationMin: 20,
        heads: { percent: 3, valveMlh: 7000, pwmPeriod: 120, enabled: true },
        subHeads: { percent: 7, valveMlh: 7000, pwmPeriod: 60, enabled: true },
        body: { valveMlh: 12000, pwmPeriod: 10, initialSpeedMlh: 1800, decrementPercent: 10 },
        tails: { percent: 5, valveMlh: 15000, pwmPeriod: 360, enabled: true }
    };
    let params = deepClone(initialParams);
    let customSsvcSettings = {};

    const inputElements = Array.from(document.querySelectorAll('input[data-path]'));
    const ssvcInputElements = Array.from(document.querySelectorAll('.ssvc-grid input[data-path]'));
    const ssvcResetButtons = Array.from(document.querySelectorAll('.ssvc-grid button[data-path]'));

    function getAnalyticsElement(id) {
        const el = document.getElementById(`an-${id}`);
        if (!el) return { value: null, initial: null, container: null };
        return {
            value: el.querySelector('.analytics-value'),
            initial: el.querySelector('.initial-value'),
            container: el
        };
    }
    
    function getSsvcElement(path) {
        return document.querySelector(`.ssvc-grid [data-path="${path}"]`);
    }

    function updateUI() {
        const calculatedResult = model.calculateProcess(params);
        const calculatedSsvcSettings = calculatedResult.ssvcSettings;

        if (Object.keys(customSsvcSettings).length === 0) {
            customSsvcSettings = deepClone(calculatedSsvcSettings);
        }

        const finalResult = model.calculateProcess({ ...params, ssvcSettings: customSsvcSettings });

        updateAnalytics(finalResult.analytics, calculatedResult.analytics);
        updateSsvcInputs(customSsvcSettings, calculatedSsvcSettings);
        
        ['heads', 'subHeads', 'tails'].forEach(key => {
            const details = document.getElementById(`details-${key}`);
            const summary = document.getElementById(`summary-${key}`);
            
            summary.classList.toggle('active', params[key].enabled);
            
            // Управляем только состоянием open, анимацию и отображение делает CSS
            if (params[key].enabled) {
                details.open = true;
            } else {
                details.open = false;
            }
        });
    }

    function updateAnalytics(analytics, calculatedAnalytics) {
        getAnalyticsElement('totalAS').value.textContent = `${analytics.totalAS.toFixed(0)} мл`;
        getAnalyticsElement('boilingTemp').value.textContent = `${analytics.boilingTemp.toFixed(2)} °C`;
        getAnalyticsElement('oneCycleTime').value.textContent = analytics.oneCycleTime;
        getAnalyticsElement('total_process_timer').value.textContent = formatTime(analytics.timers.total_process_timer);
        getAnalyticsElement('residueMl').value.textContent = `${(analytics.residueMl / 1000).toFixed(2)} л`;

        ['headsMl', 'subHeadsMl', 'bodyMl', 'tailsMl'].forEach(key => {
            getAnalyticsElement(`fractions-${key}`).value.textContent = `${analytics.fractions[key].toFixed(0)} мл`;
        });

        ['heads', 'subHeads', 'body', 'tails'].forEach(key => {
            const el = getAnalyticsElement(`flows-${key}`);
            el.value.textContent = `${analytics.flows[key]} мл/ч`;
            const initialVal = `${calculatedAnalytics.flows[key]} мл/ч`;
            const isModified = el.value.textContent !== initialVal;
            el.container.classList.toggle('modified', isModified);
            if (el.initial) {
                el.initial.textContent = isModified ? `(${initialVal})` : '';
            }
        });

        ['heads_timer', 'subHeads_timer', 'body_timer', 'tails_timer'].forEach(key => {
            getAnalyticsElement(`timers-${key}`).value.textContent = formatTime(analytics.timers[key]);
        });
    }

    function updateSsvcInputs(current, initial) {
        ['heads', 'subHeads', 'body', 'tails'].forEach(key => {
            const periodInput = getSsvcElement(`${key}.period`);
            const openTimeInput = getSsvcElement(`${key}.openTime`);
            const periodInitialEl = getSsvcElement(`${key}.period-initial`);
            const openTimeInitialEl = getSsvcElement(`${key}.openTime-initial`);
            const resetBtn = getSsvcElement(`${key}-reset`);
            const itemContainer = document.getElementById(`ssvc-${key}`);

            periodInput.value = current[key].period;
            openTimeInput.value = current[key].openTime;

            const isPeriodModified = String(current[key].period) !== String(initial[key].period);
            const isOpenTimeModified = Number(current[key].openTime).toFixed(1) !== Number(initial[key].openTime).toFixed(1);

            periodInput.classList.toggle('modified', isPeriodModified);
            periodInitialEl.textContent = isPeriodModified ? `было: ${initial[key].period}` : '';

            openTimeInput.classList.toggle('modified', isOpenTimeModified);
            openTimeInitialEl.textContent = isOpenTimeModified ? `было: ${Number(initial[key].openTime).toFixed(1)}` : '';
            
            const isModified = isPeriodModified || isOpenTimeModified;
            itemContainer.classList.toggle('modified', isModified);
            resetBtn.classList.toggle('visible', isModified);
        });
    }
    
    function readParamsFromUI() {
        const newParams = deepClone(params);
        inputElements.forEach(el => {
            const path = el.dataset.path.split('.');
            let current = newParams;
            for (let i = 0; i < path.length - 1; i++) {
                current = current[path[i]];
            }
            if (el.type === 'checkbox') {
                current[path[path.length - 1]] = el.checked;
            } else {
                current[path[path.length - 1]] = parseFloat(el.value) || 0;
            }
        });
        return newParams;
    }
    
    function readSsvcFromUI() {
        const newSsvc = deepClone(customSsvcSettings);
         ['heads', 'subHeads', 'body', 'tails'].forEach(key => {
            newSsvc[key].period = parseFloat(getSsvcElement(`${key}.period`).value) || 0;
            newSsvc[key].openTime = parseFloat(getSsvcElement(`${key}.openTime`).value) || 0;
        });
        return newSsvc;
    }

    function setParamsToUI(p) {
        inputElements.forEach(el => {
            const path = el.dataset.path.split('.');
            let val = p;
            path.forEach(k => val = val[k]);
            if (el.type === 'checkbox') {
                el.checked = val;
            } else {
                el.value = val;
            }
        });
    }

    inputElements.forEach(el => {
        el.addEventListener('input', () => {
            params = readParamsFromUI();
            customSsvcSettings = {}; // Сбрасываем кастомные настройки
            updateUI();
        });
    });
    
    ssvcInputElements.forEach(el => {
        el.addEventListener('input', () => {
            customSsvcSettings = readSsvcFromUI();
            updateUI();
        });
    });

    ssvcResetButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const key = btn.dataset.path.replace('-reset', '');
            const calculatedSsvc = model.calculateProcess(params).ssvcSettings;
            customSsvcSettings[key] = deepClone(calculatedSsvc[key]);
            updateUI();
        });
    });

    document.getElementById('reset-all-btn').addEventListener('click', () => {
        params = deepClone(initialParams);
        customSsvcSettings = {};
        setParamsToUI(params);
        updateUI();
    });

    // Инициализация
    setParamsToUI(params);
    updateUI();
});
"""

    # Объединение всего в один файл
    final_js = f"{deep_clone_js}\\n\\n{model_js}\\n\\n// --- UI Logic ---\\n{ui_logic}"

    # Запись результата
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(final_js)
        
    print(f"Калькулятор успешно собран в файл: {output_path}")

if __name__ == "__main__":
    main()
