//
// Created by demoncat on 05.05.25.
//

#include "TelegramBotHandler.h"

#include "core/subsystem/SubsystemManager.h"
#include "external/telegramm/TelegramBotClient.h"

TelegramBotHandler::TelegramBotHandler()
{
}

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

esp_err_t TelegramBotHandler::setToken(PsychicRequest* request)
{
    ESP_LOGD(TAG, "Entering setToken handler");
    PsychicJsonResponse response(request, false);
    const JsonObject root = response.getRoot();
    JsonDocument jsonBuffer;
    const DeserializationError error = deserializeJson(jsonBuffer, request->body());

    if (error) {
        ESP_LOGE("CommandHandler", "JSON parse error: %s", error.c_str());
        return request->reply(400, "text/plain", "Invalid JSON format");
    }

    if (!jsonBuffer["token"].is<std::string>()) {
        return request->reply(400, "text/plain", "Missing or invalid 'token' field");
    }

    const String token = jsonBuffer["token"].as<String>();
    ESP_LOGD(TAG, "Received token: %s", token);

    const bool result = TelegramBotClient::bot().setBotToken(token);
    if (!result) {
        ESP_LOGD(TAG, "Failed to set bot token");
        root["result"] = "failed";
        response.setCode(400);
    } else {
        ESP_LOGD(TAG, "Successfully set bot token");
        root["result"] = "true";
        response.setCode(200);
    }
    return response.send();
}

esp_err_t TelegramBotHandler::setChatId(PsychicRequest* request)
{
    ESP_LOGD(TAG, "Entering setToken handler");
    PsychicJsonResponse response(request, false);
    const JsonObject root = response.getRoot();
    JsonDocument jsonBuffer;
    const DeserializationError error = deserializeJson(jsonBuffer, request->body());

    if (error) {
        ESP_LOGE("CommandHandler", "JSON parse error: %s", error.c_str());
        return request->reply(400, "text/plain", "Invalid JSON format");
    }

    if (!jsonBuffer["chat_id"].is<std::string>()) {
        return request->reply(400, "text/plain", "Missing or invalid 'chat_id' field");
    }

    const int64_t token = jsonBuffer["chat_id"].as<int64_t>();
    ESP_LOGD(TAG, "Received chat_id: %d", token);

    TelegramBotClient::bot().setChatID(token);
    ESP_LOGD(TAG, "Successfully set bot chat_id");
    root["result"] = "true";
    response.setCode(200);
    return response.send();
}

esp_err_t TelegramBotHandler::getToken(PsychicRequest* request)
{
    auto response = PsychicJsonResponse(request, false);
    auto root = response.getRoot();
    response.setCode(200);
    String token = TelegramBotClient::bot().getBotToken();
    root["token"] = token;
    return response.send();
}

esp_err_t TelegramBotHandler::getChatId(PsychicRequest* request)
{
    auto response = PsychicJsonResponse(request, false);
    auto root = response.getRoot();
    response.setCode(200);
    int64_t chat_id = TelegramBotClient::bot().getChatId();
    root["chat_id"] = chat_id;
    return response.send();
}

bool TelegramBotHandler::checkSubsystemEnabled()
{
    return true;
}
