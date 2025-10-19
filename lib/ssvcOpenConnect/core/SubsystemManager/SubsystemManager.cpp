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

namespace {
    auto serializeSubsystem = [](const JsonObject& obj, const std::string& name, const bool& enabled) {
        obj["name"] = name;
        obj["enabled"] = enabled;
    };

    auto deserializeSubsystem = [](JsonObject& obj, std::string& name, bool& enabled) -> bool {
        const char* subsystemName = obj["name"];
        if (subsystemName == nullptr) return false;

        name = subsystemName;
        enabled = obj["enabled"].as<bool>();
        return true;
    };
}

// Внешнее объявление для template parameter
extern const char subsystemsConfigName[];
const char subsystemsConfigName[] = "subsystems";

SubsystemManager& SubsystemManager::instance() {
    static SubsystemManager _instance;
    ESP_LOGV(TAG, "[INSTANCE] Returning instance");
    return _instance;
}

SubsystemManager::SubsystemManager() {
    ESP_LOGV(TAG, "[CONSTRUCTOR] SubsystemManager created");
}

void SubsystemManager::begin() {

    if (_initialized) {
        ESP_LOGW(TAG, "[BEGIN] Already initialized!");
        return;
    }

    // 1. Сначала загружаем конфигурацию (но не учитываем enabled из неё)
    loadConfig();

    if (_subsystemFactories.empty()) {
        ESP_LOGE(TAG, "[BEGIN] No subsystems registered!");
        return;
    }

    ESP_LOGV(TAG, "[BEGIN] Initializing %zu subsystems", _subsystemFactories.size());

    // 2. Инициализируем подсистемы
    for (const auto& factory_pair : _subsystemFactories) {
        const std::string& name = factory_pair.first;
        const auto& factory = factory_pair.second;

        // Определяем нужно ли включать подсистему:
        bool shouldEnable = false; // Значение по умолчанию
        if (_initialStates.count(name)) {
            shouldEnable = _initialStates.at(name);
        }

        // Если подсистема есть в конфиге, используем её значение
        auto subsystemIt = _subsystems.find(name);
        if (subsystemIt != _subsystems.end()) {
            shouldEnable = subsystemIt->second.enabled;
        }

        ESP_LOGV(TAG, "[BEGIN] %s - enabled: %d", name.c_str(), shouldEnable);
        if (shouldEnable) {
            // 3. Гарантированно создаём экземпляр
            if (_subsystems[name].instance == nullptr) {
                _subsystems[name].instance = factory();
                _subsystems[name].instance->initialize();
                _subsystems[name].instance->enable();
            }

            // 4. Включаем подсистему
            if (!enableSubsystem(name)) {
                ESP_LOGE(TAG, "[BEGIN] Failed to enable %s", name.c_str());
                _subsystems.erase(name); // Удаляем некорректную подсистему
            }
        }
    }

    _initialized = true;
    ESP_LOGV(TAG, "[BEGIN] Initialization complete");
}

void SubsystemManager::setInitialState(const std::string& name, const bool enabled) {
    ESP_LOGV(TAG, "[SET_INITIAL] Subsystem '%s' initial state set to %d", name.c_str(), enabled);
    _initialStates[name] = enabled;
}

bool SubsystemManager::enableSubsystem(const std::string& name) {
    // 1. Проверяем регистрацию подсистемы
    if (!_subsystemFactories.count(name)) {
        ESP_LOGE(TAG, "[ENABLE] Subsystem '%s' not registered", name.c_str());
        return false;
    }

    // 2. Проверяем текущее состояние
    const auto it = _subsystems.find(name);
    if (it != _subsystems.end() && it->second.enabled) {
        ESP_LOGV(TAG, "[ENABLE] Subsystem '%s' already enabled", name.c_str());
        return true;
    }

    ESP_LOGV(TAG, "[ENABLE] Enabling subsystem '%s'", name.c_str());

    try {
        // 3. Создаем или получаем экземпляр
        std::shared_ptr<Subsystem> subsystem;

        if (it != _subsystems.end() && it->second.instance) {
            subsystem = it->second.instance;
            ESP_LOGV(TAG, "[ENABLE] Using existing instance of '%s' at %p",
                    name.c_str(), subsystem.get());
        } else {
            subsystem = _subsystemFactories[name]();
            ESP_LOGV(TAG, "[ENABLE] Created new instance of '%s' at %p",
                    name.c_str(), subsystem.get());

            // subsystem->initialize();
            ESP_LOGV(TAG, "[ENABLE] Subsystem '%s' initialized", name.c_str());
        }

        // 4. Включаем подсистему
        subsystem->enable();
        _subsystems[name] = {true, subsystem};
        ESP_LOGV(TAG, "[ENABLE] Subsystem '%s' enabled and saved", name.c_str());

        // 5. Сохраняем конфигурацию
        saveConfig();
        return true;

    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "[ENABLE] Exception enabling subsystem '%s': %s",
                name.c_str(), e.what());
        return false;
    }
}

