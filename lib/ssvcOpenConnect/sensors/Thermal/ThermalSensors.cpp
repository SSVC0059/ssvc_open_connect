#include "ThermalSensors.h"

ThermalSensors* ThermalSensors::_instance = nullptr;
const char thermalSensorsConfigName[] = "sensors";

ThermalSensors& ThermalSensors::thermalController()
{
  static ThermalSensors thermalSensors;
  return thermalSensors;

}

ThermalSensors::ThermalSensors()
{
  _dataMutex = xSemaphoreCreateMutex();
  _sensorManager.begin();
}

void ThermalSensors::begin(
  OpenConnectSettingsService& openConnectSettingsService)
{
  ESP_LOGI(TAG, "Initializing ThermalSensors...");

  const size_t changed = scanAndAddAllSensors(ThermalSensorZone::UNKNOWN);
  ESP_LOGI(TAG, "ThermalSensors initialized with changes: %d", changed);
  if (changed > 0)
  {
    ESP_LOGI(TAG, "Initialization completed. Changes detected: %zu", changed);
  }
  else if (!_sensorManager.hasSensors())
  {
    ESP_LOGI(TAG, "Initialization completed. No sensors found");
  }
  else
  {
    ESP_LOGI(TAG, "Initialization completed. No changes detected");
  }
  loadConfig();
}

bool ThermalSensors::startContinuousReading(const uint32_t intervalMs) {
  if (_readingTaskHandle != nullptr) return false;

  _readingIntervalMs = intervalMs;
  _continuousReading = true;

  xTaskCreate(
      readingTask,
      "TempReading",
      4096,
      this,
      tskIDLE_PRIORITY + 1,
      &_readingTaskHandle
  );

  return _readingTaskHandle != nullptr;
}

void ThermalSensors::stopContinuousReading() {
  _continuousReading = false;
  if (_readingTaskHandle != nullptr) {
    vTaskDelete(_readingTaskHandle);
    _readingTaskHandle = nullptr;
  }
}

void ThermalSensors::readingTask(void* params) {
  auto* instance = static_cast<ThermalSensors*>(params);

  while (instance->_continuousReading) {
    uint32_t startTime = millis();
    instance->performTemperatureReading();
    uint32_t elapsed = millis() - startTime;

    // Обновление диагностики
    instance->_lastReadTimeMs = millis();
    instance->_minReadTimeMs = min(instance->_minReadTimeMs, elapsed);
    instance->_maxReadTimeMs = max(instance->_maxReadTimeMs, elapsed);

    vTaskDelay(pdMS_TO_TICKS(instance->_readingIntervalMs));
  }

  vTaskDelete(nullptr);
}
void ThermalSensors::performTemperatureReading() {
    const uint32_t startTime = millis();
    ESP_LOGD(TAG, "Starting temperature reading process");

    if (!_sensorManager.requestTemperaturesForAllZones()) {
        _readErrors++;
        ESP_LOGE(TAG, "Failed to request temperatures for all zones");
        return;
    }

    std::vector<SensorTemperatureData> newData;
    const auto& config = _sensorManager.getConfig();
    ESP_LOGD(TAG, "Reading temperatures for %zu sensors", config.size());

    size_t validReadings = 0;
    for (const auto& entry : config) {
        const std::string& address = entry.first;
        const ThermalSensorZone zone = entry.second;

        if (const auto sensor = _sensorManager.getSensorByAddress(address)) {
            const float temperature = sensor->getTemperatureC();
            const bool valid = isValidTemperature(temperature);

            std::string zoneName;
            try {
                zoneName = ThermalSensorZoneHelper::toString(zone);
            } catch (const std::invalid_argument& e) {
                ESP_LOGW(TAG, "Invalid zone for sensor %s: %s", address.c_str(), e.what());
                zoneName = "unknown";
            }

            newData.push_back({address, zoneName, valid ? temperature : -999.0f, valid});
            if (valid) validReadings++;

            ESP_LOGD(TAG, "Sensor %s: %s (%.2f°C) %s",
                    address.c_str(),
                    zoneName.c_str(),
                    temperature,
                    valid ? "VALID" : "INVALID");
        }
    }

    ESP_LOGD(TAG, "Temperature reading complete. Valid readings: %zu/%zu",
             validReadings, config.size());

    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100))) {
        _cachedTemperatures = std::move(newData);
        xSemaphoreGive(_dataMutex);
        ESP_LOGD(TAG, "Cache updated successfully");

        // Уведомление подписчиков
        ESP_LOGD(TAG, "Notifying %zu registered callbacks", _callbacks.size());
        for (const auto& callback : _callbacks) {
            callback(_cachedTemperatures);
        }
    } else {
        _readErrors++;
        ESP_LOGE(TAG, "Failed to acquire mutex for cache update");
    }

    const uint32_t elapsed = millis() - startTime;
    ESP_LOGD(TAG, "Temperature reading process completed in %ums", elapsed);
}

