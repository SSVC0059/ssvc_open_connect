#include "SettingsHandler.h"

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

SettingsHandler::SettingsHandler() = default;

esp_err_t SettingsHandler::updateSettings(PsychicRequest* request)
{
    JsonDocument jsonBuffer;
    bool hasErrors = false;
    auto errors = jsonBuffer.to<JsonObject>();
    SsvcSettings::Builder builder;

    // Проверяем, есть ли данные в теле запроса (JSON)
    if (request->contentLength() > 0) {
        // Парсим JSON из тела запроса
        JsonDocument bodyDoc;
        DeserializationError err = deserializeJson(bodyDoc, request->body());

        if (err) {
            return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        }

        // Обрабатываем JSON объект из тела
        auto bodyObj = bodyDoc.as<JsonObject>();
        for (JsonPair kv : bodyObj) {
            const String& key = kv.key().c_str();
            JsonVariant value = kv.value();

            ESP_LOGV("HTTP", "Processing body parameter: %s", key.c_str());

            auto it = PARAM_HANDLERS.find(key);
            if (it == PARAM_HANDLERS.end()) {
                errors[key] = "Unknown parameter";
                // hasErrors = true;
                continue;
            }

            if (!it->second->handle(builder, value)) {
                errors[key] = "Invalid value type";
                hasErrors = true;
            }
        }
    }
    // Если нет тела, проверяем query-параметры (старый подход)
    else {
        std::vector<std::pair<String, String>> params;
        String query = request->query();
        if (query.isEmpty()) {
            ESP_LOGV("", "No parameters received");
            return request->reply(400, "application/json", R"({"error": "No parameters received"})");
        }

        parseQueryParams(query, params);

        // Обрабатываем каждый параметр (старая логика)
        for (const auto& param : params) {
            const String& key = param.first;
            const String& valueStr = param.second;

            ESP_LOGV("HTTP", "Processing query parameter: %s=%s", key.c_str(),
                     valueStr.c_str());

            JsonDocument valueDoc;
            DeserializationError err = deserializeJson(valueDoc, valueStr);
            if (err) {
                errors[key] = "Invalid JSON format";
                hasErrors = true;
                continue;
            }

            auto it = PARAM_HANDLERS.find(key);
            if (it == PARAM_HANDLERS.end()) {
                errors[key] = "Unknown parameter";
                hasErrors = true;
                continue;
            }

            if (!it->second->handle(builder, valueDoc.as<JsonVariant>())) {
                errors[key] = "Invalid value type";
                hasErrors = true;
            }
        }
    }

    if (hasErrors) {
        String errorMsg;
        serializeJson(errors, errorMsg);
        return request->reply(400, "application/json", errorMsg.c_str());
    }

    // Ожидание с проверкой флага
    ESP_LOGI("HTTP", "Waiting for _cmdSetResult...");
    TickType_t timeoutAbsolute = xTaskGetTickCount() + pdMS_TO_TICKS(10000);
    bool cmdResult = false;

    while (xTaskGetTickCount() < timeoutAbsolute) {
        cmdResult = SsvcCommandsQueue::getQueue()._cmdSetResult;
        ESP_LOGI("HTTP", "cmdResult: %d", cmdResult);
        if (cmdResult) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGV("HTTP", "Finished waiting for _cmdSetResult.");

    if (!cmdResult) {
        JsonDocument timeoutResponse;
        timeoutResponse["error"] = "Timeout";
        String timeoutMsg;
        serializeJson(timeoutResponse, timeoutMsg);
        return request->reply(408, "application/json", timeoutMsg.c_str());
    }

    SsvcCommandsQueue::getQueue().getSettings();

    JsonDocument successResponse;
    successResponse["success"] = true;
    successResponse["message"] = "Settings updated successfully";
    String successMsg;
    serializeJson(successResponse, successMsg);
    return request->reply(200, "application/json", successMsg.c_str());
}

void SettingsHandler::parseQueryParams(const String& query,
                                    std::vector<std::pair<String, String>>& output) {
    unsigned int start = 0;
    while (start < query.length()) {
        // Ищем конец пары
        unsigned int end = query.indexOf('&', start);
        if (end == -1)
            end = query.length();

        // Вырезаем отдельную пару
        String pair = query.substring(start, end);

        // Разделяем на ключ и значение
        const int eqPos = pair.indexOf('=');
        String name = (eqPos != -1) ? pair.substring(0, eqPos) : pair;
        String value = (eqPos != -1) ? pair.substring(eqPos + 1) : "";

        // Добавляем в результат
        output.emplace_back(name, value);

        // Переходим к следующей паре
        start = end + 1;
    }
}