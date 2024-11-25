
#include "SsvcOpenConnectService.h"




SsvcOpenConnectService::SsvcOpenConnectService(PsychicHttpServer *server,
                                               EventSocket *socket,
                                               SecurityManager *securityManager,
                                               PsychicMqttClient *mqttClient) : _httpEndpoint(SsvcOpenConnect::read,
                                                                                              SsvcOpenConnect::update,
                                                                                              this,
                                                                                              server,
                                                                                              SSVC_OPEN_CONNECT_ENDPOINT_PATH,
                                                                                              securityManager,
                                                                                              AuthenticationPredicates::IS_AUTHENTICATED),
                                                                                _eventEndpoint(SsvcOpenConnect::read,
                                                                                               SsvcOpenConnect::update,
                                                                                               this,
                                                                                               socket,
                                                                                               SSVC_OPEN_CONNECT_ENDPOINT_PATH),
                                                                                _mqttEndpoint(SsvcOpenConnect::homeAssistRead,
                                                                                              SsvcOpenConnect::homeAssistUpdate,
                                                                                              this,
                                                                                              mqttClient),
                                                                                _webSocketServer(SsvcOpenConnect::read,
                                                                                                 SsvcOpenConnect::update,
                                                                                                 this,
                                                                                                 server,
                                                                                                 SSVC_OPEN_CONNECT_SOCKET_PATH,
                                                                                                 securityManager,
                                                                                                 AuthenticationPredicates::IS_AUTHENTICATED),
                                                                                _mqttClient(mqttClient),
                                                                                _socket(socket){

    {

        // configure settings service update handler to update LED state
        addUpdateHandler([&](const String &originId)
                         { onTelemetryUpdated(); },
                         false);
    }
}


void SsvcOpenConnectService::begin()
{
    _httpEndpoint.begin();
    _eventEndpoint.begin();
    _webSocketServer.begin();
    // Регистрируем события
    _socket->registerEvent(SSVC_OPEN_CONNECT_EVENT);
    // Запускаем получение телеметрии
    this->uartInitializing();
}

void SsvcOpenConnectService::uartInitializing() {
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

    xTaskCreatePinnedToCore(
    this->_loop(),            // Function that should be called
    "SSVC Open Connect",     // Name of the task (for debugging)
    4096,                       // Stack size (bytes)
    this,                       // Pass reference to this class instance
    (configMAX_PRIORITIES - 1),     // task priority
    nullptr,                       // Task handle
    1 // Pin to application core
    );
}

[[noreturn]] TaskFunction_t SsvcOpenConnectService::_loop() const {
    char data[1024]; // Буфер для данных
    JsonDocument doc;
    while (true) {
        const int len = uart_read_bytes(UART_NUM_1, (uint8_t*)data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len] = '\0'; // Нуль-терминатор для строки
            DeserializationError error = deserializeJson(doc, data);
            if (!error) {
                auto jsonObject = doc.as<JsonObject>();
                _socket->emitEvent(SSVC_OPEN_CONNECT_EVENT, jsonObject);
            }else {
                #ifdef SERIAL_INFO
                    Serial.print(F("Ошибка разбора JSON: "));
                    Serial.println(error.f_str());
                #endif
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // Пауза для предотвращения перегрузки
    }}

void SsvcOpenConnectService::onTelemetryUpdated() {
    Serial.println("onTelemetryUpdated");
}

// SsvcOpenConnect

void SsvcOpenConnect::read(SsvcOpenConnect &SsvcOpenConnect, JsonObject &root)
{
    // Serial.println("SsvcOpenConnect::read");
}

StateUpdateResult SsvcOpenConnect::update(JsonObject &root, SsvcOpenConnect &SsvcOpenConnect)
{
    return commandHandler(root["commands"]);
}

StateUpdateResult SsvcOpenConnect::commandHandler(const String &_command) {
    if (_command == "GET_SETTINGS") {
        sendCommand("GET_SETTINGS\n\r");
    }
    else if (_command == "STOP") {
        sendCommand("STOP\n\r");
    }
    else if (_command == "PAUSE") {
        sendCommand("PAUSE\n\r");
    }
    else if (_command == "RESUME") {
        sendCommand("RESUME\n\r");
    }
    else if (_command == "NEXT") {
        sendCommand("NEXT\n\r");
    }
    else if (_command == "VERSION") {
        sendCommand("VERSION\n\r");
    }
    else {
        return StateUpdateResult::ERROR;
    }

    return StateUpdateResult::CHANGED;
}
