//
// Created by demoncat on 05.05.25.
//

#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include "PsychicHttp.h"
#include <sensors/Thermal/ThermalSensors.h>
#include <vector>
#include <utility>

class SensorHandler {
public:
    SensorHandler();

    static esp_err_t getSensorAddresses(PsychicRequest* request);
    static esp_err_t updateSensorZone(PsychicRequest* request);
    static esp_err_t getTemperatures(PsychicRequest* request);

private:
    static void parseQueryParams(const String& query,
                               std::vector<std::pair<String, String>>& output);
};

#endif