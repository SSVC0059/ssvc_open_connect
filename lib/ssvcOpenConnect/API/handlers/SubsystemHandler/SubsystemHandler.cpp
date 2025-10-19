//
// Created by demoncat on 07.05.25.
//

#include "SubsystemHandler.h"


SubsystemHandler::SubsystemHandler() = default;

esp_err_t SubsystemHandler::getStatus(PsychicRequest* request)
{
    PsychicJsonResponse response(request, false);
    JsonObject root = response.getRoot();
    // JsonArray _subsystems = root["subsystem"].to<JsonArray>();
    const std::unordered_map<std::string, bool> subsystems = SubsystemManager::instance().getSubsystemsStatus();

    for (const auto& pair : subsystems) {
        const auto& fst = pair.first;
        const auto& snd = pair.second;
        root[fst] = snd;
    }

    return response.send();
}

esp_err_t SubsystemHandler::state(PsychicRequest* request) {
    JsonDocument doc;
    auto response = PsychicJsonResponse(request, false);

    if (deserializeJson(doc, request->body()) != DeserializationError::Ok) {
        ESP_LOGE("SubsystemHandler", "Invalid JSON received");
        return request->reply(400);
    }

    JsonObject root = response.getRoot();
    bool allSuccess = true;
    auto errors = root["errors"].to<JsonArray>();

    for (JsonPair kv : doc.as<JsonObject>()) {
        const std::string subsystem = kv.key().c_str();
        bool enable = kv.value().as<bool>();

        ESP_LOGI("SubsystemHandler", "Processing subsystem %s: %s",
                subsystem.c_str(), enable ? "enable" : "disable");

        bool result = enable
            ? SubsystemManager::instance().enableSubsystem(subsystem)
            : SubsystemManager::instance().disableSubsystem(subsystem);

        if (!result) {
            allSuccess = false;
            auto error = errors.add<JsonObject>(); // Замена createNestedObject
            error["subsystem"] = subsystem;
            error["message"] = enable ? "Enable failed" : "Disable failed";
            ESP_LOGW("SubsystemHandler", "Failed to change state for %s", subsystem.c_str());
        }
    }

    root["success"] = allSuccess;
    return response.send();
}

esp_err_t SubsystemHandler::disable(PsychicRequest* request)
{
    ESP_LOGI("SubsystemHandler", "Disable subsystem request received");

    auto response = PsychicJsonResponse(request, false);
    const JsonObject root = response.getRoot();

    // Проверка обязательных параметров
    if (!request->hasParam("name")) {
        ESP_LOGE("SubsystemHandler", "Missing 'name' parameter in disable request");
        root["error"] = "Missing 'name' parameter";
        return response.send();
    }

    const std::string name = request->getParam("name")->value().c_str();
    ESP_LOGD("SubsystemHandler", "Attempting to disable subsystem: %s", name.c_str());

    const bool result = SubsystemManager::instance().disableSubsystem(name);
    root["result"] = result;

    if (result) {
        ESP_LOGI("SubsystemHandler", "Successfully disabled subsystem: %s", name.c_str());
    } else {
        ESP_LOGW("SubsystemHandler", "Failed to disable subsystem: %s", name.c_str());
    }

    return response.send();
}

esp_err_t SubsystemHandler::enable(PsychicRequest* request)
{
    ESP_LOGI("SubsystemHandler", "Enable subsystem request received");

    auto response = PsychicJsonResponse(request, false);
    const JsonObject root = response.getRoot();

    // Проверка обязательных параметров
    if (!request->hasParam("name")) {
        ESP_LOGE("SubsystemHandler", "Missing 'name' parameter in enable request");
        root["error"] = "Missing 'name' parameter";
        return response.send();
    }

    const std::string name = request->getParam("name")->value().c_str();
    ESP_LOGD("SubsystemHandler", "Attempting to enable subsystem: %s", name.c_str());

    const bool result = SubsystemManager::instance().enableSubsystem(name);
    root["result"] = result;

    if (result) {
        ESP_LOGI("SubsystemHandler", "Successfully enabled subsystem: %s", name.c_str());
    } else {
        ESP_LOGW("SubsystemHandler", "Failed to enable subsystem: %s", name.c_str());
    }

    return response.send();
}