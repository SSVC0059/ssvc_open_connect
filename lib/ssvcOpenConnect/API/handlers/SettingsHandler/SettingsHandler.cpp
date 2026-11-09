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

void SettingsHandler::getSettings(AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    const auto ssvcSettings = root["ssvcSettings"].to<JsonVariant>();
    SsvcSettings::init().fillSettings(ssvcSettings);
    response->setLength();
    request->send(response);
}

void SettingsHandler::updateSettings(AsyncWebServerRequest* request, JsonVariant& json)
{
    ESP_LOGD(TAG, "Received updateSettings request.");
    JsonDocument jsonBuffer;
    bool hasErrors = false;
    auto errors = jsonBuffer.to<JsonObject>();

    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", "{\"error\":\"Body must be a JSON object\"}");
        return;
    }

    // Builder живёт в рамках одного запроса и сам ведёт список полей, которые
    // очередь отбросила по версии API устройства: общего накопителя нет, поэтому
    // параллельные MQTT/профильные операции не подмешиваются в отчёт.
    SsvcSettings::Builder builder;
    // Пакетный режим: SET копятся локально и уходят на контроллер только после
    // проверки всех полей. Иначе ошибка в середине тела возвращала клиенту 400,
    // хотя часть настроек уже была применена.
    builder.beginBatch();

    JsonDocument ignoredBuffer;
    JsonArray ignored = ignoredBuffer.to<JsonArray>();
    size_t accepted = 0;

    ESP_LOGD(TAG, "Request has JSON body.");
    JsonObject bodyObj = json.as<JsonObject>();
    for (JsonPair kv : bodyObj) {
        const String& key = kv.key().c_str();
        JsonVariant value = kv.value();
        ESP_LOGV("HTTP", "Processing body parameter: %s", key.c_str());
        ESP_LOGV(TAG, "Processing parameter: '%s'", key.c_str());

        auto it = PARAM_HANDLERS.find(key);
        if (it == PARAM_HANDLERS.end()) {
            // Ключи, которых прошивка не знает (локальные настройки
            // OpenConnect в полном объекте настроек), не отменяют весь запрос:
            // остальные поля валидны. Клиент получает их список в "ignored" и
            // видит расхождение с контроллером, но не теряет всю запись.
            ignored.add(key);
            continue;
        }

        if (!it->second->handle(builder, value)) {
            ESP_LOGE(TAG, "Invalid value for parameter: '%s'", key.c_str());
            errors[key] = "Invalid value";
            hasErrors = true;
            continue;
        }
        accepted++;
    }

    if (hasErrors) {
        String errorMsg;
        serializeJson(errors, errorMsg);
        request->send(400, "application/json", errorMsg.c_str());
        return;
    }

    // Только теперь, когда все поля прошли проверку, отправляем накопленные
    // SET на контроллер. applySettings() возвращает false, если хотя бы один
    // пакет не встал в очередь: часть настроек могла уйти, и запись нельзя
    // считать применённой — клиент должен получить отказ и не запускать процесс.
    const bool queued = builder.applySettings();

    // Поля, которых нет на устройстве этой версии API, и пакеты, не попавшие в
    // очередь: копии, чтобы сериализация не зависела от состояния Builder.
    const std::vector<std::string> skipped = builder.skippedCommands();
    const std::vector<std::string> failed = builder.failedCommands();

    auto fillReport = [&ignored, &skipped](JsonDocument& doc) {
        if (!ignored.isNull() && ignored.size() > 0) {
            JsonArray ignoredOut = doc["ignored"].to<JsonArray>();
            for (JsonVariant item : ignored) {
                ignoredOut.add(item.as<const char*>());
            }
        }
        if (!skipped.empty()) {
            JsonArray skippedArray = doc["skipped"].to<JsonArray>();
            for (const auto& item : skipped) {
                skippedArray.add(item);
            }
        }
    };

    if (!queued) {
        JsonDocument errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = "Settings queue is unavailable";
        errorResponse["message"] = "Часть настроек не попала в очередь контроллера";
        // Часть SET уже в очереди — запись частичная, повторять её целиком нельзя.
        errorResponse["partial"] = true;
        errorResponse["accepted"] = accepted;
        errorResponse["failedChunks"] = static_cast<uint32_t>(failed.size());
        fillReport(errorResponse);
        JsonArray failedArray = errorResponse["failed"].to<JsonArray>();
        for (const auto& item : failed) {
            failedArray.add(item);
        }
        String errorMsg;
        serializeJson(errorResponse, errorMsg);
        ESP_LOGE(TAG, "Failed to queue %u SET chunk(s) for the controller",
                 static_cast<unsigned>(failed.size()));
        request->send(503, "application/json", errorMsg.c_str());
        return;
    }

    // Настройки ушли в контроллер — запрашиваем актуальное состояние.
    SsvcCommandsQueue::getQueue().getSettings();

    JsonDocument successResponse;
    successResponse["success"] = true;
    successResponse["message"] = "Settings updated successfully";
    successResponse["accepted"] = accepted;
    fillReport(successResponse);
    String successMsg;
    serializeJson(successResponse, successMsg);
    request->send(200, "application/json", successMsg.c_str());
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