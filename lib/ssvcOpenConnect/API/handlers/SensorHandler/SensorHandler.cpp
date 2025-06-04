//
// Created by demoncat on 05.05.25.
//

#include "SensorHandler.h"
#include "esp_log.h"

#define TAG "SensorHandler"

SensorHandler::SensorHandler() {}

esp_err_t SensorHandler::getSensorAddresses(PsychicRequest* request)
{
    ESP_LOGD(TAG, "Processing sensor addresses request");
    auto response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();

    ESP_LOGD(TAG, "Getting sensor addresses by zone");
    auto sensorsByZone = ThermalSensors::thermalController().getSensorAddressesByZone();
    ESP_LOGD(TAG, "Received %zu zones", sensorsByZone.size());

    JsonObject zonesJson = root["zones"].to<JsonObject>();
    for (const auto& zoneEntry : sensorsByZone) {
        ESP_LOGD(TAG, "Processing zone: %s with %zu sensors",
                zoneEntry.first.c_str(), zoneEntry.second.size());

        JsonArray addressesJson = zonesJson[zoneEntry.first].to<JsonArray>();
        for (const auto& address : zoneEntry.second) {
            ESP_LOGV(TAG, "Adding sensor address: %s to zone: %s",
                    address.c_str(), zoneEntry.first.c_str());
            (void)addressesJson.add(address);
        }
    }

#ifdef CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG
    String jsonOutput;
    serializeJson(root, jsonOutput);
    ESP_LOGD(TAG, "Response JSON: %s", jsonOutput.c_str());
#endif

    response.setCode(200);
    return response.send();
}

esp_err_t SensorHandler::updateSensorZone(PsychicRequest* request)
{
    auto response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();

    // Проверка обязательных параметров
    if (!request->hasParam("address") || !request->hasParam("zone")) {
        root["status"] = "error";
        root["message"] = "Missing required parameters: 'address' and 'zone'";
        response.setCode(400);
        return response.send();
    }

    // Получение параметров
    const std::string address = request->getParam("address")->value().c_str();
    const std::string zoneName = request->getParam("zone")->value().c_str();

    // Преобразование зоны
    ThermalSensorZone zone;
    try {
        zone = ThermalSensorZoneHelper::fromString(zoneName);
    } catch (const std::invalid_argument& e) {
        root["status"] = "error";
        root["message"] = "Invalid zone value. Valid values: unknown, inlet_water, outlet_water, act";
        root["received_zone"] = zoneName;
        response.setCode(400);
        return response.send();
    }

    // Обновление зоны датчика
    const bool success = ThermalSensors::thermalController().setSensorZone(address, zone);

    // Формирование ответа
    if (success) {
        root["status"] = "success";
        root["message"] = "Zone updated successfully";
        root["address"] = address;
        root["zone"] = zoneName;
        response.setCode(200);
    } else {
        root["status"] = "error";
        root["message"] = "Failed to update zone. Sensor not found or invalid parameters";
        root["address"] = address;
        root["zone"] = zoneName;
        response.setCode(404);
    }

    return response.send();
}

esp_err_t SensorHandler::getTemperatures(PsychicRequest* request)
{
    auto response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    JsonArray sensors = root["sensors"].to<JsonArray>();

    // Получение температур
    const auto& temperatures = ThermalSensors::thermalController().getTemperatures();
    root["count"] = temperatures.size();
    root["timestamp"] = millis();

    // Формирование ответа
    for (const auto& sensor : temperatures) {
        JsonObject item = sensors.add<JsonObject>();
        item["address"] = sensor.address;
        item["zone"] = sensor.zone;
        item["temperature"] = sensor.temperature;
    }

#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG
    String jsonOutput;
    serializeJson(root, jsonOutput);
    ESP_LOGD(TAG, "Temperature response: %s", jsonOutput.c_str());
#endif

    return response.send();
}

void SensorHandler::parseQueryParams(const String& query,
                                   std::vector<std::pair<String, String>>& output) {
    unsigned int start = 0;
    while (start < query.length()) {
        unsigned int end = query.indexOf('&', start);
        if (end == -1) end = query.length();

        String pair = query.substring(start, end);
        int eqPos = pair.indexOf('=');
        String name = (eqPos != -1) ? pair.substring(0, eqPos) : pair;
        String value = (eqPos != -1) ? pair.substring(eqPos + 1) : "";

        output.emplace_back(name, value);
        start = end + 1;
    }
}
