#include "ThermalSensorManager.h"

ThermalSensorManager::ThermalSensorManager() : _bus(THERMAL_SENSOR_PIN)
{
}

void ThermalSensorManager::begin()
{
  ESP_LOGI("ThermalSensorManager", "Initializing bus...");
  _bus.begin();
}

size_t ThermalSensorManager::scanAndAddAllSensors(ThermalSensorZone defaultZone)
{
  ESP_LOGI("ThermalSensorManager", "Начат скан шины на наличие датчиков...");
  auto devices = _bus.discoverDevices();
  size_t changed = 0;

  if (devices.empty())
  {
    ESP_LOGE("ThermalSensorManager", "Датчики DS18B20 не найдены!");
    return 0;
  }

  ESP_LOGI("ThermalSensorManager", "Обнаружено датчиков: %zu", devices.size());

  std::unordered_set<std::string> foundAddresses;

  // Обработка найденных датчиков
  for (auto& sensor : devices)
  {
    const std::string addr = sensor->getAddressString();
    foundAddresses.insert(addr);

    ESP_LOGI("ThermalSensorManager", "Найден адрес датчика: %s", addr.c_str());

    auto it = _addressToZoneMap.find(addr);
    if (it == _addressToZoneMap.end())
    {
      // Новый датчик
      ESP_LOGI("ThermalSensorManager", "Новый датчик, добавляется в зону %d", static_cast<int>(defaultZone));
      _addressToZoneMap[addr] = defaultZone;
      _sensorsByZone[defaultZone].push_back(std::move(sensor));
      changed++;
      ESP_LOGI("ThermalSensorManager", "Датчик %s добавлен в зону %d", addr.c_str(), static_cast<int>(defaultZone));
    }
    else
    {
      ThermalSensorZone zone = it->second;

      // Восстановление из DELETED
      if (zone == ThermalSensorZone::DELETED)
      {
        ESP_LOGW("ThermalSensorManager", "Датчик %s был ранее удалён. Восстановление в зону по умолчанию (%d)...",
                 addr.c_str(), static_cast<int>(defaultZone));
        _addressToZoneMap[addr] = defaultZone;
        _sensorsByZone[defaultZone].push_back(std::move(sensor));
        changed++;
        ESP_LOGI("ThermalSensorManager", "Датчик %s восстановлен в зону %d", addr.c_str(),
                 static_cast<int>(defaultZone));
      }
      else
      {
        // Датчик уже в системе
        ESP_LOGI("ThermalSensorManager", "Датчик %s уже зарегистрирован в зоне %d", addr.c_str(),
                 static_cast<int>(zone));
        _sensorsByZone[zone].push_back(std::move(sensor));
        ESP_LOGI("ThermalSensorManager", "Датчик %s добавлен в список зоны %d", addr.c_str(), static_cast<int>(zone));
      }
    }
  }

  // Удаление датчиков, которых больше нет
  for (auto it = _addressToZoneMap.begin(); it != _addressToZoneMap.end();)
  {
    if (foundAddresses.find(it->first) == foundAddresses.end())
    {
      ESP_LOGW("ThermalSensorManager", "Датчик %s больше не найден, помечен как удалённый", it->first.c_str());
      it->second = ThermalSensorZone::DELETED;
      changed++;
      it = _addressToZoneMap.erase(it);
    }
    else
    {
      ++it;
    }
  }

  ESP_LOGI("ThermalSensorManager", "Сканирование завершено. Изменений: %zu", changed);
  return changed;
}

bool ThermalSensorManager::hasSensors() const
{
  // Проходим по всем зонам и проверяем, есть ли хотя бы один датчик
  for (const auto& entry : _sensorsByZone)
  {
    if (!entry.second.empty())
    {
      return true; // Если хотя бы одна зона содержит датчик, возвращаем true
    }
  }
  return false; // Если все зоны пустые, возвращаем false
}

void ThermalSensorManager::applyConfig(
  const std::unordered_map<std::string, ThermalSensorZone>& config)
{
  _addressToZoneMap = config;
}

