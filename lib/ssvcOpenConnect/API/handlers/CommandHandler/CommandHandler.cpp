#include "CommandHandler.h"

#include "components/subsystem/Xkc25Subsystem.h"

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

CommandHandler::CommandHandler() = default;

void CommandHandler::handleCommand(AsyncWebServerRequest* request, JsonVariant& json)
{
    if (!json.is<JsonObject>()) {
        request->send(400, "text/plain", "Invalid JSON format");
        return;
    }

    if (!json["commands"].is<std::string>()) {
        request->send(400, "text/plain", "Missing or invalid 'commands' field");
        return;
    }

    const std::string commandName = json["commands"].as<std::string>();
    ESP_LOGD("CommandHandler", "Received command: %s", commandName.c_str());

    const auto it = SsvcCommandsQueue::COMMAND_MAP.find(commandName);
    if (it == SsvcCommandsQueue::COMMAND_MAP.end()) {
        ESP_LOGW("CommandHandler", "Unknown command: %s", commandName.c_str());
        request->send(501, "text/plain", "Command not implemented");
        return;
    }

    // Предстартовая проверка XKC-25: защищает только команды, проходящие через ESP32
    // (веб-UI, MQTT, боты). Старт физической кнопкой SSVC-0059 так не перехватить.
    Xkc25StartCheck xkc25Check;
    if (commandName == "start") {
        xkc25Check = Xkc25Subsystem::checkStart();
        if (!xkc25Check.allow) {
            JsonDocument doc;
            doc["status"] = "rejected";
            doc["command"] = commandName;
            JsonArray reasons = doc["blockReasons"].to<JsonArray>();
            for (const auto& reason : xkc25Check.blockReasons) {
                reasons.add(reason);
            }
            String body;
            serializeJson(doc, body);
            ESP_LOGW("CommandHandler", "Command '%s' rejected by the XKC-25 start guard",
                     commandName.c_str());
            request->send(409, "application/json", body);
            return;
        }
    }

    try {
        std::string params;
        if (json["parameters"].is<std::string>()) {
            params = json["parameters"].as<std::string>();
        }

        // Команда сообщает, поставлена ли она в очередь: 200 отдаётся только при
        // фактическом принятии, иначе клиент не отличит отказ от выполнения.
        const bool accepted = it->second(params);
        if (!accepted) {
            const std::string reason = SsvcCommandsQueue::getQueue().lastRejectionReason();
            ESP_LOGW("CommandHandler", "Command rejected: %s (reason: %s)",
                     commandName.c_str(), reason.c_str());

            if (reason == "feature_unsupported") {
                request->send(409, "text/plain", "Command unsupported by device API");
            } else if (reason == "queue_full") {
                request->send(503, "text/plain", "Command queue is full");
            } else {
                request->send(503, "text/plain", "Command not accepted");
            }
            return;
        }

        ESP_LOGI("CommandHandler", "Command accepted: %s", commandName.c_str());

        // Неблокирующие предупреждения XKC-25 отдаются вместе с 200, чтобы UI
        // показал их до старта процесса. Отказ guard'а обработан выше (409).
        if (!xkc25Check.warnings.empty()) {
            JsonDocument doc;
            doc["status"] = "accepted";
            doc["command"] = commandName;
            JsonArray warnings = doc["warnings"].to<JsonArray>();
            for (const auto& warning : xkc25Check.warnings) {
                warnings.add(warning);
            }
            String body;
            serializeJson(doc, body);
            request->send(200, "application/json", body);
            return;
        }
        request->send(200, "text/plain", "Command accepted");
    } catch (const std::exception& e) {
        ESP_LOGE("CommandHandler", "Command error: %s", e.what());
        request->send(500, "text/plain", "Command execution failed");
    }
}