# Автоматическая установка актуальной версии



!!! info inline end
    Если загрузка актуальных прошивок происходит долго - обновите страницу


## Использование установщика

Установщик позволяет в автоматическом режиме установить актуальную или тестовую прошивку с правильным смещением. Для его использования на компьютере должны быть установленны драйвера.

## Прошивка контроллера

1. Подключите контроллер проводом к компьютеру проводом и убедитесь что драйвера установленны

2. Нажмите [кнопку установки](#flash-tool), которая соответствует вашему чипу

!!! warning "ВНИМАНИЕ"
    Так как прошивальщик не может точно определить модель вашего чипа, то критически важно правильно выбрать тип.
    Убедитесь, что прошивка соответствует установленной памяти на вашем чипе. Подробнее как это определить - [читаем в начале](controller.md) 

После нажатия появится окно выбора контроллера.

![](media/flasher_1.png)


После выбора появится меню, в нем выбираем установку и дале следуем пунктам мастера. При первоначальной установке или при не корректной работе желательно произвести очистку.

!!! warning "ВНИМАНИЕ"
    При установке стабильной или тестовой версии через кнопку возможна полная очистка данных.
    Рекомендуется сохранить настройки telegramm бота и mqtt, если они были установленны ранее.


<div style="display: flex; justify-content: center; gap: 10px; flex-wrap: wrap;">
<img src="/ssvc_open_connect/preparation/media/flasher_2.png" style="height:250px"> 
<img src="/ssvc_open_connect/preparation/media/flasher_3.png" style="height:250px"> 
<img src="/ssvc_open_connect/preparation/media/flasher_4.png" style="height:250px"> 
</div>


## Flash Tool

<style>
    *, *:before, *:after { box-sizing: border-box; }

    #install-buttons-container-styled {
        display: flex;
        flex-direction: column;
        gap: 10px;
        padding: 12px;
        border-radius: 10px;
        background-color: #f7f7f7;
        box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
        margin: 0 auto;
    }

    .collapsible-group {
        border: 1px solid #e0e0e0;
        border-radius: 8px;
        background-color: #ffffff;
        box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
        overflow: hidden;
    }
    .collapsible-group .group-header {
        display: flex;
        align-items: center;
        padding: 8px 10px;
        cursor: pointer;
        user-select: none;
        gap: 6px;
    }
    .collapsible-group .group-header:hover {
        background-color: #f8f8f8;
    }
    .collapsible-group .group-header .chevron {
        font-size: 0.75em;
        transition: transform 0.2s ease;
    }
    .collapsible-group.collapsed .group-header .chevron {
        transform: rotate(-90deg);
    }
    .collapsible-group .group-content {
        display: flex;
        flex-wrap: wrap;
        align-items: center;
        gap: 6px;
        padding: 0 10px 10px 10px;
        max-height: 300px;
        overflow: hidden;
        transition: max-height 0.25s ease;
    }
    .collapsible-group.collapsed .group-content {
        max-height: 0;
        padding-top: 0;
        padding-bottom: 0;
    }
    .group-content-column {
        flex-direction: column;
        align-items: stretch;
    }

    .collapsible-group .group-title {
        font-size: 1em;
        font-weight: bold;
        margin: 0;
        padding: 0;
        border: none;
        flex: 1;
    }

    .collapsible-group .group-content esp-web-install-button {
        flex: 1 1 140px;
        min-width: 0;
    }
    .collapsible-group esp-web-install-button button {
        width: 100%;
        padding: 8px 12px;
        border: none;
        border-radius: 6px;
        font-size: 0.9em;
        font-weight: 600;
        color: white;
        cursor: pointer;
        transition: all 0.2s ease;
        text-shadow: 0 1px 1px rgba(0, 0, 0, 0.1);
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.12);
    }

    .collapsible-group.group-stable .group-title {
        color: #28a745;
    }
    .collapsible-group.group-stable esp-web-install-button button {
        background: linear-gradient(135deg, #28a745 0%, #218838 100%);
    }
    .collapsible-group.group-test .group-title {
        color: #17a2b8;
    }
    .collapsible-group.group-test esp-web-install-button button {
        background: linear-gradient(135deg, #17a2b8 0%, #1a96a8 100%);
    }

    .collapsible-group esp-web-install-button button:hover {
        opacity: 0.95;
        transform: translateY(-1px);
        box-shadow: 0 3px 6px rgba(0, 0, 0, 0.18);
    }

    span[slot="unsupported"], span[slot="not-allowed"] {
        display: block;
        padding: 6px;
        margin-top: 4px;
        border-radius: 4px;
        font-weight: normal;
        font-size: 0.85em;
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

    .loader { border: 3px solid #f3f3f3; border-top: 3px solid #007bff; border-radius: 50%; width: 24px; height: 24px; animation: spin 1s linear infinite; margin: 12px auto; }
    @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }

    #local-install-wrapper { margin-top: 12px; padding-top: 10px; border-top: 1px solid #ddd; }
    .collapsible-group.group-local .group-title { color: #6c5ce7; }
    .collapsible-group.group-local esp-web-install-button button { background: linear-gradient(135deg, #6c5ce7 0%, #5b4cdb 100%); }
    #file-select-controls { display: flex; gap: 8px; align-items: center; margin-bottom: 6px; flex-wrap: wrap; }
    #file-selector-label {
        flex-shrink: 0;
        background: linear-gradient(135deg, #6c5ce7 0%, #5b4cdb 100%);
        color: white;
        padding: 8px 14px;
        border-radius: 6px;
        cursor: pointer;
        font-size: 0.9em;
        font-weight: 600;
        box-shadow: 0 2px 4px rgba(108, 92, 231, 0.3);
    }
    #file-selector-label:hover { opacity: 0.95; transform: translateY(-1px); }
    #file-name-display { flex-grow: 1; min-width: 100px; font-size: 13px; color: #34495e; padding: 8px 10px; border: 1px solid #e0e0e0; border-radius: 6px; background-color: #fafafa; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
</style>

<script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>

<div id="install-buttons-container-styled">
    <p id="info-message">Идет загрузка...</p>
    <div class="loader" id="loader"></div>
    <div id="release-buttons"></div>

    <div id="local-install-wrapper">
        <div class="collapsible-group group-local collapsed" id="local-install-group">
            <div class="group-header" onclick="document.getElementById('local-install-group').classList.toggle('collapsed')">
                <span class="chevron">▼</span>
                <span class="group-title">📁 Установка из файла</span>
            </div>
            <div class="group-content group-content-column">
                <p style="margin: 0 0 6px 0; font-size: 0.85em; color: #666;">Выберите .bin, затем «Установить»</p>
                <div id="file-select-controls">
                    <input type="file" id="file-input" accept=".bin" style="display: none;">
                    <label for="file-input" id="file-selector-label">Выбрать файл</label>
                    <span id="file-name-display">Файл не выбран</span>
                </div>
                <div id="local-install-button"></div>
                <p id="local-error-message" style="color: #c0392b; font-weight: 500; margin: 8px 0 0 0; font-size: 0.9em;"></p>
            </div>
        </div>
    </div>
</div>

<script>
(function() {
    const LOCAL_FIRMWARE_OFFSET = 0x0;
    const MANIFEST_TEMPLATE = {
        name: "SSVC Open Connect",
        home_assistant_domain: "ssvc_open_connect",
        new_install_prompt_erase: true,
        builds: [{ chipFamily: "ESP32-S3", parts: [{ path: "", offset: 0 }] }]
    };

    function createButtonGroup(title, variants, isStable, groupType, expanded) {
        var group = document.createElement('div');
        group.className = 'collapsible-group ' + (groupType || (isStable ? 'group-stable' : 'group-test'));
        var prefix = groupType === 'group-local' ? '🔧' : (isStable ? '🚀' : '🧪');
        var header = document.createElement('div');
        header.className = 'group-header';
        header.innerHTML = '<span class="chevron">▼</span><span class="group-title">' + prefix + ' ' + title + '</span>';
        header.onclick = function() { group.classList.toggle('collapsed'); };
        var content = document.createElement('div');
        content.className = 'group-content';
        variants.forEach(function(v) {
            var btn = document.createElement('esp-web-install-button');
            btn.setAttribute('manifest', v.manifestUrl);
            btn.innerHTML = '<button slot="activate">' + v.label + '</button>' +
                '<span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>' +
                '<span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>';
            content.appendChild(btn);
        });
        group.appendChild(header);
        group.appendChild(content);
        if (!expanded) group.classList.add('collapsed');
        return group;
    }

    function resolveUrl(url) {
        if (url.startsWith('/')) return window.location.origin + url;
        if (!url.startsWith('http')) return new URL(url, window.location.href).href;
        return url;
    }

    async function loadManifestIndex() {
        var paths = ['/ssvc_open_connect/firmware/manifest_index.json', '/firmware/manifest_index.json'];
        for (var i = 0; i < paths.length; i++) {
            var r = await fetch(window.location.origin + paths[i]);
            if (r.ok) {
                var data = await r.json();
                var result = { stable: [], test: [], local: [] };
                if (data.stable) {
                    result.stable = data.stable.map(function(v) {
                        return { manifestUrl: resolveUrl(v.manifestUrl), label: v.label, version: v.version };
                    });
                }
                if (data.test) {
                    result.test = data.test.map(function(v) {
                        return { manifestUrl: resolveUrl(v.manifestUrl), label: v.label, version: v.version };
                    });
                }
                if (data.local) {
                    result.local = data.local.map(function(v) {
                        return { manifestUrl: resolveUrl(v.manifestUrl), label: v.label, version: v.version };
                    });
                }
                return result;
            }
        }
        throw new Error('manifest_index.json не найден');
    }

    function setupLocalInstall() {
        var fileInput = document.getElementById('file-input');
        var buttonContainer = document.getElementById('local-install-button');
        var fileNameDisplay = document.getElementById('file-name-display');
        var errorDisplay = document.getElementById('local-error-message');
        var installButton = buttonContainer.querySelector('esp-web-install-button');
        if (!installButton) {
            installButton = document.createElement('esp-web-install-button');
            installButton.innerHTML = '<button slot="activate">Установить</button>' +
                '<span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>' +
                '<span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>';
            buttonContainer.appendChild(installButton);
            installButton.style.display = 'none';
        }
        fileInput.onchange = function() {
            errorDisplay.textContent = '';
            var file = fileInput.files[0];
            if (!file) {
                fileNameDisplay.textContent = 'Файл не выбран';
                installButton.style.display = 'none';
                return;
            }
            if (!file.name.endsWith('.bin')) {
                errorDisplay.textContent = '❌ Выберите файл с расширением .bin';
                installButton.style.display = 'none';
                return;
            }
            fileNameDisplay.textContent = file.name + ' (' + (file.size / 1024 / 1024).toFixed(2) + ' МБ)';
            var manifest = JSON.parse(JSON.stringify(MANIFEST_TEMPLATE));
            manifest.version = 'local';
            manifest.builds[0].parts[0].path = URL.createObjectURL(file);
            manifest.builds[0].parts[0].offset = LOCAL_FIRMWARE_OFFSET;
            installButton.setAttribute('manifest', URL.createObjectURL(new Blob([JSON.stringify(manifest)], { type: 'application/json' })));
            installButton.style.display = 'block';
        };
    }

    async function init() {
        var loader = document.getElementById('loader');
        var info = document.getElementById('info-message');
        var container = document.getElementById('release-buttons');
        loader.style.display = 'block';
        try {
            var data = await loadManifestIndex();
            var hasStable = data.stable && data.stable.length > 0;
            var hasTest = data.test && data.test.length > 0;
            var hasLocal = data.local && data.local.length > 0;
            if (!hasStable && !hasTest && !hasLocal) throw new Error('Нет доступных прошивок');
            if (hasStable) {
                var v = data.stable[0].version;
                container.appendChild(createButtonGroup('Стабильная версия (' + v + ')', data.stable, true, null, true));
            }
            if (hasTest) {
                v = data.test[0].version;
                container.appendChild(createButtonGroup('Тестовая версия (' + v + ')', data.test, false, null, false));
            }
            if (hasLocal) {
                v = data.local[0].version;
                container.appendChild(createButtonGroup('Локальная сборка (' + v + ') — не опубликована', data.local, false, 'group-local', false));
            }
            info.textContent = 'Выберите версию прошивки для установки:';
        } catch (e) {
            info.innerHTML = 'Прошивки пока не загружены. Скачайте .bin с <a href="https://github.com/SSVC0059/ssvc_open_connect/releases" target="_blank" rel="noopener">Releases</a> и используйте «Установка из файла» ниже.';
            container.innerHTML = '';
        } finally {
            loader.style.display = 'none';
            setupLocalInstall();
        }
    }

    document.addEventListener('DOMContentLoaded', init);
})();
</script>
