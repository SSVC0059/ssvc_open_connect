
#include "SsvcTelemetryService.h"




SsvcTelemetryService::SsvcTelemetryService(PsychicHttpServer *server,
                                           EventSocket *socket,
                                           SecurityManager *securityManager,
                                           EventGroupHandle_t eventGroup) : _httpEndpoint(SsvcTelemetry::read,
                                                                                          SsvcTelemetry::update,
                                                                                          this,
                                                                                          server,
                                                                                          SSVC_OPEN_CONNECT_ENDPOINT_PATH,
                                                                                          securityManager,
                                                                                          AuthenticationPredicates::IS_AUTHENTICATED),
                                                                                _eventEndpoint(SsvcTelemetry::read,
                                                                                               SsvcTelemetry::update,
                                                                                               this,
                                                                                               socket,
                                                                                               SSVC_OPEN_CONNECT_ENDPOINT_PATH),
                                                                                _webSocketServer(SsvcTelemetry::read,
                                                                                                 SsvcTelemetry::update,
                                                                                                 this,
                                                                                                 server,
                                                                                                 SSVC_OPEN_CONNECT_SOCKET_PATH,
                                                                                                 securityManager,
                                                                                                 AuthenticationPredicates::IS_AUTHENTICATED),
                                                                                _socket(socket),
                                                                                _eventGroup(eventGroup){

    {

        // configure settings service update handler to update LED state
        addUpdateHandler([&](const String &originId)
                         { onTelemetryUpdated(); },
                         false);
    }
}


void SsvcTelemetryService::begin()
{
    _httpEndpoint.begin();
    _eventEndpoint.begin();
    _webSocketServer.begin();
    // Регистрируем события
    _socket->registerEvent(SSVC_OPEN_CONNECT_EVENT);
    // Запускаем получение телеметрии
    xTaskCreatePinnedToCore(
            SsvcTelemetryService::_telemetry,            // Function that should be called
            "SSVC Open Telemetry",     // Name of the task (for debugging)
            4096,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (tskIDLE_PRIORITY),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );

//    Инициализируем стартовую задачу

}

void SsvcTelemetryService::_telemetry(void* pvParameters) {
    auto* self = static_cast<SsvcTelemetryService*>(pvParameters);
    while (true) {
        JsonDocument message = rProcess->getRectificationStatus();
        // Сериализация JSON документа и вывод в Serial
//        ESP_LOGV("SsvcTelemetryService::_telemetry: ", output);
        serializeJson(message, Serial);
        Serial.println("");
        JsonObject obj = message.as<JsonObject>();
        self->_socket->emitEvent(SSVC_OPEN_CONNECT_EVENT, obj);
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