// Получить свежие данные (игнорируя кэш)
// auto freshTemperatures = ThermalSensors::thermalController().getTemperatures(true);

std::vector<SensorTemperatureData> ThermalSensors::getTemperatures(const bool forceUpdate) {
    ESP_LOGD(TAG, "getTemperatures() called, forceUpdate: %d, continuousReading: %d",
             forceUpdate, _continuousReading);

    if (forceUpdate && !_continuousReading) {
        ESP_LOGD(TAG, "Performing forced temperature reading");
        performTemperatureReading();
    }

    std::vector<SensorTemperatureData> result;

    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50))) {
        ESP_LOGD(TAG, "Mutex acquired, copying cached data");
        result = _cachedTemperatures;
        xSemaphoreGive(_dataMutex);
        ESP_LOGD(TAG, "Mutex released, returning %zu temperature readings", result.size());
    } else {
        ESP_LOGW(TAG, "Failed to acquire mutex within timeout, returning empty result");
    }

    return result;
}

void ThermalSensors::registerUpdateCallback(
    const std::function<void(const std::vector<SensorTemperatureData>&)>& callback) {
    ESP_LOGD(TAG, "Registering new temperature update callback");
    _callbacks.push_back(callback);
    ESP_LOGD(TAG, "Total registered callbacks: %zu", _callbacks.size());
}

ThermalSensors::Diagnostics ThermalSensors::getDiagnostics() const {
    ESP_LOGD(TAG, "getDiagnostics() called");

    const size_t sensorCount = _sensorManager.getConfig().size();
    const uint32_t now = millis();
    const uint32_t timeSinceLastRead = _lastReadTimeMs > 0 ? now - _lastReadTimeMs : 0;

    ESP_LOGD(TAG, "Diagnostics info: "
             "lastRead=%ums ago, errors=%u, minReadTime=%ums, maxReadTime=%ums, sensors=%zu",
             timeSinceLastRead, _readErrors, _minReadTimeMs, _maxReadTimeMs, sensorCount);

    return {
        _lastReadTimeMs,
        _readErrors,
        _minReadTimeMs,
        _maxReadTimeMs,
        sensorCount
    };
}

bool ThermalSensors::isValidTemperature(const float temp) {
    const bool valid = temp != DEVICE_DISCONNECTED_C &&
                      !std::isnan(temp) &&
                      temp != 85.0f;

    if (!valid) {
        ESP_LOGD(TAG, "Invalid temperature detected: %.2f°C", temp);
    }

    return valid;
}

void ThermalSensors::loadConfig()
{
  ESP_LOGI(TAG, "Loading thermal sensors configuration...");

  std::unordered_map<std::string, ThermalSensorZone> loadedConfig;

  const bool success = GlobalConfig::config().getObject(
      thermalSensorsConfigName,
      "sensor_zones",
      loadedConfig,
      ThermalSensors::sensorConfigFromJson);

  if (success) {
    for (const auto& entry : loadedConfig) {
      _sensorManager.setSensorZone(entry.first, entry.second);
    }
    ESP_LOGI(TAG, "Loaded configuration for %zu sensors", loadedConfig.size());
  } else {
    ESP_LOGW(TAG, "No saved configuration found, using defaults");
  }
}


