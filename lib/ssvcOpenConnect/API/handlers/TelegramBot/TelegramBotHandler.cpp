#include "TelegramBotHandler.h"

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

#include "core/SubsystemManager/SubsystemManager.h"
#include "external/telegramm/TelegramBotClient.h"

TelegramBotHandler::TelegramBotHandler()
= default;

esp_err_t TelegramBotHandler::settings(PsychicRequest* request)
{
    PsychicJsonResponse response(request, false);
    const JsonObject root = response.getRoot();

    ESP_LOGI(TAG, "TelegramBot::setToken");

    const std::string subsystemName = "telegram_bot";
    const bool enable = SubsystemManager::instance().isSubsystemEnabled(subsystemName);
    root["enable"] = enable;
    root["subsystem_name"] = subsystemName;
    if (enable) {
        root["status"] = "ok";
        response.setCode(200);
    } else {
        root["status"] = "telegram_bot disable";
        response.setCode(423);
    }
    return response.send();

}

esp_err_t TelegramBotHandler::updateSettings(PsychicRequest* request)
{

    PsychicJsonResponse response(request, false);
    const JsonObject root = response.getRoot();
    JsonDocument jsonBuffer;
    const DeserializationError error = deserializeJson(jsonBuffer, request->body());
    bool result = false;

    if (error) {
        ESP_LOGE("CommandHandler", "JSON parse error: %s", error.c_str());
        return request->reply(400, "text/plain", "Invalid JSON format");
    }

    bool result_set_token = false;
    if (jsonBuffer["token"].is<std::string>()) {
        const auto token = jsonBuffer["token"].as<String>();
        ESP_LOGD(TAG, "Received token: %s", token);
        result_set_token = TelegramBotClient::bot().setBotToken(token);
        result = result_set_token;
    }

    bool result_set_chat_id = false;
    if (jsonBuffer["chat_id"].is<std::string>()) {
        const auto chat_id = jsonBuffer["chat_id"].as<int64_t>();
        ESP_LOGD(TAG, "Received chat_id: %d", chat_id);
        TelegramBotClient::bot().setChatID(chat_id);
        result = true;
    }

    root["result"] = result;
    root["result_set_token"] = result_set_token;
    root["result"] = result_set_chat_id;
    if (!result) {
        ESP_LOGD(TAG, "Failed to set bot token");
        response.setCode(400);
    } else {
        ESP_LOGD(TAG, "Successfully set bot token");
        response.setCode(200);
    }

    return response.send();
}


esp_err_t TelegramBotHandler::getSettings(PsychicRequest* request)
{
    auto response = PsychicJsonResponse(request, false);
    const auto root = response.getRoot();
    response.setCode(200);
    const String token = TelegramBotClient::bot().getBotToken();
    const int64_t chat_id = TelegramBotClient::bot().getChatId();
    root["token"] = token;
    root["chat_id"] = chat_id;
    return response.send();
}

bool TelegramBotHandler::checkSubsystemEnabled()
{
    return true;
}
