#ifndef THERMALSUBSYSTEM_H
#define THERMALSUBSYSTEM_H
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

#include "components/sensors/OneWireThermalSubsystem/OneWireThermalSubsystem.h"
#include "core/AlarmMonitor/AlarmMonitor.h"

class ThermalSubsystem final : public Subsystem {
public:
    static std::string getName() { return "thermal"; }

    void initialize() override {
        if (!_initialized) {
            ESP_LOGI("ThermalSubsystem", "Initializing thermal subsystem");
            _initialized = true;
        }
    }

    void enable() override {
        if (!_initialized) {
            ESP_LOGE("ThermalSubsystem", "Subsystem not initialized!");
            return;
        }

        if (!_enabled) {
            ESP_LOGI("ThermalSubsystem", "Enabling thermal subsystem");
            _enabled = true;
        }
    }

    void disable() override {
        if (_enabled) {
            ESP_LOGI("ThermalSubsystem", "Disabling thermal subsystem");
            // Остановка операций, но сохранение состояния
            _enabled = false;
        }
    }

    void setSettingsService(SsvcMqttSettingsService& service) {
        _ssvcMqttSettingsService = &service;
    }

private:
    SsvcMqttSettingsService* _ssvcMqttSettingsService = nullptr;
    bool _initialized = false;
    bool _enabled = false; // Отдельный флаг для состояния вкл/выкл
};

#endif //THERMALSUBSYSTEM_H
