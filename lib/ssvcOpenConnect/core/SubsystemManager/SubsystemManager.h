#ifndef SUBSYSTEM_MANAGER_H
#define SUBSYSTEM_MANAGER_H

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

#include <ArduinoJson.h>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <core/GlobalConfig/GlobalConfig.h>
#include <EventSocket.h>


class Subsystem {
public:
    virtual ~Subsystem() = default;
    virtual void initialize() = 0;
    virtual void enable() = 0;
    virtual void disable() = 0;
};

// Структура для хранения информации о подсистеме
struct SubsystemInfo {
    bool enabled;
    std::shared_ptr<Subsystem> instance;
};


class SubsystemManager {
public:
    using SubsystemFactory = std::function<std::shared_ptr<Subsystem>()>;
    
    static SubsystemManager& instance();
    std::shared_ptr<Subsystem> getSubsystem(const std::string& name) const;

    void begin();
    
    template<typename T>
    void registerSubsystem() {
        static_assert(std::is_base_of<Subsystem, T>::value,
                     "T must inherit from Subsystem");

        const std::string name = T::getName();
        ESP_LOGD(TAG, "[REGISTER] Registering subsystem: %s", name.c_str());

        if (_subsystemFactories.count(name)) {
            ESP_LOGW(TAG, "[REGISTER] %s already registered!", name.c_str());
            return;
        }

        _subsystemFactories[name] = []() {
            ESP_LOGD(TAG, "[CREATE] Creating instance of %s", T::getName().c_str());
            return std::make_shared<T>();
        };

        ESP_LOGI(TAG, "[REGISTER] %s registered successfully", name.c_str());
    }
    
    void setInitialState(const std::string& name, bool enabled);
    
    bool enableSubsystem(const std::string& name);
    bool disableSubsystem(const std::string& name);
    bool exists(const std::string& name) const;
    
    std::vector<std::string> getAvailableSubsystems() const;
    std::vector<std::string> getEnabledSubsystems() const;
    std::unordered_map<std::string, bool> getSubsystemsStatus() const;
    bool isSubsystemEnabled(const std::string& name) const;

    void setEventSocket(EventSocket* socket);


private:
    SubsystemManager();
    void loadConfig();
    void saveConfig() const;
    bool isSubsystemRegistered(const std::string& name) const;
    void onWiFiConnected(const JsonObject& payload);

    std::unordered_map<std::string, SubsystemFactory> _subsystemFactories;
    std::unordered_map<std::string, bool> _initialStates;
    std::unordered_map<std::string, SubsystemInfo> _subsystems;
    EventSocket* _socket = nullptr;


    bool _initialized = false;
    
    static SubsystemManager* _instance;
    static constexpr auto TAG = "SubsystemManager";

};

#endif // SUBSYSTEM_MANAGER_H