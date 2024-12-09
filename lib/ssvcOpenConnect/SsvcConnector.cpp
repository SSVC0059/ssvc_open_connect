//
// Created by demoncat on 26.11.2024.
//

#include "SsvcConnector.h"

#include <utility>

// Инициализация статической переменной
SsvcConnector* SsvcConnector::_ssvcConnector = nullptr;

SsvcConnector::SsvcConnector(EventGroupHandle_t eventGroup) : _eventGroup(eventGroup),
                                                              _isCommandInProgress(false){
    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr) {
        Serial.println("Ошибка: не удалось создать мьютекс!");
    }
    uartCommunicationError = false;
    isSupportSSVCVersion = true;
}

SsvcConnector::~SsvcConnector() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

SsvcConnector* SsvcConnector::getConnector(EventGroupHandle_t eventGroup) {
    if (!_ssvcConnector) {
        _ssvcConnector = new SsvcConnector(eventGroup);
    }
    return _ssvcConnector;  // Возвращаем указатель на экземпляр
}

void SsvcConnector::begin() {
    Serial.println("Start SsvcConnector begin()");
//    gpio_set_pull_mode(SSVC_OPEN_CONNECT_UART_RX, GPIO_PULLUP_ONLY);

    constexpr uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(SSVC_OPEN_CONNECT_UART_NUM,
                        1024,
                        SSVC_OPEN_CONNECT_BUF_SIZE * 2,
                        SSVC_OPEN_CONNECT_BUF_SIZE * 2,
                        nullptr,
                        0);
    uart_param_config(SSVC_OPEN_CONNECT_UART_NUM, &uart_config);
    uart_set_pin(SSVC_OPEN_CONNECT_UART_NUM,
                 SSVC_OPEN_CONNECT_UART_TX,
                 SSVC_OPEN_CONNECT_UART_RX,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    ESP_LOGV("SSVC Open Connect", "Starting loop task");

    SsvcConnector::initSsvcController();
};

// CRON
void SsvcConnector::initSsvcController() {
    Serial.println("start _telemetry task");
    // Запуск сбора телеметрии
    xTaskCreatePinnedToCore(
            SsvcConnector::_telemetry,            // Function that should be called
            "TelemetryTask",     // Name of the task (for debugging)
            4096,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (configMAX_PRIORITIES - 10),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );

    // Запрос настроек SSVC
    #ifdef SSVC_DEBUG
    Serial.println("start init task: GET_SETTINGS")
    #endif

    // Получение настроек SSVC
    taskGetSettingsCommand();
    taskVersionCommand();

}

void SsvcConnector::_startTask(void *pvParameters) {
    Serial.println("SsvcConnector::_startTask");
    auto* taskParams = static_cast<TaskParameters*>(pvParameters);
    auto* self = taskParams->connector;
    auto commandFunction = taskParams->commandFunction;
    bool _need_deleteTask = taskParams->need_deleteTask;

    while (true) {

        vTaskDelay(pdMS_TO_TICKS(4000));

        if (self -> isCommandInProgress()) {
            Serial.println("Команда уже выполняется. Ожидание завершения...");
            vTaskDelay(pdMS_TO_TICKS(5000));
        } else {
            self->setCommandInProgress(true);
        }

        if (commandFunction) {
            commandFunction(self); // Выполнение переданной команды
        } else {
            Serial.println("SsvcConnector::_startTask: commandFunction is nullptr!");
        }

        vTaskDelay(pdMS_TO_TICKS(100));

        // Ожидание события
        EventBits_t bits = xEventGroupWaitBits(
                self->_eventGroup,
                BIT0,        // Ждём BIT0
                pdTRUE,      // Сбрасываем флаг после обработки
                pdFALSE,     // Любое событие
                portMAX_DELAY
        );

        if (bits & BIT0) {
            // Проверка наличия и валидности сообщения
            if (!self->_message.isNull()) {
                String type = self->getValue<String>("type");
                String request = self->getValue<String>("request");

                #ifdef SSVC_DEBUG
                    Serial.print("SsvcConnector::_startTask: Message type: ");
                    Serial.println(type);
                    Serial.print("SsvcConnector::_startTask: Request: ");
                    Serial.println(request);
                #endif

                // Обработка правильного ответа
                if (type == "response" && request == taskParams->expectedRequest) {
                    Serial.println("SsvcConnector::_startTask: Correct response received, calling response handler...");
                    // Передаем объект JsonDocument в обработчик
                    taskParams->responseHandler(self, self->_message);

                    // Сбрасываем флаг завершения и только потом удаляем задачу
                    self->setCommandInProgress(false);
                    if (_need_deleteTask) {
                        vTaskDelete(nullptr); // Удаляем задачу// Сбрасываем флаг после обработки
                    }else {
                        vTaskDelay(pdMS_TO_TICKS(1000*5));
                    }
                } else {
                    #ifdef SSVC_DEBUG
                        Serial.println("SsvcConnector::_startTask: Waiting for correct response...");
                    #endif
                }
            } else {
                Serial.println("SsvcConnector::_startTask: _message is nullptr!");
            }
        } else {
            Serial.println("_startTask: No bits & BIT0");
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void SsvcConnector::_telemetry(void* pvParameters) {
    auto* self = static_cast<SsvcConnector*>(pvParameters);
    char data[1024]; // Буфер для данных
    const int errorThreshold = 10;  // Количество итераций по 500 мс, чтобы достичь 5 секунд
    int errorCounter = 0;  // Счётчик ошибок десериализации

    while (true) {
        // Предполагаем, что данные получаются через UART (замените на ваш код получения данных)
        int len = uart_read_bytes(UART_NUM_1, (uint8_t*)data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len] = '\0';  // Нуль-терминатор для строки
            String jsonStr = String(data);  // Преобразуем данные в строку
            #ifdef SSVC_DEBUG
            Serial.println(jsonStr);
            #endif
            JsonDocument temp;
            DeserializationError error = deserializeJson(temp, jsonStr);
            if (error) {
                errorCounter++;  // Увеличиваем счётчик ошибок
                Serial.print("Ошибка десериализации: ");
                Serial.println(error.c_str());

                if (errorCounter >= errorThreshold) {
                    self->uartCommunicationError = true;  // Устанавливаем ошибку
                }
            }else {
                self->uartCommunicationError = false;
                self->updateMessage(temp);  // Обновляем _message через updateMessage
                xEventGroupSetBits(self->_eventGroup, BIT0 | BIT1);
#ifdef SSVC_DEBUG
                ESP_LOGI("_telemetry", "Установлен бит оповещения подписчиков");
#endif
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
#ifdef SSVC_DEBUG
            ESP_LOGI("_telemetry", "Нет данных по UART");
#endif
        }
        vTaskDelay(pdMS_TO_TICKS(500));  // Пауза для предотвращения перегрузки
    }
}


// Task send command

bool SsvcConnector::taskGetSettingsCommand(){
    auto get_settingsParams = new TaskParameters{
            this,                   // Указатель на текущий экземпляр
            [](SsvcConnector* self) {
#ifdef SSVC_DEBUG
                Serial.println("Running GET_SETTINGS command");
#endif

                SsvcConnector::sendGetSettingsCommand();
            },       // Команда, которую нужно выполнить
            "GET_SETTINGS",        // Ожидаемый тип команды в ответе
            [](SsvcConnector* self, JsonDocument& message) {
                self->updateSettings(message);
//                #ifdef SSVC_DEBUG
                Serial.println("Получен результат команды GET_SETTINGS");
//                #endif
            },
            true
    };

    xTaskCreatePinnedToCore(
            SsvcConnector::_startTask,
            "CommandTask",
            4096,
            get_settingsParams,
            1,
            nullptr,
            tskNO_AFFINITY
    );
    return true;
}

bool SsvcConnector::taskVersionCommand() {
    auto getVersionParams = new TaskParameters{
            this,                   // Указатель на текущий экземпляр
            [](SsvcConnector* self) {
#ifdef SSVC_DEBUG
                Serial.println("Running VERSION command");
#endif
                SsvcConnector::sendVersionCommand();
            },       // Команда, которую нужно выполнить
            "VERSION",        // Ожидаемый тип команды в ответе
            [](SsvcConnector* self, JsonDocument& message) {
                self->updateVersion(message);
//            #ifdef SSVC_DEBUG
                Serial.println("Получен результат команды VERSION");
//            #endif

            },
            true
    };

    xTaskCreatePinnedToCore(
            SsvcConnector::_startTask,
            "CommandVersion",
            2048,
            getVersionParams,
            1,
            nullptr,
            tskNO_AFFINITY
    );
    return true;
}


bool SsvcConnector::taskStopCommand() {
    return createTask(
            "CommandStop",
            "STOP",
            "STOP",
            [](SsvcConnector* self) {
                self->sendStopCommand();
            },
            [](SsvcConnector* self, JsonDocument& message) {
                if (message["type"] == "response" && message["request"] == "STOP") {
                    Serial.println("Команда STOP успешно выполнена");
                }
            }
    );
}

bool SsvcConnector::taskResumeCommand() {
    return createTask(
            "CommandResume",
            "RESUME",
            "RESUME",
            [](SsvcConnector* self) {
                self->sendResumeCommand();
            },
            [](SsvcConnector* self, JsonDocument& message) {
                if (message["type"] == "response" && message["request"] == "RESUME") {
                    Serial.println("Команда RESUME успешно выполнена");
                }
            }
    );
}

bool SsvcConnector::taskPauseCommand() {
    return createTask(
            "CommandPause",
            "PAUSE",
            "PAUSE",
            [](SsvcConnector* self) {
                self->sendPauseCommand();
            },
            [](SsvcConnector* self, JsonDocument& message) {
                if (message["type"] == "response" && message["request"] == "PAUSE") {
                    Serial.println("Команда PAUSE успешно выполнена");
                }
            }
    );
}

bool SsvcConnector::taskNestCommand() {
    return createTask(
            "CommandNext",
            "NEXT",
            "NEXT",
            [](SsvcConnector* self) {
                self->sendNextCommand();
            },
            [](SsvcConnector* self, JsonDocument& message) {
                if (message["type"] == "response" && message["request"] == "NEXT") {
                    Serial.println("Команда NEXT успешно выполнена");
                }
            }
    );
}

// internal command
bool SsvcConnector::sendGetSettingsCommand() {
    return SsvcConnector::sendCommand("GET_SETTINGS\n\r");
}

bool SsvcConnector::sendStopCommand() {
    return SsvcConnector::sendCommand("STOP\n\r");
}

bool SsvcConnector::sendPauseCommand() {
    return SsvcConnector::sendCommand("PAUSE\n\r");
}

bool SsvcConnector::sendResumeCommand() {
    return SsvcConnector::sendCommand("RESUME\n\r");
}

bool SsvcConnector::sendNextCommand() {
    return SsvcConnector::sendCommand("NEXT\n\r");
}

bool SsvcConnector::sendVersionCommand() {
    Serial.println("Отправка команды VERSION");
    bool result = SsvcConnector::sendCommand("VERSION\n\r");
    #ifdef SSVC_DEBUG
        if (result) {
            Serial.println("Команда VERSION успешно отправлена");
        } else {
            Serial.println("Ошибка отправки команды VERSION");
        }
    #endif
    return result;
}

bool SsvcConnector::sendAtCommand() {
    return SsvcConnector::sendCommand("AT\n\r");
}

// ENDER COMMAND

void SsvcConnector::updateMessage(const JsonDocument& ssvcMetrics) {
    _message.clear();
    _message.set(ssvcMetrics);
}

void SsvcConnector::updateSettings(const JsonDocument& ssvcSettings) {
    _ssvcSettings.clear();
    Serial.println(ssvcSettings["settings"].as<String>());
    _ssvcSettings.set(ssvcSettings["settings"]);
}

void SsvcConnector::updateVersion(const JsonDocument& ssvcVersion){
    Serial.println("ssvcVersion is set");
    // Проверяем, существует ли ключ "version" и его тип
    if (!ssvcVersion["version"].isNull() && ssvcVersion["version"].is<String>()) {
        String ver =  ssvcVersion["version"].as<String>();
        _ssvcVersion = ver;  // Присваиваем значение поля "version"
        checkVersionSupported();
        Serial.print("Текущая версия SSVC: ");
        Serial.println(ver);
#ifdef SSVC_SUPPORT_VERSION
        // Перенос проверки функционала поддерживаемой версии
#endif
    } else {
        Serial.println("Поле 'version' не найдено или его тип неверен");
    }
}

bool SsvcConnector::sendCommand(const char *command) {
//    #ifdef SSVC_DEBUG
    Serial.print("Sending command: ");
    Serial.println(command);
//    #endif
    return uart_write_bytes(UART_NUM_1, command, strlen(command)) != -1;
}

void SsvcConnector::checkVersionSupported() {
    const std::string minSupportedVersion = SSVC_MIN_SUPPORT_VERSION;

    // Преобразуем версии в массивы чисел
    auto currentParts = parseVersion(_ssvcVersion.c_str());
    auto minSupportedParts = parseVersion(minSupportedVersion);

    // Определяем максимальную длину для сравнения
    size_t maxLength = std::max(currentParts.size(), minSupportedParts.size());

    // Дополняем недостающие части нулями
    currentParts.resize(maxLength, 0);
    minSupportedParts.resize(maxLength, 0);

    // Сравниваем версии
    for (size_t i = 0; i < maxLength; ++i) {
        if (currentParts[i] < minSupportedParts[i]) {
            isSupportSSVCVersion = false;
            return;  // Если версия не поддерживается, выходим
        }
        if (currentParts[i] > minSupportedParts[i]) {
            isSupportSSVCVersion = true;
            return;  // Если версия поддерживается, выходим
        }
    }

    // Если все части равны, версия поддерживается
    isSupportSSVCVersion = true;
}

std::vector<int> SsvcConnector::parseVersion(const std::string& version) {
    std::vector<int> parts;
    std::stringstream ss(version);
    std::string part;

    while (std::getline(ss, part, '.')) {
        parts.push_back(std::stoi(part));
    }

    return parts;
}







