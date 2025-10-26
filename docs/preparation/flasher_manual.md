# Автоматическая установка актуальной версии

!!! warning "ВНИМАНИЕ"
    При установке стабильной или тестовой версии через кнопку возможна полная очистка данных.
    Рекомендуется сохранить настройки telegramm бота, если они были установленны ранее.

!!! warning "ВНИМАНИЕ"
    Так как прошивальщик не может точно определить модель вашего чипа, то критически важно правильно выбрать тип.
    Убедитесь, что прошивка соответствует установленной памяти на вашем чипе. Подробнее как это определить - [читаем в начале](controller.md) 

!!! info inline end
    Если загрузка актуальных прошивок происходит долго - обновите страницу


<script
        type="module"
        src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
    ></script>

    <style>
        .install-button-wrapper { 
            margin: 20px auto; 
            max-width: 90%; /* Занимает 90% ширины родительского контейнера */
            width: 100%;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            text-align: center; 
            padding: 15px; 
            border-radius: 12px; 
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.15); 
            background-color: #f0f4f8; 
            transition: all 0.3s ease; 
            margin-bottom: 40px; 
            box-sizing: border-box;
        }
                
        /* Стили для кнопок GitHub и Локальной установки */
        .install-button-wrapper button[slot="activate"] { 
            background-color: #007bff; 
            color: white; 
            padding: 12px 10px; 
            border: none; 
            border-radius: 8px; 
            font-size: 16px; 
            font-weight: 600; 
            cursor: pointer; 
            box-shadow: 0 4px 8px rgba(0, 123, 255, 0.4); 
            transition: background-color 0.2s, transform 0.1s, box-shadow 0.2s; 
            text-transform: uppercase; 
            letter-spacing: 0.5px; 
            margin-top: 10px; 
            display: block; 
            width: 100%; 
            box-sizing: border-box; 
        }
        .install-button-wrapper button[slot="activate"]:hover { background-color: #0056b3; box-shadow: 0 6px 12px rgba(0, 123, 255, 0.6); }
        .install-button-wrapper button[slot="activate"]:active { transform: translateY(1px); box-shadow: 0 2px 5px rgba(0, 123, 255, 0.4); }

        /* Стили для раздела локальной прошивки */
        #local-install-wrapper {
            margin-top: 30px;
            padding-top: 15px;
            border-top: 1px solid #ddd;
        }
        #local-install-wrapper p {
            font-weight: 600;
            color: #34495e;
            margin-bottom: 15px;
        }
        
        /* Стили для контейнера выбора файла */
        #file-select-controls {
            display: flex;
            gap: 10px; /* Отступ между кнопкой и именем файла */
            align-items: center;
            margin-bottom: 15px;
        }

        /* Стили для LABEL, которая имитирует кнопку "Выбрать файл" */
        #file-selector-label {
            flex-shrink: 0; /* Не сжимать кнопку */
            background-color: #9b59b6; /* Фиолетовый цвет для выбора файла */
            color: white;
            padding: 10px 15px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 500;
            box-shadow: 0 2px 5px rgba(155, 89, 182, 0.4);
            transition: background-color 0.2s, box-shadow 0.2s;
        }
        #file-selector-label:hover {
            background-color: #8e44ad;
            box-shadow: 0 4px 8px rgba(155, 89, 182, 0.6);
        }

        /* Стиль для отображения имени файла */
        #file-name-display {
            flex-grow: 1; /* Занимает все доступное место */
            font-size: 14px;
            color: #34495e;
            text-align: left;
            padding: 8px 10px;
            border: 1px solid #bdc3c7;
            border-radius: 6px;
            background-color: white;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        
        /* Стили для кнопки локальной прошивки */
        #local-install-button button[slot="activate"] {
            background-color: #27ae60; /* Зеленый цвет */
            box-shadow: 0 4px 8px rgba(39, 174, 96, 0.4);
        }
        #local-install-button button[slot="activate"]:hover {
            background-color: #229954;
            box-shadow: 0 6px 12px rgba(39, 174, 96, 0.6);
        }

        /* Общие стили для сообщений */
        .install-button-wrapper span[slot="unsupported"], .install-button-wrapper span[slot="not-allowed"] { 
            display: block; padding: 10px; margin-top: 10px; border-radius: 6px; font-size: 14px; font-weight: 500; color: #c0392b; background-color: #fdedec; border: 1px solid #e74c3c; line-height: 1.4; 
        }
        .loader { border: 4px solid #f3f3f3; border-top: 4px solid #007bff; border-radius: 50%; width: 30px; height: 30px; animation: spin 1s linear infinite; margin: 20px auto; }
        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
    </style>

<div class="install-button-wrapper">
        <p id="info-message">Идет загрузка данных о релизах с GitHub...</p>
        <div class="loader" id="loader"></div>
        <div id="install-buttons-container"></div>
        
        <div id="local-install-wrapper">
            <p>Установка пользовательской прошивки (Смещение: 0x10000)</p>
            
            <div id="file-select-controls">
                <input type="file" id="file-input" accept=".bin" style="display: none;">
                
                <label for="file-input" id="file-selector-label">
                    Выбрать файл
                </label>
                
                <span id="file-name-display">Файл .bin не выбран</span>
            </div>

            <div id="local-install-button">
                </div>
            
            <p id="local-error-message" style="color: #c0392b; font-weight: 500; margin-top: 10px;"></p>
        </div>
    </div>

    <script>
        const REPO_OWNER = "SSVC0059";
        const REPO_NAME = "ssvc_open_connect";
        const CHIP_FAMILY = "ESP32-S3";
        const FIRMWARE_BASE_SUFFIX = "firmware_merged.bin";
        const LOCAL_FIRMWARE_OFFSET = 0x10000; // Смещение для локальной прошивки
        
        const CHIP_VARIANTS = [
            { name: "16MB Flash", pattern: "16m_" },
            { name: "8MB Flash", pattern: "8m_" }
        ];

        const CORS_PROXY_URL = "https://corsproxy.io/?"; 
        
        const MANIFEST_TEMPLATE = {
            "name": "SSVC Open Connect",
            "home_assistant_domain": "ssvc_open_connect",
            "new_install_prompt_erase": true,
            "builds": [
                {
                    "chipFamily": CHIP_FAMILY,
                    "parts": [
                        { "path": "", "offset": 0 }
                    ]
                }
            ]
        };

        // --- ФУНКЦИИ ДЛЯ ДИНАМИЧЕСКИХ КНОПОК GITHUB (без изменений) ---

        function createManifestBlobUrl(version, firmwareDownloadUrl) {
            const manifest = JSON.parse(JSON.stringify(MANIFEST_TEMPLATE));
            manifest.version = version;
            
            const proxiedUrl = CORS_PROXY_URL + encodeURIComponent(firmwareDownloadUrl);
            manifest.builds[0].parts[0].path = proxiedUrl; 

            const json = JSON.stringify(manifest);
            const blob = new Blob([json], { type: "application/json" });
            return URL.createObjectURL(blob);
        }

        function createInstallButton(buttonText, manifestUrl, container, isPrerelease = false) {
            const buttonWrapper = document.createElement('div');
            if (isPrerelease) { buttonWrapper.id = 'test-button'; }
            
            const installButton = document.createElement('esp-web-install-button');
            installButton.setAttribute('manifest', manifestUrl);

            const activateButton = document.createElement('button');
            activateButton.setAttribute('slot', 'activate');
            activateButton.textContent = buttonText;
            installButton.appendChild(activateButton);

            const unsupportedSpan = document.createElement('span');
            unsupportedSpan.setAttribute('slot', 'unsupported');
            unsupportedSpan.innerHTML = '❌ Ошибка: Ваш браузер не поддерживает Web Serial API. Попробуйте Chrome/Edge.';
            installButton.appendChild(unsupportedSpan);

            const notAllowedSpan = document.createElement('span');
            notAllowedSpan.setAttribute('slot', 'not-allowed');
            notAllowedSpan.innerHTML = '⚠️ Внимание: Установка требует HTTPS-соединения.'; 
            installButton.appendChild(notAllowedSpan);

            buttonWrapper.appendChild(installButton);
            container.appendChild(buttonWrapper);
        }
        
        // --- НОВАЯ ФУНКЦИЯ ДЛЯ ЛОКАЛЬНОЙ ПРОШИВКИ (Обновлена) ---

        function setupLocalInstall() {
            const fileInput = document.getElementById('file-input');
            const buttonContainer = document.getElementById('local-install-button');
            const fileNameDisplay = document.getElementById('file-name-display');
            const errorDisplay = document.getElementById('local-error-message');
            
            // Инициализация отображения
            fileNameDisplay.textContent = 'Файл .bin не выбран'; 

            // Создаем кнопку прошивки один раз, но пока скрываем ее
            const installButton = document.createElement('esp-web-install-button');
            installButton.id = 'local-flasher';

            const activateButton = document.createElement('button');
            activateButton.setAttribute('slot', 'activate');
            activateButton.textContent = 'Установить выбранный файл';
            installButton.appendChild(activateButton);
            
            // Добавляем стандартные слоты для сообщений
            installButton.innerHTML += `
                <span slot="unsupported">❌ Ошибка: Ваш браузер не поддерживает Web Serial API.</span>
                <span slot="not-allowed">⚠️ Внимание: Установка требует HTTPS-соединения.</span>
            `;
            
            buttonContainer.appendChild(installButton);
            installButton.style.display = 'none'; // По умолчанию скрыта

            fileInput.addEventListener('change', (event) => {
                errorDisplay.textContent = ''; // Сброс ошибок
                const file = event.target.files[0];
                
                if (file) {
                    // Проверяем расширение файла
                    if (!file.name.endsWith('.bin')) {
                        errorDisplay.textContent = '❌ Выберите файл прошивки с расширением .bin.';
                        installButton.style.display = 'none';
                        fileNameDisplay.textContent = 'Файл .bin не выбран';
                        return;
                    }
                    
                    // Обновляем отображение
                    fileNameDisplay.textContent = `Выбран: ${file.name} (Размер: ${(file.size / 1024 / 1024).toFixed(2)} МБ)`;
                    
                    // 1. Создаем Blob URL для локального файла
                    const firmwareBlobUrl = URL.createObjectURL(file);
                    
                    // 2. Создаем манифест для локального файла
                    const manifest = JSON.parse(JSON.stringify(MANIFEST_TEMPLATE));
                    manifest.version = 'local';
                    
                    // Устанавливаем параметры прошивки для локального файла
                    manifest.builds[0].parts[0].path = firmwareBlobUrl; 
                    manifest.builds[0].parts[0].offset = LOCAL_FIRMWARE_OFFSET; 

                    const manifestJson = JSON.stringify(manifest);
                    const manifestBlob = new Blob([manifestJson], { type: "application/json" });
                    const manifestBlobUrl = URL.createObjectURL(manifestBlob);

                    // 3. Обновляем кнопку и показываем ее
                    installButton.setAttribute('manifest', manifestBlobUrl);
                    installButton.style.display = 'block';

                } else {
                    fileNameDisplay.textContent = 'Файл .bin не выбран';
                    installButton.style.display = 'none';
                }
            });
        }

        // --- ГЛАВНЫЙ ИСПОЛНЯЕМЫЙ БЛОК (без изменений) ---

        async function loadAndCreateButtons() {
            const container = document.getElementById('install-buttons-container');
            const loader = document.getElementById('loader');
            const infoParagraph = document.getElementById('info-message');

            loader.style.display = 'block';
            infoParagraph.textContent = 'Идет загрузка данных о релизах с GitHub...';

            try {
                const url = `https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases`;
                const response = await fetch(url, { headers: { 'Accept': 'application/vnd.github.v3+json' } });

                if (!response.ok) {
                    throw new Error(`GitHub API Error: ${response.status} ${response.statusText}`);
                }

                const releases = await response.json();
                let releaseVariants = {}; 

                for (const release of releases) {
                    for (const variant of CHIP_VARIANTS) {
                        const patternKey = variant.pattern;
                        
                        if (releaseVariants[patternKey] && releaseVariants[patternKey].isStable && !release.prerelease) {
                            continue;
                        }

                        const firmwareAsset = release.assets.find(a => 
                            a.name.endsWith(FIRMWARE_BASE_SUFFIX) && a.name.includes(patternKey)
                        );

                        if (firmwareAsset) {
                            const releaseData = {
                                version: release.tag_name,
                                firmwareDownloadUrl: firmwareAsset.browser_download_url,
                                variantName: variant.name,
                                isStable: !release.prerelease
                            };

                            if (releaseData.isStable || !releaseVariants[patternKey]) {
                                releaseVariants[patternKey] = releaseData;
                            }
                        }
                    }
                }

                container.innerHTML = '';
                let foundAny = false;
                
                const buttonsData = Object.values(releaseVariants).sort((a, b) => {
                    if (a.isStable && !b.isStable) return -1;
                    if (!a.isStable && b.isStable) return 1;
                    return 0;
                });

                for (const data of buttonsData) {
                    foundAny = true;
                    const prefix = data.isStable ? '🚀' : '🧪';
                    const releaseType = data.isStable ? 'Стабильная' : 'Тестовая';
                    const buttonText = `${prefix} Установить (${data.variantName}, ${releaseType}: ${data.version})`;
                    
                    const manifestUrl = createManifestBlobUrl(data.version, data.firmwareDownloadUrl);
                    createInstallButton(buttonText, manifestUrl, container, !data.isStable);
                }

                if (!foundAny) {
                    infoParagraph.textContent = '❌ Не удалось найти файл прошивки для 8MB или 16MB чипов в последних релизах.';
                } else {
                    infoParagraph.textContent = 'Выберите версию прошивки для установки:';
                }

            } catch (error) {
                console.error("Ошибка при работе с GitHub API или создании кнопок:", error);
                infoParagraph.textContent = `❌ Критическая ошибка: ${error.message}.`;
                container.innerHTML = '';
            } finally {
                loader.style.display = 'none';
                
                // *** ИНИЦИАЛИЗАЦИЯ ЛОКАЛЬНОЙ УСТАНОВКИ ***
                setupLocalInstall();
            }
        }

        document.addEventListener('DOMContentLoaded', loadAndCreateButtons);
    </script>