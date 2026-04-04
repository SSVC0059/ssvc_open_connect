//
// Created by demoncat on 05.05.25.
//

#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

#include <vector>
#include <utility>

class SensorHandler {
public:
    SensorHandler();

    static void getSensorAddresses(AsyncWebServerRequest* request);
    static void updateSensorZone(AsyncWebServerRequest* request);

private:
    static void parseQueryParams(const String& query,
                               std::vector<std::pair<String, String>>& output);

};

#endif