#ifndef THERMALSENSORSCONFIG_H
#define THERMALSENSORSCONFIG_H

#include <unordered_map>
#include <ArduinoJson.h>
#include "ThermalSensorZone.h"
#include <core/OpenConnectSettings/OpenConnectSettings.h>

class ThermalSensorsConfig
{
public:
    ThermalSensorsConfig();
    void begin(OpenConnectSettingsService& service);
    void loadConfig(std::function<void(const std::unordered_map<std::string, ThermalSensorZone>&)> callback) const;
    void saveConfig(const std::unordered_map<std::string, ThermalSensorZone>& config) const;

private:
    OpenConnectSettingsService* _service;
};

#endif //THERMALSENSORSCONFIG_H
