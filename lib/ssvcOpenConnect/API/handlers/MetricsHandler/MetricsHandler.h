//
// Created by demoncat on 05.05.25.
//

#ifndef METRICS_HANDLER_H
#define METRICS_HANDLER_H

#include "PsychicHttp.h"
#include "rectification/RectificationProcess.h"
#include "ArduinoJson.h"

class MetricsHandler {
public:
    explicit MetricsHandler(RectificationProcess& rProcess);

    esp_err_t getTelemetry(PsychicRequest* request) const;
    static esp_err_t getMetrics(PsychicRequest* request);

private:
    RectificationProcess& _rProcess;
};

#endif