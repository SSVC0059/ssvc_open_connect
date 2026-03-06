#ifndef SETTINGSSUBSYSTEM_H
#define SETTINGSSUBSYSTEM_H

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

#include <core/SubsystemManager/SubsystemManager.h>
class SettingsSubsystem final : public Subsystem {
public:
    static std::string getName() { return "settings"; }

    void initialize() override {
        if (!_initialized) {
            ESP_LOGI(TAG, "Initializing %s subsystem", TAG);
            _initialized = true;
        }
    }

    void enable() override {
        if (!_initialized) {
            ESP_LOGE(TAG, "Subsystem %s not initialized!" , TAG);
            return;
        }

        if (!_enabled) {
            ESP_LOGI(TAG, "Enabling %s subsystem" , TAG);
            _enabled = true;
        }
    }

    void disable() override {
        if (_enabled) {
            ESP_LOGI(TAG, "Disabling %s subsystem is not support" , TAG);
        } else
        {
            _enabled = true;
        }
    }

    void setSettingsService(SsvcMqttSettingsService& service) {
        _ssvcMqttSettingsService = &service;
    }

private:
    SsvcMqttSettingsService* _ssvcMqttSettingsService = nullptr;
    bool _initialized = false;
    bool _enabled = false; // Отдельный флаг для состояния вкл/выкл

    static constexpr auto TAG = "SettingsSubsystem";
};

#endif //SETTINGSSUBSYSTEM_H