void ThermalSensors::saveConfig() const
{
  ESP_LOGI(TAG, "Saving thermal sensors configuration...");

  const auto& currentConfig = _sensorManager.getConfig();

  GlobalConfig::config().setObject(
      thermalSensorsConfigName,
      "sensor_zones",
      currentConfig,
      sensorConfigToJson);

  ESP_LOGI(TAG, "Configuration saved for %zu sensors", currentConfig.size());
}

void ThermalSensors::sensorConfigFromJson(const JsonObject& json, std::unordered_map<std::string, ThermalSensorZone>& config)
{
  config.clear();
  for (JsonPair kv : json) {
    const char* address = kv.key().c_str();
    const char* zoneStr = kv.value().as<const char*>();
    try {
      ThermalSensorZone zone = ThermalSensorZoneHelper::fromString(zoneStr);
      config[address] = zone;
    } catch (const std::invalid_argument& e) {
      ESP_LOGE(TAG, "Invalid zone '%s' for sensor %s", zoneStr, address);
    }
  }
}

void ThermalSensors::sensorConfigToJson(const std::unordered_map<std::string, ThermalSensorZone>& config, JsonObject& json)
{
  for (const auto& entry : config) {
    try {
      json[entry.first.c_str()] = ThermalSensorZoneHelper::toString(entry.second).c_str();
    } catch (const std::invalid_argument& e) {
      ESP_LOGE(TAG, "Failed to serialize zone for sensor %s", entry.first.c_str());
    }
  }
}

size_t ThermalSensors::scanAndAddAllSensors(ThermalSensorZone defaultZone)
{
  return _sensorManager.scanAndAddAllSensors(defaultZone);
}


std::unordered_map<std::string, std::vector<std::string>>
ThermalSensors::getSensorAddressesByZone() const
{
  std::unordered_map<std::string, std::vector<std::string>> result;

  // Получаем все адреса и зоны из ThermalSensorManager
  for (const auto& entry : _sensorManager.getConfig())
  {
    const std::string& address = entry.first;
    ThermalSensorZone zone = entry.second;

    // Получаем строковое описание зоны
    std::string zoneDescription;
    try
    {
      zoneDescription = ThermalSensorZoneHelper::toString(zone);
    }
    catch (const std::invalid_argument& e)
    {
      zoneDescription = "unknown"; // Если произошла ошибка, зона будет unknown
    }

    // Добавляем адрес в соответствующую зону
    result[zoneDescription].push_back(address);
  }

  return result;
}

std::unordered_map<std::string, std::vector<std::string>>
ThermalSensors::getZonesWithAddresses() const
{
  ESP_LOGI(TAG, "getZonesWithAddresses(): started");

  std::unordered_map<std::string, std::vector<std::string>> result;

  try
  {
    const auto& config = _sensorManager.getConfig();
    ESP_LOGI(TAG, "getZonesWithAddresses(): loaded %d entries from sensor manager", config.size());

    for (const auto& entry : config)
    {
      const std::string& address = entry.first;
      ThermalSensorZone zone = entry.second;

      ESP_LOGI(TAG, "Processing sensor address: %s", address.c_str());

      std::string zoneDescription;
      try
      {
        zoneDescription = ThermalSensorZoneHelper::toString(zone);
        ESP_LOGI(TAG, "Zone resolved to: %s", zoneDescription.c_str());
      }
      catch (const std::invalid_argument& e)
      {
        ESP_LOGI(TAG, "Invalid zone value for sensor %s, defaulting to 'unknown'", address.c_str());
        zoneDescription = "unknown";
      }

      result[zoneDescription].push_back(address);
      ESP_LOGI(TAG, "Added sensor %s to zone '%s'", address.c_str(), zoneDescription.c_str());
    }
  }
  catch (const std::exception& e)
  {
    ESP_LOGE(TAG, "Exception in getZonesWithAddresses(): %s", e.what());
  }
  catch (...)
  {
    ESP_LOGE(TAG, "Unknown exception occurred in getZonesWithAddresses()");
  }

  ESP_LOGI(TAG, "getZonesWithAddresses(): completed with %d zones", result.size());
  return result;
}

