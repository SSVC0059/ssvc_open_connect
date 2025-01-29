//
// Created by demoncat on 26.11.2024.
//

#include "SsvcConnector.h"

// Инициализация статической переменной
SsvcConnector* SsvcConnector::_ssvcConnector = nullptr;

SsvcConnector::SsvcConnector() {
    uartCommunicationError = false;
    lastSettings = "";
    lastMessage = "";
    InitUartDriver();
}

SsvcConnector& SsvcConnector::getConnector()  {
    if (!_ssvcConnector) {
        _ssvcConnector = new SsvcConnector();
    }
    return *_ssvcConnector;  // Возвращаем указатель на экземпляр
}

void SsvcConnector::InitUartDriver() {
    ESP_LOGI("SsvcConnector", "Start SsvcConnector begin()");
    gpio_set_pull_mode(SSVC_OPEN_CONNECT_UART_RX, GPIO_PULLUP_ONLY);

    constexpr uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(SSVC_OPEN_CONNECT_UART_NUM,
                        SSVC_OPEN_CONNECT_BUF_SIZE * 2,
                        SSVC_OPEN_CONNECT_BUF_SIZE * 2,
                        0,
                        nullptr,
                        0);
    uart_param_config(SSVC_OPEN_CONNECT_UART_NUM, &uart_config);
    uart_set_pin(SSVC_OPEN_CONNECT_UART_NUM,
                 SSVC_OPEN_CONNECT_UART_TX,
                 SSVC_OPEN_CONNECT_UART_RX,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    ESP_LOGV("SsvcConnector", "Starting loop task");

    SsvcConnector::initSsvcController();
};

// CRON
void SsvcConnector::initSsvcController() {
    ESP_LOGV("SsvcConnector", "start _telemetry task");

    // Запуск сбора телеметрии
    xTaskCreatePinnedToCore(
            SsvcConnector::_telemetry,            // Function that should be called
            "TelemetryTask",     // Name of the task (for debugging)
            4096,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (tskIDLE_PRIORITY + 1),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );

}


[[noreturn]] void SsvcConnector::_telemetry(void* pvParameters) {
    auto* self = static_cast<SsvcConnector*>(pvParameters);
    static char data[1024]; // Буфер для данных
    const int errorThreshold = 10;  // Количество ошибок для признаков сбоя
    int errorCounter = 0;

    while (true) {
        UBaseType_t stackWaterMark = uxTaskGetStackHighWaterMark(nullptr);
//        ESP_LOGI("SsvcConnector", "Telemetry water mark: %u", stackWaterMark);

        size_t data_len = 0;
        uart_get_buffered_data_len(SSVC_OPEN_CONNECT_UART_NUM, &data_len);
        if (data_len > SSVC_OPEN_CONNECT_BUF_SIZE) {
            ESP_LOGE("SsvcConnector", "Buffer overflow detected, clearing buffer!");
            uart_flush(SSVC_OPEN_CONNECT_UART_NUM);  // Очистка буфера
        }

        memset(data, 0, sizeof(data));
        int idx = 0;

        while (true) {
            // Чтение одного байта за раз
            int len = uart_read_bytes(SSVC_OPEN_CONNECT_UART_NUM, (uint8_t*)&data[idx], 1, pdMS_TO_TICKS(100));
            if (len > 0) {
                // Если встретили символ переноса строки, завершаем чтение
                if (data[idx] == '\n') {
                    data[idx] = '\0';  // Завершаем строку
                    break;
                }
                idx++;
                // Проверка на переполнение буфера
                if (idx >= sizeof(data) - 1) {
                    data[sizeof(data) - 1] = '\0'; // Ограничиваем строку
                    break;
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
#ifdef SSVC_DEBUG
                ESP_LOGI("_telemetry", "Нет данных по UART");
#endif
            }
        }

        // После чтения всей строки пробуем десериализовать JSON
#ifdef CONFIG_IDF_TARGET_ESP32S3
        SpiRamAllocator allocator;
        JsonDocument doc(&allocator);
#else
        JsonDocument doc;
#endif
        DeserializationError error = deserializeJson(doc, data);
        if (error) {
            errorCounter++;
            ESP_LOGE("SsvcConnector", "Ошибка десериализации: %s", error.c_str());

            if (errorCounter >= errorThreshold) {
                self->uartCommunicationError = true;
            }
        } else {

            ESP_LOGV("SsvcConnector", "Начало вывода данных", data);
            ESP_LOGV("SsvcConnector", "%s", data);
            ESP_LOGV("SsvcConnector", "Конец вывода данных");

            self->uartCommunicationError = false;
            if (doc["type"] == "response" && doc["request"] == "GET_SETTINGS") {
                xEventGroupSetBits(eventGroup, BIT10);
            } else if (doc["type"] == "response" && doc["request"] == "STOP") {
                xEventGroupSetBits(eventGroup, BIT11);
            } else if (doc["type"] == "response" && doc["request"] == "PAUSE") {
                xEventGroupSetBits(eventGroup, BIT12);
            } else if (doc["type"] == "response" && doc["request"] == "RESUME") {
                xEventGroupSetBits(eventGroup, BIT13);
            } else if (doc["type"] == "response" && doc["request"] == "START") {
                xEventGroupSetBits(eventGroup, BIT14);
            } else if (doc["type"] == "response" && doc["request"] == "AT") {
                xEventGroupSetBits(eventGroup, BIT15);
            }else {
//              телеметрия и все остальное
                xEventGroupSetBits(eventGroup, BIT0);
            }

            if (xSemaphoreTake(mutex, portMAX_DELAY)) {
                self->lastMessage = std::string(data);
                ESP_LOGV("SsvcConnector", "lastMessage: %s", self->lastMessage.c_str());
                xSemaphoreGive(mutex);  // Копирование данных
            } else {
                ESP_LOGE("SsvcConnector", "Не удалось захватить мьютекс для lastMessage");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Пауза для предотвращения перегрузки
    }
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
    return SsvcConnector::sendCommand("VERSION\n\r");
}

bool SsvcConnector::sendAtCommand() {
    return SsvcConnector::sendCommand("AT\n\r");
}

// ENDER COMMAND
bool SsvcConnector::sendCommand(const char *command) {
    ESP_LOGV("SsvcConnector", "Отправка команды SSVC: %s", command); // Логирование команды
    return uart_write_bytes(UART_NUM_1, command, strlen(command)) != -1;
}

std::string SsvcConnector::getLastMessage() {
    // Возвращаем строку вместо указателя
    return lastMessage;
}

bool SsvcConnector::sendSetCommand(const std::string& newSettings) {
    ESP_LOGV("SsvcConnector", "Отправка настроек SSVC: %s", newSettings.c_str()); // Логирование команды
    return false;
}

