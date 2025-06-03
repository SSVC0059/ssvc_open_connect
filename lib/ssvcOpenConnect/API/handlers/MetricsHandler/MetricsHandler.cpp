//
// Created by demoncat on 05.05.25.
//

#include "MetricsHandler.h"
#include "esp_log.h"

#define TAG "MetricsHandler"

MetricsHandler::MetricsHandler(RectificationProcess& rProcess)
    : _rProcess(rProcess) {}

esp_err_t MetricsHandler::getTelemetry(PsychicRequest* request) const {
    PsychicJsonResponse response(request, false);
    JsonObject root = response.getRoot();

    // Получаем телеметрию в виде JSON строки
    std::string telemetryJson = _rProcess.getTelemetry();
    ESP_LOGD(TAG, "Raw telemetry JSON: %s", telemetryJson.c_str());

    // Парсим JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, telemetryJson);
    if (error) {
        ESP_LOGE(TAG, "Failed to parse telemetry JSON: %s", error.c_str());
        return request->reply(500, "text/plain", "Telemetry data parsing error");
    }

    // Добавляем телеметрию в ответ
    root["telemetry"] = doc.as<JsonVariant>();

    // Обработка параметра mode
    if (request->hasParam("mode")) {
        std::string mode = request->getParam("mode")->value().c_str();
        root["request"] = mode;

        if (mode == "status") {
            JsonVariant status = root["status"].to<JsonVariant>();
            if (!_rProcess.getStatus(status)) {
                return request->reply(500, "text/plain", "Failed to get status");
            }
        }
    }

    return response.send();
}

esp_err_t MetricsHandler::getMetrics(PsychicRequest* request)
{
    PsychicJsonResponse response(request, false);
    JsonObject root = response.getRoot();

    // Параметры запроса
    int point = 0;
    int periodicity = 12;

    if (request->hasParam("point")) {
        point = request->getParam("point")->value().toInt();
    }

    if (request->hasParam("periodicity")) {
        periodicity = request->getParam("periodicity")->value().toInt();
    }

    ESP_LOGD(TAG, "Metrics request - point: %d, periodicity: %d", point, periodicity);

    // Получаем данные метрик
    JsonDocument metricsData; // Замените на реальный метод получения данных
    // metricsData = _rProcess.getMetricsData(point, periodicity);

    // Формируем ответ
    root["graphData"] = metricsData.as<JsonVariant>();
    root["point"] = point;
    root["periodicity"] = periodicity;

#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG
    String jsonOutput;
    serializeJson(root, jsonOutput);
    ESP_LOGD(TAG, "Metrics response: %s", jsonOutput.c_str());
#endif

    return response.send();
}
