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

#include "SubsystemManager.h"

SubsystemManager* SubsystemManager::_instance = nullptr;

SubsystemManager& SubsystemManager::instance() {
    static SubsystemManager _instance;
    return _instance;
}

SubsystemManager::SubsystemManager() = default;

void SubsystemManager::begin() {
    if (_initialized) {
        ESP_LOGW(TAG, "[BEGIN] Already initialized!");
        return;
    }
    if (_subsystems_info.empty()) {
        ESP_LOGE(TAG, "[BEGIN] No subsystems registered!");
        return;
    }

    // 1. Загружаем сохраненные состояния из NVS.
    std::unordered_map<std::string, bool> savedStates;
    GlobalConfig::config().getObject("subsystem", "config", savedStates, GlobalConfig::fromJson);

    // 2. Инициализируем все подсистемы и определяем их целевое состояние.
    for (auto& info : _subsystems_info) {
        // Приоритет у _initialStates.
        if (_initialStates.count(info.name)) {
            info.enabled = _initialStates.at(info.name);
        } else if (savedStates.count(info.name)) {
            info.enabled = savedStates.at(info.name);
        } else {
            info.enabled = false; // По умолчанию выключено
        }

        info.instance = info.factory();
        info.instance->initialize();
        ESP_LOGD(TAG, "[BEGIN] Initialized '%s', target state: %s", info.name.c_str(), info.enabled ? "ENABLED" : "DISABLED");
    }

    // 3. Проходим по подсистемам в порядке регистрации и пытаемся их включить.
    for (auto& info : _subsystems_info) {
        if (info.enabled) {
            ESP_LOGD(TAG, "[BEGIN] Attempting to enable '%s'", info.name.c_str());
            info.instance->enable();
        }
    }

    // 4. Финальное сохранение итогового, фактического состояния.
    ESP_LOGI(TAG, "[BEGIN] Subsystem initialization pass complete. Saving final states.");
    saveConfig();

    _initialized = true;
    ESP_LOGI(TAG, "[BEGIN] Subsystem manager initialization finished.");
}

void SubsystemManager::setInitialState(const std::string& name, bool enabled) {
    _initialStates[name] = enabled;
}

bool SubsystemManager::enableSubsystem(const std::string& name) {
    auto it = std::find_if(_subsystems_info.begin(), _subsystems_info.end(),
        [&name](const SubsystemInfo& info){ return info.name == name; });

    if (it == _subsystems_info.end()) {
        ESP_LOGE(TAG, "[ENABLE] Subsystem '%s' not registered", name.c_str());
        return false;
    }

    if (it->enabled) {
        return true;
    }

    ESP_LOGD(TAG, "[ENABLE] Enabling subsystem '%s'", name.c_str());

    it->enabled = true;
    it->instance->enable();

    if (it->enabled) {
        saveConfig();
    }

    return true;
}

bool SubsystemManager::disableSubsystem(const std::string& name) {
    auto it = std::find_if(_subsystems_info.begin(), _subsystems_info.end(),
        [&name](const SubsystemInfo& info){ return info.name == name; });

    if (it == _subsystems_info.end() || !it->enabled) {
        return true;
    }

    ESP_LOGD(TAG, "[DISABLE] Disabling subsystem '%s'", name.c_str());

    it->instance->disable();
    it->enabled = false;
    saveConfig();

    return true;
}

std::shared_ptr<Subsystem> SubsystemManager::getSubsystem(const std::string& name) const {
    auto it = std::find_if(_subsystems_info.cbegin(), _subsystems_info.cend(),
        [&name](const SubsystemInfo& info){ return info.name == name; });

    if (it != _subsystems_info.cend() && it->enabled) {
        return it->instance;
    }
    return nullptr;
}

bool SubsystemManager::isSubsystemEnabled(const std::string& name) const {
    auto it = std::find_if(_subsystems_info.cbegin(), _subsystems_info.cend(),
        [&name](const SubsystemInfo& info){ return info.name == name; });

    if (it != _subsystems_info.cend()) {
        return it->enabled;
    }
    return false;
}

void SubsystemManager::saveConfig() const {
    std::unordered_map<std::string, bool> currentConfig;
    for (const auto& info : _subsystems_info) {
        currentConfig[info.name] = info.enabled;
    }
    GlobalConfig::config().setObject("subsystem", "config", currentConfig, GlobalConfig::toJson);
}

std::unordered_map<std::string, bool> SubsystemManager::getSubsystemsStatus() const {
    std::unordered_map<std::string, bool> statusMap;
    for (const auto& info : _subsystems_info) {
        statusMap[info.name] = info.enabled;
    }
    return statusMap;
}

bool SubsystemManager::isSubsystemRegistered(const std::string& name) const {
    auto it = std::find_if(_subsystems_info.cbegin(), _subsystems_info.cend(),
        [&name](const SubsystemInfo& info){ return info.name == name; });
    return it != _subsystems_info.cend();
}