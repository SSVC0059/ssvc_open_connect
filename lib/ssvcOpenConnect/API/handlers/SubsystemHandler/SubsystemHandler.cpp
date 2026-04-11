#include "SubsystemHandler.h"

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

SubsystemHandler::SubsystemHandler() = default;

void SubsystemHandler::getStatus(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    const std::unordered_map<std::string, bool> subsystems = SubsystemManager::instance().getSubsystemsStatus();

    for (const auto& pair : subsystems) {
        const auto& fst = pair.first;
        const auto& snd = pair.second;
        root[fst.c_str()] = snd;
    }

    response->setLength();
    request->send(response);
}

void SubsystemHandler::state(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        ESP_LOGE("SubsystemHandler", "Invalid JSON received");
        request->send(400);
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    bool allSuccess = true;
    auto errors = root["errors"].to<JsonArray>();

    for (JsonPair kv : json.as<JsonObject>()) {
        const std::string subsystem = kv.key().c_str();
        bool enable = kv.value().as<bool>();

        ESP_LOGI("SubsystemHandler", "Processing subsystem %s: %s",
                subsystem.c_str(), enable ? "enable" : "disable");

        bool result = enable
            ? SubsystemManager::instance().enableSubsystem(subsystem)
            : SubsystemManager::instance().disableSubsystem(subsystem);

        if (!result) {
            allSuccess = false;
            auto error = errors.add<JsonObject>();
            error["subsystem"] = subsystem.c_str();
            error["message"] = enable ? "Enable failed" : "Disable failed";
            ESP_LOGW("SubsystemHandler", "Failed to change state for %s", subsystem.c_str());
        }
    }

    root["success"] = allSuccess;
    response->setLength();
    request->send(response);
}

void SubsystemHandler::disable(AsyncWebServerRequest* request)
{
    ESP_LOGI("SubsystemHandler", "Disable subsystem request received");

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    if (!request->hasParam("name")) {
        ESP_LOGE("SubsystemHandler", "Missing 'name' parameter in disable request");
        request->send(400, "application/json", R"({"error":"Missing 'name' parameter"})");
        delete response;
        return;
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

    response->setLength();
    request->send(response);
}

void SubsystemHandler::enable(AsyncWebServerRequest* request)
{
    ESP_LOGI("SubsystemHandler", "Enable subsystem request received");

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    if (!request->hasParam("name")) {
        ESP_LOGE("SubsystemHandler", "Missing 'name' parameter in enable request");
        request->send(400, "application/json", R"({"error":"Missing 'name' parameter"})");
        delete response;
        return;
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

    response->setLength();
    request->send(response);
}