//
// Created by demoncat on 05.05.25.
//

#ifndef TELEMETRY_HANDLER_H
#define TELEMETRY_HANDLER_H

#include "PsychicHttp.h"
#include "SecurityManager.h"
#include "rectification/RectificationProcess.h"

class TelemetryHandler {
public:
    explicit TelemetryHandler(RectificationProcess& rProcess);

    esp_err_t handleRequest(PsychicRequest* request) const;

private:
    RectificationProcess& _rProcess;

    static constexpr const char* TAG = "TelemetryHandler";
};

#endif //TELEMETRY_HANDLER_H
