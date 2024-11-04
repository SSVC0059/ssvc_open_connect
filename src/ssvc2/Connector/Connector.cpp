#include "Connector.h"

Connector::Connector(uart_port_t uart_num, gpio_num_t tx_pin, gpio_num_t rx_pin)
    : uart_num(uart_num), tx_pin(tx_pin), rx_pin(rx_pin), running(false) {
    // Настройка UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(uart_num, 1024, BUF_SIZE * 2, BUF_SIZE * 2, nullptr, 0);
    uart_param_config(uart_num, &uart_config);
    uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Инициализация кольцевого буфера
    if (ringBuffer == nullptr) {
        Logger::error("Failed to allocate memory for ring buffer");
        // Обработка ошибки
    }
    memset(ringBuffer, 0, ring_buffer_size);
    head = 0;
    tail = 0;

}

void Connector::start() {
    xTaskCreatePinnedToCore(taskReceiveData, "ReceiveDataTask", 4096, this, 1, nullptr, 1);
    xTaskCreatePinnedToCore(taskProcessData, "ProcessDataTask", 4096, this, 1, nullptr, 1);
    xTaskCreatePinnedToCore(taskSendCommandAT, "SendCommandATTask", 4096, this, 1, nullptr, 1);
}

void Connector::stop() {

}

void Connector::taskReceiveData(void *pvParameters) {
    Connector *connector = static_cast<Connector *>(pvParameters);
    // char* data = (char*) malloc(BUF_SIZE);
    char data[1024]; // Буфер для данных
    while (true) {
        int len = uart_read_bytes(UART_NUM_1, (uint8_t*)data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len] = '\0'; // Нуль-терминатор для строки

            // Добавление данных в кольцевой буфер
            connector->addToBuffer(data);
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // Пауза для предотвращения перегрузки
    }
}

// Обработчик очереди телеметрии
void Connector::taskProcessData(void *pvParameters) {
    Connector* connector = static_cast<Connector*>(pvParameters);
    while (true) {
        char* data = connector->getDataFromBuffer();
        if (data) {
            connector->processData(data);
            free(data); // Освобождаем память
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Пауза между обработкой данных
    }
    vTaskDelete(nullptr);
}

// 
void Connector::processData(const char* data) {
    // Пример обработки JSON
    JsonDocument doc; // Убедитесь, что размер документа подходит
    DeserializationError error = deserializeJson(doc, data);
    
    if (!error) {
        Logger::debug("Данные полученые из SSVC: ");
        Logger::debug(data);
        // Здесь обработка данных
        if (awaitingResponse && doc["type"] == "response") {
            Logger::debug("Received response from device.");
            awaitingResponse = false; // Сбрасываем флаг после получения ответа
        }
         // Проверка тайм-аута на получение ответа
        if (awaitingResponse && (esp_timer_get_time() - commandSentTime > responseTimeout)) {
            Logger::error("Device did not respond in time.");
            awaitingResponse = false; // Сбрасываем флаг при тайм-ауте
        }
        // TODO: Сюда подключаются обработчики сообщений от ssvc
        // Отправка телеметрии через TelemetryHandler
        telemetryHandler.sendData(doc);
        const char* value = doc["type"];
        Logger::debug(value);
    } else {
        Logger::error("Failed to parse JSON");
    }
}

void Connector::taskSendCommandAT(void *pvParameters) {
    Connector *connector = static_cast<Connector *>(pvParameters);
    while (true) {
        // Отправка команды AT
        connector->sendCommandAT();
        // Отпавка команды GET_SETTINGS
        // connector->sendCommandGetSettings();
        vTaskDelay(20000 / portTICK_PERIOD_MS); // Отправлять каждые 20 секунд
    }
    vTaskDelete(nullptr);
}

void Connector::sendCommandAT(){
    const char *command = "AT\n\r";
    Logger::debug("send command: AT");
    Connector::sendCommand(command);
}

void Connector::sendCommandGetSettings() {
    const char *command = "GET_SETTINGS\n\r";
    Logger::debug("send command: GET_SETTINGS");
    Connector::sendCommand(command);
}

void Connector::sendCommandStop()
{
    const char *command = "STOP\n\r";
    Logger::debug("send command: GET_SETTINGS");
    Connector::sendCommand(command);
}

void Connector::sendCommandPause()
{
    const char *command = "PAUSE\n\r";
    Logger::debug("send command: GET_SETTINGS");
    Connector::sendCommand(command);
}

void Connector::sendCommandGetVersion()
{
    const char *command = "VERSION\n\r";
    Logger::debug("send command: GET_SETTINGS");
    Connector::sendCommand(command);
}

void Connector::sendCommand(const char *command){
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    Connector::awaitingResponse = true;
    Connector::commandSentTime = esp_timer_get_time();
}


void Connector::addToBuffer(const char* data) {
    size_t len = strlen(data);
    for (size_t i = 0; i < len; ++i) {
        ringBuffer[head] = data[i];
        head = (head + 1) % ring_buffer_size;
        if (head == tail) {
            // Если буфер переполнен, переместим tail
            tail = (tail + 1) % ring_buffer_size;
        }
    }
}

char* Connector::getDataFromBuffer() {
    if (head == tail) {
        return nullptr; // Если буфер пуст
    }
    // Создаем новый буфер для данных
    char* data = (char*)malloc(ring_buffer_size); //
    size_t index = 0;

    while (tail != head && index < ring_buffer_size - 1) {
        data[index++] = ringBuffer[tail];
        tail = (tail + 1) % ring_buffer_size;
    }
    data[index] = '\0'; // Нуль-терминатор для строки

    return data; // Возвращаем данные для обработки
}