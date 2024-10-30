#include "routes.h"
#include <ArduinoJson.h>


void initRoutes(AsyncWebServer& server) {
    // Обработчик для GET-запроса
    server.on("/get_setting_ssvc", HTTP_GET, handleGetSetingSSVC);
}

// Получение настроек SSVC
void handleGetSetingSSVC(AsyncWebServerRequest *request) {
    sendCommandToSsvc("GET_SETTINGS");
    request->send(200, "application/json", "OK");
    return;
}
