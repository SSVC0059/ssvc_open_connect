#include "CommandHandler.h"

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
        request->send(200, "text/plain", "Command accepted");
    } catch (const std::exception& e) {
        ESP_LOGE("CommandHandler", "Command error: %s", e.what());
        request->send(500, "text/plain", "Command execution failed");
    }
}