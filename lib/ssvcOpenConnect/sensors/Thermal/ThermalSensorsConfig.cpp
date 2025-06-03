#include "ThermalSensorsConfig.h"

#include "ThermalSensorZone.h"

ThermalSensorsConfig::ThermalSensorsConfig() : _service(nullptr)
{
}

void ThermalSensorsConfig::begin(OpenConnectSettingsService& service)
{
  _service = &service;
}

void ThermalSensorsConfig::loadConfig(
  std::function<
    void(const std::unordered_map<std::string, ThermalSensorZone>&)>
  callback) const
{
  _service->read([this, callback](OpenConnectSettings& settings)
  {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, settings.sensorsConfig);
    if (error)
    {
      ESP_LOGE("ThermalSensorsConfig", "Failed to parse config: %s",
               error.c_str());
      return;
    }

    auto sensorsArray = doc["sensors"].as<JsonArray>();
    std::unordered_map<std::string, ThermalSensorZone> config;

    for (JsonObject sensorObj : sensorsArray)
    {
      const char* addr = sensorObj["address"];
      if (addr != nullptr)
      {
        auto zone = static_cast<ThermalSensorZone>(sensorObj["zone"].as<int>());
        config[addr] = zone;
      }
    }

    callback(config);
  });
}

void ThermalSensorsConfig::saveConfig(
  const std::unordered_map<std::string, ThermalSensorZone>& config) const
{
  _service->update(
    [this, &config](OpenConnectSettings& settings)
    {
      JsonDocument doc;
      auto sensorsArray = doc["sensors"].to<JsonArray>();

      for (const auto& entry : config)
      {
        JsonDocument sensor;
        sensor["address"] = entry.first;
        sensor["zone"] = static_cast<int>(entry.second);
        sensorsArray.add(sensor.as<JsonObject>());
      }

      settings.sensorsConfig.clear();
      serializeJson(doc, settings.sensorsConfig);
      return StateUpdateResult::CHANGED;
    },
    "sensors_update");
}

