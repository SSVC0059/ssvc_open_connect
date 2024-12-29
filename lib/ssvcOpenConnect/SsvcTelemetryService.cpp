
#include "SsvcTelemetryService.h"




SsvcTelemetryService::SsvcTelemetryService(EventSocket *socket,
                                           SecurityManager *securityManager,
                                           EventGroupHandle_t eventGroup,
                                           RectificationProcess* rProcess) : _eventEndpoint(SsvcTelemetry::read,
                                                                                               SsvcTelemetry::update,
                                                                                               this,
                                                                                               socket,
                                                                                               SSVC_OPEN_CONNECT_ENDPOINT_PATH),
                                                                                _socket(socket),
                                                                                _eventGroup(eventGroup),
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
            SsvcTelemetryService::_telemetry,            // Function that should be called
            "SSVC Open Telemetry",     // Name of the task (for debugging)
            4096,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (tskIDLE_PRIORITY + 1),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );

//    Инициализируем стартовую задачу

}

void SsvcTelemetryService::_telemetry(void* pvParameters) {
    auto* self = static_cast<SsvcTelemetryService*>(pvParameters);
    while (true) {
        if (self->rectificationProcess == nullptr){
            log_printf("SsvcTelemetryService", "rectificationProcess is null");
            vTaskDelay(1000);
        }else {
            JsonDocument message = self->rectificationProcess->getRectificationStatus();
#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG // Выполняем только при уровне Debug или выше
            // Создаем строку для хранения сериализованного JSON
            String jsonString;
            serializeJson(message, jsonString);

            // Логируем JSON
            ESP_LOGD("RECTIFICATION", "%s", jsonString.c_str());
#endif
            JsonObject obj = message.as<JsonObject>();
            self->_socket->emitEvent(SSVC_OPEN_CONNECT_EVENT, obj);
            vTaskDelay(1000);
        }
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

