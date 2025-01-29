
#include "SsvcTelemetryService.h"




SsvcTelemetryService::SsvcTelemetryService(EventSocket *socket,
                                           SecurityManager *securityManager,
                                           RectificationProcess& rProcess) : _eventEndpoint(SsvcTelemetry::read,
                                                                                               SsvcTelemetry::update,
                                                                                               this,
                                                                                               socket,
                                                                                               SSVC_OPEN_CONNECT_ENDPOINT_PATH),
                                                                                _socket(socket),
                                                                                rectificationProcess(rProcess){

    {

        addUpdateHandler([&](const String &originId)
                         { onTelemetryUpdated(); },
                         false);
    }
}


void SsvcTelemetryService::begin()
{
    _eventEndpoint.begin();
    // Регистрируем события
    _socket->registerEvent(SSVC_OPEN_CONNECT_EVENT);
    // Запускаем получение телеметрии
    xTaskCreatePinnedToCore(
            SsvcTelemetryService::update,            // Function that should be called
            "SsvcTelemetry",     // Name of the task (for debugging)
            4096,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (tskIDLE_PRIORITY),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );

//    Инициализируем стартовую задачу

}

void SsvcTelemetryService::update(void* pvParameters) {
    auto* self = static_cast<SsvcTelemetryService*>(pvParameters);

    while (true) {
        std::string message = self->rectificationProcess.getTelemetry();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            Serial.print("Failed to parse JSON: ");
            Serial.println(error.c_str());
            return;
        }
#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG // Выполняем только при уровне Debug или выше
        // Создаем строку для хранения сериализованного JSON
        // Логируем JSON
        ESP_LOGD("RECTIFICATION", "send: %s", message.c_str());
#endif
        JsonObject root = doc.as<JsonObject>();
        self->_socket->emitEvent(SSVC_OPEN_CONNECT_EVENT, root);
        vTaskDelay(1000);
    }
}

void SsvcTelemetryService::onTelemetryUpdated() {
}

void SsvcTelemetry::read(SsvcTelemetry &SsvcOpenConnect, JsonObject &root){
}

StateUpdateResult SsvcTelemetry::update(JsonObject &root, SsvcTelemetry &SsvcOpenConnect)
{
    return StateUpdateResult::UNCHANGED;
}

