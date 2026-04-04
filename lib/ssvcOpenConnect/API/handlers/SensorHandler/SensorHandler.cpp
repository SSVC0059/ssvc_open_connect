#include "SensorHandler.h"

/**
*   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive SvelteKit web interface for
 *   monitoring and controlling the distillation process.
 *   https://github.com/SSVC0059/ssvc_open_connect
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 *
 *   This software is independent and not affiliated with SSVC0059 company.
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *
 *   Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/

#include "DallasTemperature.h"
#include "esp_log.h"
#include "components/sensors/SensorManager/SensorManager.h"
#include "core/SsvcOpenConnect.h"
#include "core/StatefulServices/SensorConfigService/SensorConfigService.h"

#define TAG "SensorHandler"

SensorHandler::SensorHandler() = default;


void SensorHandler::updateSensorZone(AsyncWebServerRequest* request)
{
    // Проверка обязательных параметров
    if (!request->hasParam("address") || !request->hasParam("zone")) {
        request->send(400, "application/json", R"({"status":"error","message":"Missing required parameters: 'address' and 'zone'"})");
        return;
    }

    const char* address = request->getParam("address")->value().c_str();
    const std::string zoneName = request->getParam("zone")->value().c_str();

    AbstractSensor::Address addressBytes;
    if (!SensorManager::stringToAddress(address, addressBytes)) {
        request->send(400, "application/json", R"({"status":"error","message":"Invalid 1-Wire address format. Must be 16 hex characters.","address":")" + String(address) + "\"}");
        return;
    }

    SensorZone zone;
    try {
        zone = SensorZoneHelper::fromString(zoneName);
    } catch (const std::invalid_argument& e) {
        String body = R"({"status":"error","message":"Invalid zone value. Valid values: unknown, inlet_water, outlet_water, act","received_zone":")" + String(zoneName.c_str()) + "\"}";
        request->send(400, "application/json", body);
        return;
    }
    SensorConfigService* service = SsvcOpenConnect::getInstance().getSensorConfigService();
    if (!service) {
        request->send(500, "application/json", R"({"message":"Zone Service not initialized"})");
        return;
    }

    const bool success = service->setZoneForSensor(address, zone);

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    if (success) {
        root["status"] = "success";
        root["message"] = "Zone updated successfully (Persistence initiated)";
        root["address"] = address;
        root["zone"] = zoneName.c_str();
    } else {
        root["status"] = "error";
        root["message"] = "Failed to update zone. Sensor not found or invalid parameters";
        root["address"] = address;
        root["zone"] = zoneName.c_str();
        response->setLength();
        String str;
        serializeJson(root, str);
        request->send(404, "application/json", str);
        delete response;
        return;
    }

    response->setLength();
    request->send(response);
}

void SensorHandler::parseQueryParams(const String& query,
                                   std::vector<std::pair<String, String>>& output) {
    unsigned int start = 0;
    while (start < query.length()) {
        unsigned int end = query.indexOf('&', start);
        if (end == -1) end = query.length();

        String pair = query.substring(start, end);
        const int eqPos = pair.indexOf('=');
        String name = (eqPos != -1) ? pair.substring(0, eqPos) : pair;
        String value = (eqPos != -1) ? pair.substring(eqPos + 1) : "";

        output.emplace_back(name, value);
        start = end + 1;
    }
}
