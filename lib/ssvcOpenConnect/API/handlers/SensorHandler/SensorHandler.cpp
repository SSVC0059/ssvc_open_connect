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
