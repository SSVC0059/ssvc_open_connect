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

esp_err_t SensorHandler::getSensorAddresses(PsychicRequest* request)
{
    ESP_LOGD(TAG, "Processing sensor addresses request");
    auto response = PsychicJsonResponse(request, false);
    const JsonObject root = response.getRoot();

    ESP_LOGD(TAG, "Getting sensor addresses by zone");
    const auto sensorsByZone =
        SensorManager::getInstance().getAllSensorsGroupedByZone(MeasuredValueType::TEMPERATURE);
    ESP_LOGD(TAG, "Received %zu zones", sensorsByZone.size());

    const JsonObject zonesJson = root["zones"].to<JsonObject>();
    // Перебор всех зон
    for (const auto& zoneEntry : sensorsByZone) {

        // 1. Преобразуем enum SensorZone в строку для ключа JSON
        const std::string zoneName = SensorZoneHelper::toString(zoneEntry.first);

        ESP_LOGD(TAG, "Processing zone: %s with %zu sensors",
                zoneName.c_str(), zoneEntry.second.size());

        auto sensorArray = zonesJson[zoneName].to<JsonArray>();

        // 2. Перебор всех датчиков в зоне
        for (const auto& sensor : zoneEntry.second) {

            // Получаем 1-Wire адрес в строковом формате
            const std::string addressStr = SensorManager::addressToString(sensor->getAddress());

            // Получаем последнее кэшированное значение
            float temperature = sensor->getData();

            // Проверка на ошибку (например, -127.0f)
            if (temperature == DEVICE_DISCONNECTED_C) {
                ESP_LOGW(TAG, "Sensor %s reported error (%.2f). Excluding from successful response.",
                        addressStr.c_str(), temperature);
                continue; // Пропускаем датчик с ошибкой
            }

            ESP_LOGV(TAG, "Adding sensor address: %s to zone: %s",
                    addressStr.c_str(), zoneName.c_str());

            auto sensorObject = sensorArray.add<JsonObject>();

            sensorObject["address"] = addressStr; // Добавляем адрес
            sensorObject["temp"] = temperature;   // Добавляем температуру

            ESP_LOGV(TAG, "Adding sensor address: %s and temp: %.2f to zone: %s",
                    addressStr.c_str(), temperature, zoneName.c_str());

        }
    }

#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG
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
    const JsonObject root = response.getRoot();

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

    // 1. Создание переменной для 8-байтового адреса (не используется для вызова сервиса, но необходима для проверки формата)
    AbstractSensor::Address addressBytes;

    // 2. Преобразование строкового адреса в массив байт
    if (!SensorManager::stringToAddress(address, addressBytes)) {
        // Обработка ошибки, если строка имеет неверный формат (например, не 16 hex-символов)
        root["status"] = "error";
        root["message"] = "Invalid 1-Wire address format. Must be 16 hex characters.";
        root["address"] = address;
        response.setCode(400);
        return response.send();
    }

    // Преобразование зоны
    SensorZone zone;
    try {
        zone = SensorZoneHelper::fromString(zoneName);
    } catch (const std::invalid_argument& e) {
        root["status"] = "error";
        root["message"] = "Invalid zone value. Valid values: unknown, inlet_water, outlet_water, act";
        root["received_zone"] = zoneName;
        response.setCode(400);
        return response.send();
    }
    SensorConfigService* service = SsvcOpenConnect::getInstance().getSensorConfigService();
    if (!service) {
        // Логирование ошибки и возврат 500, если сервис не найден
        root["message"] = "Zone Service not initialized";
        response.setCode(500);
        return response.send();
    }

    // ВЫЗОВ СЕРВИСА: Используем корректное имя переменной 'address'
    const bool success = service->setZoneForSensor(address, zone);

    // Формирование ответа
    if (success) {
        root["status"] = "success";
        root["message"] = "Zone updated successfully (Persistence initiated)"; // Добавлено уточнение про персистентность
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