std::unordered_map<std::string, ThermalSensorZone>
ThermalSensorManager::getConfig() const
{
  return _addressToZoneMap;
}

DS18B20Sensor*
ThermalSensorManager::getSensorByAddress(const std::string& address) const
{
  for (const auto& zoneEntry : _sensorsByZone)
  {
    for (const auto& sensor : zoneEntry.second)
    {
      if (sensor->getAddressString() == address)
      {
        return sensor.get();
      }
    }
  }
  return nullptr;
}

const std::vector<std::unique_ptr<DS18B20Sensor>>&
ThermalSensorManager::getSensorsByZone(ThermalSensorZone zone) const
{
  static const std::vector<std::unique_ptr<DS18B20Sensor>> empty;
  auto it = _sensorsByZone.find(zone);
  if (it != _sensorsByZone.end())
  {
    return it->second;
  }
  return empty;
}

bool ThermalSensorManager::setSensorZone(const std::string& address, ThermalSensorZone zone)
{
  auto it = _addressToZoneMap.find(address);
  if (it == _addressToZoneMap.end())
  {
    ESP_LOGE(TAG, "Sensor with address %s not found", address.c_str());
    return false;
  }

  // Если зона не изменилась, ничего не делаем
  if (it->second == zone)
  {
    ESP_LOGI(TAG, "Sensor %s already in zone %d", address.c_str(), static_cast<int>(zone));
    return true;
  }

  ThermalSensorZone oldZone = it->second;

  // 1. Находим датчик в старой зоне
  auto& oldZoneSensors = _sensorsByZone[oldZone];
  auto sensorIt = std::find_if(oldZoneSensors.begin(), oldZoneSensors.end(),
                               [&address](const std::unique_ptr<DS18B20Sensor>& sensor)
                               {
                                 return sensor->getAddressString() == address;
                               });

  if (sensorIt == oldZoneSensors.end())
  {
    ESP_LOGE(TAG, "Sensor %s not found in zone %d", address.c_str(), static_cast<int>(oldZone));
    return false;
  }

  // 2. Перемещаем во временную переменную
  std::unique_ptr<DS18B20Sensor> sensorToMove = std::move(*sensorIt);
  oldZoneSensors.erase(sensorIt);

  // 3. Добавляем в новую зону
  _sensorsByZone[zone].push_back(std::move(sensorToMove));

  // 4. Обновляем маппинг
  _addressToZoneMap[address] = zone;

  ESP_LOGI(TAG, "Sensor %s moved from zone %d to zone %d",
           address.c_str(), static_cast<int>(oldZone), static_cast<int>(zone));
  return true;
}

bool ThermalSensorManager::isValidAddress(const std::string& address) const
{
  return _addressToZoneMap.find(address) != _addressToZoneMap.end();
}

bool ThermalSensorManager::getSensorZone(const std::string& address, ThermalSensorZone& outZone) const
{
  const auto it = _addressToZoneMap.find(address);
  if (it != _addressToZoneMap.end())
  {
    outZone = it->second;
    return true;
  }
  return false;
}

std::vector<std::pair<std::string, float>> ThermalSensorManager::getTemperaturesByZone(ThermalSensorZone zone)
{
  std::vector<std::pair<std::string, float>> result;

  const auto& zoneSensors = getSensorsByZone(zone);
  if (zoneSensors.empty()) return result;

  if (!_bus.requestTemperatures(true)) {
    ESP_LOGE(TAG, "Temperature request failed for zone %d", static_cast<int>(zone));
    return result;
  }

  for (const auto& sensor : zoneSensors) {
    float temp = sensor->getTemperatureC();
    result.emplace_back(sensor->getAddressString(), temp);

    if (temp == DEVICE_DISCONNECTED_C) {
      ESP_LOGE(TAG, "Error reading sensor %s", sensor->getAddressString().c_str());
    }
  }

  return result;
}

bool ThermalSensorManager::requestTemperaturesForAllZones() {
  return _bus.requestTemperatures(true);
}
