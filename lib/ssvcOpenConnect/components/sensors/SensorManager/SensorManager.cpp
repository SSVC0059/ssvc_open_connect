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

#include "SensorManager.h"

#include "FSPersistence.h"
#include "core/SsvcOpenConnect.h"
#include "core/StatefulServices/SensorDataService/SensorDataService.h"


// Глобальная константа ошибки
#define SENSOR_ERROR_VALUE (-999.0f)

SensorManager& SensorManager::getInstance() {
    static SensorManager instance;
    ESP_LOGV(TAG, "Singleton instance retrieved.");
    return instance;
}

void SensorManager::registerSensor(AbstractSensor* newSensor) {
    if (!newSensor) {
        ESP_LOGW(TAG, "Attempted to register a NULL sensor. Ignoring.");
        return;
    }

    // Преобразуем адрес в строковый ключ
    const std::string key = addressToString(newSensor->getAddress());

    // Проверяем, что датчик с таким адресом еще не зарегистрирован
    if (sensors.count(key)) {
        ESP_LOGE(TAG, "Registration failed: Sensor with address %s already exists. Deleting new object.", key.c_str());
        delete newSensor; // Освобождаем память, так как дубликат не нужен
        return;
    }

    sensors[key] = newSensor;
    // Логируем тип датчика (используя getMeasurementType для информации)
    ESP_LOGV(TAG, "Registered sensor %s (Type: %d). Total sensors: %zu.",
             key.c_str(), (int)newSensor->getMeasurementType(), sensors.size());
}

const std::map<std::string, AbstractSensor*>& SensorManager::getAllSensors() const {
    ESP_LOGV(TAG, "Retrieving all %zu registered sensors.", sensors.size());
    return sensors;
}

AbstractSensor* SensorManager::getSensorByAddress(const AbstractSensor::Address& addr) const {
    const std::string key = addressToString(addr);
    if (sensors.count(key)) {
        ESP_LOGV(TAG, "Found sensor by address %s.", key.c_str());
        return sensors.at(key);
    }
    ESP_LOGW(TAG, "Sensor with address %s not found.", key.c_str());
    return nullptr;
}

// возвращает количество зарегистрированных датчиков
size_t SensorManager::getRegisteredSensorCount() const {
    return sensors.size();
}

float SensorManager::getValueByAddress(const std::string& addressStr) const {
    if (sensors.count(addressStr)) {
        AbstractSensor* sensor = sensors.at(addressStr);
        
        ESP_LOGV(TAG, "Requesting readValue() for sensor %s (Type: %d).",
                 addressStr.c_str(), (int)sensor->getMeasurementType());
        
        sensor->readValue();
        
        const float value = sensor->getData();
        ESP_LOGV(TAG, "Sensor %s read value: %.2f.", addressStr.c_str(), value);
        return value;
    }

    ESP_LOGE(TAG, "getValueByAddress failed: Sensor %s not found.", addressStr.c_str());
    return SENSOR_ERROR_VALUE;
}

bool SensorManager::isSensorRegistered(const std::string& addressStr) const {
    const bool exists = sensors.count(addressStr) > 0;
    
    ESP_LOGV(TAG, "Sensor registration check for %s: %s",
             addressStr.c_str(), exists ? "True" : "False");

    return exists;
}

bool SensorManager::assignZone(const AbstractSensor::Address& addr, SensorZone newZone) const {
    // 1. Преобразуем адрес для логирования
    const std::string addrStr = addressToString(addr);

    // 2. Ищем датчик
    AbstractSensor* sensor = getSensorByAddress(addr);

    if (sensor) {
        ESP_LOGV(TAG, "[ZONE_APPLY] Applying zone %d to sensor %s.",
                 static_cast<int>(newZone), addrStr.c_str());

        // 3. Устанавливаем зону
        sensor->setZone(newZone);
        
        if (sensor->getZone() == newZone) {
            ESP_LOGV(TAG, "[ZONE_APPLY] Zone %d successfully VERIFIED on sensor %s.",
                     static_cast<int>(newZone), addrStr.c_str());
        } else {
            ESP_LOGE(TAG, "[ZONE_APPLY] ZONE FAILED TO SET! Sensor %s still reports zone %d, expected %d.",
                     addrStr.c_str(), static_cast<int>(sensor->getZone()), static_cast<int>(newZone));
        }


        return true;
    }

    ESP_LOGW(TAG, "[ZONE_APPLY] Assign failed for sensor %s. Sensor object not found/registered.", addrStr.c_str());
    return false;
}

