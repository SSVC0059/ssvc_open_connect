#include "TelegramBotHandler.h"

#include "core/SubsystemManager/SubsystemManager.h"
#include "external/telegramm/TelegramBotClient.h"

TelegramBotHandler::TelegramBotHandler()
= default;

void TelegramBotHandler::settings(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    ESP_LOGI(TAG, "TelegramBot::setToken");

    const std::string subsystemName = "telegram_bot";
    const bool enable = SubsystemManager::instance().isSubsystemEnabled(subsystemName);
    root["enable"] = enable;
    root["subsystem_name"] = subsystemName.c_str();
    if (enable) {
        root["status"] = "ok";
    } else {
        root["status"] = "telegram_bot disable";
        response->setLength();
        String str;
        serializeJson(root, str);
        request->send(423, "application/json", str);
        delete response;
        return;
    }
    response->setLength();
    request->send(response);
}

void TelegramBotHandler::updateSettings(AsyncWebServerRequest* request, JsonVariant& json)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    bool result = false;

    if (!json.is<JsonObject>()) {
        request->send(400, "text/plain", "Invalid JSON format");
        delete response;
        return;
    }

    if (json["token"].is<std::string>()) {
        const auto token = json["token"].as<String>();
        ESP_LOGD(TAG, "Received token: %s", token.c_str());
        TelegramBotClient::bot().setBotToken(token);
        result = true;
    }

    if (json["chat_id"].is<const char*>() || json["chat_id"].is<std::string>()) {
        const auto chat_id = json["chat_id"].as<String>();
        ESP_LOGD(TAG, "Received chat_id: %s", chat_id.c_str());
        TelegramBotClient::bot().setChatID(chat_id);
        result = true;
    } else if (!json["chat_id"].isNull()) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lld", (long long)json["chat_id"].as<int64_t>());
        TelegramBotClient::bot().setChatID(String(buf));
        result = true;
    }

    root["result"] = result;
    if (!result) {
        ESP_LOGD(TAG, "No valid settings provided");
        response->setLength();
        String str;
        serializeJson(root, str);
        request->send(400, "application/json", str);
        delete response;
        return;
    }
    ESP_LOGD(TAG, "Settings updated");
    response->setLength();
    request->send(response);
}

void TelegramBotHandler::getSettings(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    const String token = TelegramBotClient::bot().getBotToken();
    const String chat_id = TelegramBotClient::bot().getChatId();
    root["token"] = token.c_str();
    root["chat_id"] = chat_id.c_str();
    response->setLength();
    request->send(response);
}

bool TelegramBotHandler::checkSubsystemEnabled()
{
    return true;
}
