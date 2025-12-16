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
    ESP_LOGD(TAG, "Received updateSettings request.");
    JsonDocument jsonBuffer;
    bool hasErrors = false;
    auto errors = jsonBuffer.to<JsonObject>();

    // Проверяем, есть ли данные в теле запроса (JSON)
    if (request->contentLength() > 0) {
        SsvcSettings::Builder builder;
        // Парсим JSON из тела запроса
        ESP_LOGD(TAG, "Request has body. Length: %d", request->contentLength());
        ESP_LOGV(TAG, "Request body: %s", request->body().c_str());

        JsonDocument bodyDoc;
        DeserializationError err = deserializeJson(bodyDoc, request->body());

        if (err) {
            ESP_LOGE(TAG, "Failed to parse JSON body: %s", err.c_str());
            return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        }

        // Обрабатываем JSON объект из тела
        ESP_LOGD(TAG, "Successfully parsed JSON body.");
        auto bodyObj = bodyDoc.as<JsonObject>();
        for (JsonPair kv : bodyObj) {
            const String& key = kv.key().c_str();
            JsonVariant value = kv.value();
            ESP_LOGV("HTTP", "Processing body parameter: %s", key.c_str());
            ESP_LOGV(TAG, "Processing parameter: '%s'", key.c_str());

            auto it = PARAM_HANDLERS.find(key);
            if (it == PARAM_HANDLERS.end()) {
                errors[key] = "Unknown parameter";
                // hasErrors = true;
                continue;
            }

            if (!it->second->handle(builder, value)) {
                ESP_LOGE(TAG, "Invalid value for parameter: '%s'", key.c_str());
                hasErrors = true;
            }
        }
    }

    if (hasErrors) {
        String errorMsg;
        serializeJson(errors, errorMsg);
        return request->reply(400, "application/json", errorMsg.c_str());
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