std::map<SensorZone, std::vector<AbstractSensor*>> SensorManager::getAllSensorsGroupedByZone() const {
    ESP_LOGV(TAG, "Grouping all sensors by zone (No filter).");
    std::map<SensorZone, std::vector<AbstractSensor*>> groupedSensors;
    for (auto const& pair : sensors) {
        groupedSensors[pair.second->getZone()].push_back(pair.second);
    }
    return groupedSensors;
}


// Новый метод с фильтрацией
std::map<SensorZone, std::vector<AbstractSensor*>> SensorManager::getAllSensorsGroupedByZone(
    const MeasuredValueType filterType) const
{
    ESP_LOGV(TAG, "Grouping sensors by zone with filter Type: %d.", (int)filterType);
    std::map<SensorZone, std::vector<AbstractSensor*>> groupedSensors;
    int filteredCount = 0;

    for (auto const& pair : sensors) {
        AbstractSensor* sensor = pair.second;

        if (sensor->getMeasurementType() == filterType) {

            // Если тип соответствует фильтру, группируем его по зоне
            groupedSensors[sensor->getZone()].push_back(sensor);
            filteredCount++;
        }
    }
    ESP_LOGV(TAG, "Filtering complete. Found %d sensors matching filter Type: %d.", filteredCount, (int)filterType);
    return groupedSensors;
}

std::string SensorManager::addressToString(const AbstractSensor::Address& addr) {
    char buffer[17];
    for (int i = 0; i < 8; i++) {
        sprintf(&buffer[i * 2], "%02X", addr[i]);
    }
    return {buffer, 16};
}

bool SensorManager::stringToAddress(const std::string& addrStr, AbstractSensor::Address& addr) {
    if (addrStr.length() != 16) {
        ESP_LOGE(TAG, "Address conversion failed: Length is %zu, expected 16.", addrStr.length());
        return false;
    }
    for (int i = 0; i < 8; i++) {
        char byteStr[3];
        if (addrStr.copy(byteStr, 2, i * 2) != 2) return false;
        byteStr[2] = '\0';

        char* endptr;
        const long val = strtol(byteStr, &endptr, 16);

        if (*endptr != '\0' || val < 0 || val > 255) {
             ESP_LOGE(TAG, "Address conversion failed: Invalid hex char in segment %d.", i);
             return false;
        }

        addr[i] = static_cast<uint8_t>(val);
    }
    ESP_LOGV(TAG, "Address %s converted successfully.", addrStr.c_str());
    return true;
}

void SensorManager::processReadingsAndPublish() const {
    const auto groupedData = this->getAllSensorsGroupedByZone();
    SensorDataService::getInstance()->updateSensorData(groupedData);
    ESP_LOGV(TAG, "Sensor data gathered and published to SensorDataService.");
}


SensorManager::~SensorManager() {
    ESP_LOGW(TAG, "SensorManager destructor called. Initiating cleanup.");
    cleanup();
}

void SensorManager::cleanup() {
    // Освобождение памяти, занятой объектами датчиков
    const size_t count = sensors.size();
    for (auto const& pair : sensors) {
        // Логирование адреса удаляемого датчика
        ESP_LOGV(TAG, "Deleting sensor object with address: %s", pair.first.c_str());
        delete pair.second;
    }
    sensors.clear();
    ESP_LOGW(TAG, "Cleanup complete. %zu sensor objects deleted.", count);
}