[[deprecated("Use startContinuousReading() instead")]]
std::vector<SensorTemperatureData> ThermalSensors::getAllSensorTemperatures()
{
  ESP_LOGD(TAG, "Starting temperature data collection");
  std::vector<SensorTemperatureData> sensorData;

  // Запрашиваем температуры для всех зон сразу
  if (!_sensorManager.requestTemperaturesForAllZones()) {
    ESP_LOGE(TAG, "Failed to request temperatures for all zones");
    return sensorData;
  }

  // Получаем конфигурацию (адрес -> зона)
  const auto& config = _sensorManager.getConfig();
  float temperature = DEVICE_DISCONNECTED_C;

  for (const auto& entry : config) {
    const std::string& address = entry.first;
    const ThermalSensorZone zone = entry.second;

    // Получаем датчик по адресу
    if (const auto sensor = _sensorManager.getSensorByAddress(address)) {
      // Делаем 3 попытки чтения температуры
      for (int attempt = 0; attempt < 3; ++attempt) {
        temperature = sensor->getTemperatureC();

        // Проверка на корректность температуры
        if (temperature != DEVICE_DISCONNECTED_C &&
            !std::isnan(temperature) &&
            temperature != 85.0f) {
          break;  // Успешное чтение
            }

        if (attempt < 2) {  // Не логируем последнюю неудачную попытку
          ESP_LOGW(TAG, "Attempt %d: Invalid reading from %s (value: %.2f)",
                  attempt + 1, address.c_str(), temperature);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
      }

      // Проверка на корректность температуры
      if (temperature == DEVICE_DISCONNECTED_C || std::isnan(temperature)) {
        ESP_LOGW(TAG, "Invalid temperature reading from sensor %s", address.c_str());
        temperature = -999.0f;
      } else if (temperature == 85.0f) {
        ESP_LOGW(TAG, "Got default value 85°C from sensor %s - possible timing issue",
                address.c_str());
        temperature = -999.0f;
      }

      // Получаем название зоны
      std::string zoneName;
      try {
        zoneName = ThermalSensorZoneHelper::toString(zone);
      } catch (const std::invalid_argument&) {
        zoneName = "unknown";
      }

      sensorData.push_back({address, zoneName, temperature});
      ESP_LOGD(TAG, "Sensor %s: zone=%s, temp=%.2f°C",
              address.c_str(), zoneName.c_str(), temperature);
    }
  }

  ESP_LOGD(TAG, "Completed temperature collection. Total sensors: %zu", sensorData.size());
  return sensorData;
}

bool ThermalSensors::setSensorZone(const std::string& address, ThermalSensorZone zone)
{
  if (!_sensorManager.setSensorZone(address, zone))
  {
    return false;
  }

  // Сохраняем новую конфигурацию
  saveConfig();
  return true;
}

// ThermalSensors.h
bool setSensorZone(const std::string& address, const std::string& zoneName);

// ThermalSensors.cpp
bool ThermalSensors::setSensorZone(const std::string& address, const std::string& zoneName)
{
  try
  {
    const ThermalSensorZone zone = ThermalSensorZoneHelper::fromString(zoneName);
    return setSensorZone(address, zone);
  }
  catch (const std::invalid_argument& e)
  {
    ESP_LOGE(TAG, "Invalid zone name: %s", zoneName.c_str());
    return false;
  }
}
