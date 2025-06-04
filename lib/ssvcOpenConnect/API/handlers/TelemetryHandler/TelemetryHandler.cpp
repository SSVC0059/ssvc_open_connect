//
// Created by demoncat on 05.05.25.
//

#include "TelemetryHandler.h"

TelemetryHandler::TelemetryHandler(RectificationProcess& rProcess)
    : _rProcess(rProcess) {}

esp_err_t TelemetryHandler::handleRequest(PsychicRequest* request) const {
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    std::string message = _rProcess.getTelemetry();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
        response.setCode(500);
        return response.send();
    }

    root["telemetry"] = doc;

    if (request->hasParam("mode")) {
        std::string type = request->getParam("mode")->value().c_str();
        root["request"] = type;
        JsonVariant _response = root["status"].to<JsonVariant>();
        if (type == "status") {
            if (_rProcess.getStatus(_response)) {
                response.setCode(200);
            } else {
                response.setCode(500);
            }
        }
    }

    return response.send();
}
