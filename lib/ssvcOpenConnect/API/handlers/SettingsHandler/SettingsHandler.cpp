//
// Created by demoncat on 05.05.25.
//

#include "SettingsHandler.h"

SettingsHandler::SettingsHandler() {}

esp_err_t SettingsHandler::getSettings(PsychicRequest* request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    root["request"] = "settings";
    JsonVariant _response = root["settings"].to<JsonVariant>();

    // Получаем настройки из системы
    SsvcSettings::init().getSettings(_response);
    response.setCode(200);

    return response.send();
}

esp_err_t SettingsHandler::updateSettings(PsychicRequest* request)
{
    String query = request->query();
    if (query.isEmpty()) {
        ESP_LOGV("", "No parameters received");
        return request->reply(400, "text/plain", "No parameters received");
    }

    JsonDocument jsonBuffer;
    std::vector<std::pair<String, String>> params;
    parseQueryParams(query, params);

    bool hasErrors = false;
    JsonObject errors = jsonBuffer.to<JsonObject>();
    SsvcSettings::Builder builder;

    // Обрабатываем каждый параметр
    for (const auto& param : params) {
        const String& key = param.first;
        const String& valueStr = param.second;

        ESP_LOGV("HTTP", "Processing parameter: %s=%s", key.c_str(),
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

    if (hasErrors) {
        String errorMsg;
        serializeJson(errors, errorMsg);
        return request->reply(400, "text/plain", errorMsg.c_str());
    }

    // Ожидание с проверкой флага
    ESP_LOGV("HTTP", "Waiting for _cmdSetResult...");
    TickType_t startTicks = xTaskGetTickCount();
    constexpr TickType_t timeoutTicks = pdMS_TO_TICKS(10000);
    bool cmdResult = false;

    while ((xTaskGetTickCount() - startTicks) < timeoutTicks) {
        cmdResult = SsvcCommandsQueue::getQueue()._cmdSetResult;
        if (cmdResult) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGV("HTTP", "Finished waiting for _cmdSetResult.");

    if (!cmdResult) {
        return request->reply(408, "text/plain", "Timeout");
    }

    return request->reply(200, "text/plain", "OK");
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