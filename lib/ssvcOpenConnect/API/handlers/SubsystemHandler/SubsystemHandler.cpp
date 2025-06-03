//
// Created by demoncat on 07.05.25.
//

#include "SubsystemHandler.h"


SubsystemHandler::SubsystemHandler() {
    // инициализация при необходимости
}

esp_err_t SubsystemHandler::getStatus(PsychicRequest* request)
{
    PsychicJsonResponse response(request, false);
    JsonObject root = response.getRoot();
    JsonArray _subsystems = root["subsystem"].to<JsonArray>();
    const std::unordered_map<std::string, bool> subsystems = SubsystemManager::instance().getSubsystemsStatus();

    for (const auto& pair : subsystems) {
        auto subsystem = _subsystems.add<JsonObject>();
        subsystem[pair.first] = pair.second;;
    }

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