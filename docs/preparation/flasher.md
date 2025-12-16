# Автоматическая установка актуальной версии



!!! info inline end
    Если загрузка актуальных прошивок происходит долго - обновите страницу


## Использование установщика

Установщик позволяет в автоматическом режиме установить актуальную или тестовую прошивку с правильным смещением. Для его использования на компьютере должны быть установленны драйвера.

## Прошивка контроллера

1. Подключите контроллер проводом к компьютеру проводом и убедитесь что драйвера установленны

2. Нажмите кнопку установки, которая соответствует вашему чипу

!!! warning "ВНИМАНИЕ"
    Так как прошивальщик не может точно определить модель вашего чипа, то критически важно правильно выбрать тип.
    Убедитесь, что прошивка соответствует установленной памяти на вашем чипе. Подробнее как это определить - [читаем в начале](controller.md) 

После нажатия появится окно выбора контроллера.

![](media/flasher_1.png)


После выбора появится меню, в нем выбираем установку и дале следуем пунктам мастера. При первоначальной установке или при не корректной работе желательно произвести очистку.

!!! warning "ВНИМАНИЕ"
    При установке стабильной или тестовой версии через кнопку возможна полная очистка данных.
    Рекомендуется сохранить настройки telegramm бота, если они были установленны ранее.


<div style="display: flex; justify-content: center; gap: 10px; flex-wrap: wrap;">
<img src="/ssvc_open_connect/preparation/media/flasher_2.png" style="height:250px"> 
<img src="/ssvc_open_connect/preparation/media/flasher_3.png" style="height:250px"> 
<img src="/ssvc_open_connect/preparation/media/flasher_4.png" style="height:250px"> 
</div>


## Flash Tool

<style>
    /* Убеждаемся, что отступы и границы не добавляются к общей ширине */
    *, *:before, *:after {
        box-sizing: border-box;
    }

    /* Общий контейнер */
    #install-buttons-container-styled {
        display: flex;
        flex-direction: column;
        gap: 20px; /* Отступ между группами */
        padding: 20px;
        border-radius: 12px;
        background-color: #f7f7f7; /* Слегка серый фон */
        box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
        /* УДАЛЕНО: max-width, чтобы контейнер занимал всю доступную ширину родителя */
        margin: 0 auto; /* Центрируем контейнер, если родитель шире */
    }

    /* Стили для групп кнопок */
    .button-group {
        border: 1px solid #e0e0e0;
        padding: 15px;
        border-radius: 10px;
        display: flex;
        flex-direction: column;
        gap: 10px;
        background-color: #ffffff;
        box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
    }

    .group-title {
        font-size: 1.3em;
        font-weight: bold;
        margin-bottom: 5px;
        padding-bottom: 5px;
        border-bottom: 2px solid;
    }

    /* Стилизация внутренних кнопок */
    .button-group esp-web-install-button button {
        width: 100%;
        padding: 14px 20px;
        border: none;
        border-radius: 8px;
        font-size: 1.05em;
        font-weight: 600;
        color: white;
        cursor: pointer;
        transition: all 0.3s ease;
        text-shadow: 0 1px 1px rgba(0, 0, 0, 0.1);
        box-shadow: 0 3px 6px rgba(0, 0, 0, 0.15);
    }

    /* Тестовая версия: Синий (умеренный) */
    .group-test .group-title {
        color: #17a2b8;
        border-bottom-color: #17a2b8;
    }
    .group-test esp-web-install-button button {
        background: linear-gradient(135deg, #17a2b8 0%, #1a96a8 100%);
    }

    /* Стабильная версия: Зеленый (умеренный) */
    .group-stable .group-title {
        color: #28a745;
        border-bottom-color: #28a745;
    }
    .group-stable esp-web-install-button button {
        background: linear-gradient(135deg, #28a745 0%, #218838 100%);
    }

    /* Эффект при наведении (Hover effect) */
    .button-group esp-web-install-button button:hover {
        opacity: 0.95;
        transform: translateY(-1px);
        box-shadow: 0 5px 10px rgba(0, 0, 0, 0.2);
    }

    /* Стили для сообщений о несовместимости/предупреждений */
    span[slot="unsupported"],
    span[slot="not-allowed"] {
        display: block;
        padding: 8px;
        margin-top: 5px;
        border-radius: 6px;
        font-weight: normal;
        font-size: 0.9em;
        text-align: center;
        width: 100%;
        box-sizing: border-box;
    }
    span[slot="unsupported"] {
        color: #dc3545;
        background-color: #f8d7da;
    }
    span[slot="not-allowed"] {
        color: #856404;
        background-color: #fff3cd;
    }
</style>

<script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>

<div id="install-buttons-container-styled">
    <div class="button-group group-test">
        <div class="group-title">🚀 Стабильная Версия (0.2.4.1)</div>
        <esp-web-install-button manifest="/ssvc_open_connect/firmware/manifest_16m_0-2-4-1.json">
            <button slot="activate">16MB Flash</button>
            <span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>
            <span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>
        </esp-web-install-button>

        <esp-web-install-button manifest="/ssvc_open_connect/firmware/manifest_8m_0-2-4-1.json">
            <button slot="activate">8MB Flash</button>
            <span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>
            <span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>
        </esp-web-install-button>
    </div>
    <div class="button-group group-stable">
        <div class="group-title">🚀 Стабильная Версия (0.2.4.0)</div>
        <esp-web-install-button manifest="/ssvc_open_connect/firmware/manifest_16m_0-2-4-0.json">
            <button slot="activate">16MB Flash</button>
            <span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>
            <span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>
        </esp-web-install-button>

        <esp-web-install-button manifest="/ssvc_open_connect/firmware/manifest_8m_0-2-4-0.json">
            <button slot="activate">8MB Flash</button>
            <span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>
            <span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>
        </esp-web-install-button>
    </div>

    <div class="button-group group-stable">
        <div class="group-title">🚀 Стабильная Версия (0.2.1.8)</div>
        <esp-web-install-button manifest="/ssvc_open_connect/firmware/manifest_16m_0-2-1-8.json">
            <button slot="activate">16MB Flash</button>
            <span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>
            <span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>
        </esp-web-install-button>

        <esp-web-install-button manifest="/ssvc_open_connect/firmware/manifest_8m_0-2-1-8.json">
            <button slot="activate">8MB Flash</button>
            <span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>
            <span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>
        </esp-web-install-button>
    </div>
</div>