bool SubsystemManager::disableSubsystem(const std::string& name) {
    if (!_subsystems.count(name)) {
        ESP_LOGW(TAG, "[DISABLE] Subsystem '%s' not found", name.c_str());
        return true;
    }

    if (!_subsystems[name].enabled) {
        ESP_LOGW(TAG, "[DISABLE] Subsystem '%s' already disabled", name.c_str());
        return true;
    }

    ESP_LOGV(TAG, "[DISABLE] Disabling subsystem '%s'", name.c_str());

    try {
        // Отключаем подсистему
        _subsystems[name].instance->disable();
        ESP_LOGV(TAG, "[DISABLE] Subsystem '%s' disabled", name.c_str());

        _subsystems[name].enabled = false;
        ESP_LOGV(TAG, "[DISABLE] Subsystem '%s' state updated", name.c_str());

        // _subsystems.erase(name);
        // Сохраняем конфигурацию
        saveConfig();
        ESP_LOGV(TAG, "[DISABLE] Configuration saved for '%s'", name.c_str());

        return true;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "[DISABLE] Exception disabling subsystem '%s': %s", name.c_str(), e.what());
        return false;
    }
}

bool SubsystemManager::exists(const std::string& name) const
{
    if (!_subsystems.count(name)) {
        ESP_LOGW(TAG, "[DISABLE] Subsystem '%s' not found", name.c_str());
        return false;
    }
    return true;
}

void SubsystemManager::loadConfig() {
    ESP_LOGV(TAG, "[LOAD_CONFIG] Starting configuration load process");

    // 1. Пытаемся загрузить конфиг из хранилища
    std::unordered_map<std::string, bool> loadedConfig;
    if (GlobalConfig::config().getObject(
            "subsystem", "config", loadedConfig, GlobalConfig::fromJson)) {

        ESP_LOGV(TAG, "[LOAD_CONFIG] Loaded %zu subsystem configurations", loadedConfig.size());

        // 2. Для каждой зарегистрированной фабрики создаем запись в _subsystems
        for (const auto& factory_pair : _subsystemFactories) {
            const std::string& name = factory_pair.first;

            // 3. Проверяем, есть ли настройки для этой подсистемы
            auto configIt = loadedConfig.find(name);
            bool enabled = _initialStates[name]; // Значение по умолчанию

            if (configIt != loadedConfig.end()) {
                enabled = configIt->second;
                ESP_LOGV(TAG, "[LOAD_CONFIG] Found config for '%s': %d", name.c_str(), enabled);
            } else {
                ESP_LOGV(TAG, "[LOAD_CONFIG] Using default state for '%s': %d", name.c_str(), enabled);
            }

            // 4. Создаем запись (экземпляр пока nullptr)
            _subsystems[name] = {enabled, nullptr};
        }

#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_WARN
        // 5. Логируем нераспознанные записи
        for (const auto& config_pair : loadedConfig) {
            if (_subsystemFactories.count(config_pair.first) == 0) {
                ESP_LOGW(TAG, "[LOAD_CONFIG] Unknown subsystem in config: '%s'", config_pair.first.c_str());
            }
        }   
#endif
        

    } else {
        ESP_LOGW(TAG, "[LOAD_CONFIG] No config found, using default states");

        // 6. Если конфига нет, инициализируем все подсистемы значениями по умолчанию
        for (const auto& factory_pair : _subsystemFactories) {
            const std::string& name = factory_pair.first;
            _subsystems[name] = {_initialStates[name], nullptr};
        }
    }
}

