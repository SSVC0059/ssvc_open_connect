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

// Инициализация статической карты команд
const std::map<std::string, std::function<void()>> CommandHandler::COMMAND_MAP = {
    {"next",    []() { SsvcCommandsQueue::getQueue().next(); }},
    {"pause",   []() { SsvcCommandsQueue::getQueue().pause(); }},
    {"stop",    []() { SsvcCommandsQueue::getQueue().stop(); }},
    {"start",   []() { SsvcCommandsQueue::getQueue().start(); }},
    {"resume",  []() { SsvcCommandsQueue::getQueue().resume(); }},
    {"version", []() { SsvcCommandsQueue::getQueue().version(); }},
    {"settings",[]() { SsvcCommandsQueue::getQueue().getSettings(); }}
};

CommandHandler::CommandHandler() = default;

esp_err_t CommandHandler::handleCommand(PsychicRequest* request)
{
    JsonDocument jsonBuffer;
    const DeserializationError error = deserializeJson(jsonBuffer, request->body());

    if (error) {
        ESP_LOGE("CommandHandler", "JSON parse error: %s", error.c_str());
        return request->reply(400, "text/plain", "Invalid JSON format");
    }

    if (!jsonBuffer["commands"].is<std::string>()) {
        return request->reply(400, "text/plain", "Missing or invalid 'commands' field");
    }

    const std::string commandName = jsonBuffer["commands"].as<std::string>();
    ESP_LOGD("CommandHandler", "Received command: %s", commandName.c_str());

    const auto it = COMMAND_MAP.find(commandName);
    if (it == COMMAND_MAP.end()) {
        ESP_LOGW("CommandHandler", "Unknown command: %s", commandName.c_str());
        return request->reply(501, "text/plain", "Command not implemented");
    }

    try {
        // Выполняем команду
        it->second();
        ESP_LOGI("CommandHandler", "Command executed: %s", commandName.c_str());
        return request->reply(200, "text/plain", "Command accepted");
    } catch (const std::exception& e) {
        ESP_LOGE("CommandHandler", "Command error: %s", e.what());
        return request->reply(500, "text/plain", "Command execution failed");
    }
}