#include "VkBotHandler.h"

#include <ArduinoJson.h>
#include <Features.h>

#include "core/SubsystemManager/SubsystemManager.h"
#include "core/StatefulServices/VkSettingsService/VkSettingsService.h"

#if FT_ENABLED(FT_VK_BOT)

void VkBotHandler::getSettings(AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    const std::string subsystemName = "vk_bot";
    const bool enable = SubsystemManager::instance().isSubsystemEnabled(subsystemName);
    root["enable"] = enable;
    root["subsystem_name"] = subsystemName.c_str();
    if (!enable) {
        root["status"] = "vk_bot disabled";
        response->setLength();
        String str;
        serializeJson(root, str);
        request->send(423, "application/json", str);
        delete response;
        return;
    }

    VkSettingsService* svc = VkSettingsService::getInstance();
    if (!svc) {
        request->send(500);
        delete response;
        return;
    }
    svc->read([&](const VkSettings& s) { readVkSettings(const_cast<VkSettings&>(s), root); });
    response->setLength();
    request->send(response);
}

void VkBotHandler::updateSettings(AsyncWebServerRequest* request, JsonVariant& json) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    bool result = false;

    if (!json.is<JsonObject>()) {
        request->send(400, "text/plain", "Invalid JSON format");
        delete response;
        return;
    }

    VkSettingsService* svc = VkSettingsService::getInstance();
    if (!svc) {
        request->send(500);
        delete response;
        return;
    }

    JsonObject body = json.as<JsonObject>();
    svc->update(
        [&](VkSettings& settings) { return updateVkSettings(body, settings); },
        "api");

    result = true;
    root["result"] = result;
    response->setLength();
    request->send(response);
}

#else

void VkBotHandler::getSettings(AsyncWebServerRequest* request) {
    request->send(404);
}

void VkBotHandler::updateSettings(AsyncWebServerRequest* request, JsonVariant&) {
    request->send(404);
}

#endif
