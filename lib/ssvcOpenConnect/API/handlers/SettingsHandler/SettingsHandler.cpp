//
// Created by demoncat on 05.05.25.
//

#include "SettingsHandler.h"

SettingsHandler::SettingsHandler() = default;

esp_err_t SettingsHandler::getSettings(PsychicRequest* request)
{
    auto response = PsychicJsonResponse(request, false);
    const JsonObject root = response.getRoot();
    root["request"] = "settings";
    const JsonVariant _response = root["settings"].to<JsonVariant>();

    // Получаем настройки из системы
    SsvcSettings::init().fillSettings(_response);
    response.setCode(200);

    return response.send();
}

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