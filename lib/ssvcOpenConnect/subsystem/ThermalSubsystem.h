//
// Created by demoncat on 03.05.25.
//

#ifndef THERMALSUBSYSTEM_H
#define THERMALSUBSYSTEM_H
#include <sensors/Thermal/ThermalSensors.h>
#include <core/subsystem/SubsystemManager.h>
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
            ThermalSensors& thermalSensors = ThermalSensors::thermalController();
            thermalSensors.begin(*_settingsService);
            // Запуск фонового чтения температур (каждую секунду)
            thermalSensors.startContinuousReading(1000);
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

    void setSettingsService(OpenConnectSettingsService& service) {
        _settingsService = &service;
    }

private:
    OpenConnectSettingsService* _settingsService = nullptr;
    bool _initialized = false;
    bool _enabled = false; // Отдельный флаг для состояния вкл/выкл
};

#endif //THERMALSUBSYSTEM_H