void SubsystemManager::saveConfig() const {
    ESP_LOGV(TAG, "[SAVE_CONFIG] Starting configuration save process");
    ESP_LOGV(TAG, "[SAVE_CONFIG] Current subsystems state:");

#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_VERBOSE
    for (const auto& subsystem_pair : _subsystems) {
        ESP_LOGV(TAG, "  - %s: %s", subsystem_pair.first.c_str(), subsystem_pair.second.enabled ? "ENABLED" : "DISABLED");
    }
#endif

    std::unordered_map<std::string, bool> currentConfig;
    ESP_LOGV(TAG, "[SAVE_CONFIG] Preparing configuration data for saving");

    for (const auto& subsystem_pair : _subsystems) {
        currentConfig[subsystem_pair.first] = subsystem_pair.second.enabled;
    }

    ESP_LOGV(TAG, "[SAVE_CONFIG] Attempting to save %zu subsystem states", currentConfig.size());

    try {
        GlobalConfig::config().setObject("subsystem", "config", currentConfig, GlobalConfig::toJson);
        ESP_LOGV(TAG, "[SAVE_CONFIG] Configuration successfully saved");
        ESP_LOGV(TAG, "[SAVE_CONFIG] Saved configuration contents:");
        for (const auto& config_pair : currentConfig) {
            ESP_LOGV(TAG, "  - %s: %s", config_pair.first.c_str(), config_pair.second ? "true" : "false");
        }
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "[SAVE_CONFIG] ERROR: Failed to save configuration - %s", e.what());
    }

    ESP_LOGV(TAG, "[SAVE_CONFIG] Configuration save process completed");
}

std::shared_ptr<Subsystem> SubsystemManager::getSubsystem(const std::string& name) const {
    ESP_LOGV(TAG, "[GET_SUBSYS] Looking for subsystem: '%s'", name.c_str());

    const auto it = _subsystems.find(name);
    if (it != _subsystems.end()) {
        if (it->second.enabled) {
            ESP_LOGV(TAG, "[GET_SUBSYS] Found enabled: '%s' at %p",
                    name.c_str(), it->second.instance.get());
            return it->second.instance;
        }
        ESP_LOGW(TAG, "[GET_SUBSYS] '%s' exists but disabled", name.c_str());
    } else {
        ESP_LOGW(TAG, "[GET_SUBSYS] '%s' not registered", name.c_str());
    }
    return nullptr;
}

bool SubsystemManager::isSubsystemRegistered(const std::string& name) const {
    return _subsystemFactories.count(name) > 0;
}

std::vector<std::string> SubsystemManager::getAvailableSubsystems() const {
    ESP_LOGV(TAG, "[GET_AVAILABLE] Getting list of all available subsystems");
    std::vector<std::string> subsystems;
    subsystems.reserve(_subsystemFactories.size());

    for (const auto& factory_pair : _subsystemFactories) {
        subsystems.push_back(factory_pair.first);
        ESP_LOGV(TAG, "[GET_AVAILABLE] Found subsystem: %s", factory_pair.first.c_str());
    }

    ESP_LOGV(TAG, "[GET_AVAILABLE] Total available subsystems: %zu", subsystems.size());
    return subsystems;
}

std::vector<std::string> SubsystemManager::getEnabledSubsystems() const {
    ESP_LOGV(TAG, "[GET_ENABLED] Getting list of enabled subsystems");
    std::vector<std::string> enabledSubsystems;

    for (const auto& subsystem_pair : _subsystems) {
        if (subsystem_pair.second.enabled) {
            enabledSubsystems.push_back(subsystem_pair.first);
            ESP_LOGV(TAG, "[GET_ENABLED] Found enabled subsystem: %s", subsystem_pair.first.c_str());
        }
    }

    ESP_LOGV(TAG, "[GET_ENABLED] Total enabled subsystems: %zu", enabledSubsystems.size());
    return enabledSubsystems;
}

bool SubsystemManager::isSubsystemEnabled(const std::string& name) const {
    ESP_LOGV(TAG, "[IS_ENABLED] Checking if subsystem '%s' is enabled", name.c_str());

    const auto it = _subsystems.find(name);
    if (it != _subsystems.end()) {
        const bool enabled = it->second.enabled;
        ESP_LOGV(TAG, "[IS_ENABLED] Subsystem '%s' is %s",
                name.c_str(), enabled ? "ENABLED" : "DISABLED");
        return enabled;
    }

    ESP_LOGW(TAG, "[IS_ENABLED] Subsystem '%s' not found", name.c_str());
    return false;
}

std::unordered_map<std::string, bool> SubsystemManager::getSubsystemsStatus() const
{
    ESP_LOGV(TAG, "[GET_AVAILABLE] Getting list of available subsystems");
    std::unordered_map<std::string, bool> enabledSubsystems;

    for (const auto& subsystem_pair : _subsystems) {
        enabledSubsystems[subsystem_pair.first] = subsystem_pair.second.enabled;
    }

    ESP_LOGV(TAG, "[GET_ENABLED] Total enabled subsystems: %zu", enabledSubsystems.size());
    return enabledSubsystems;
}