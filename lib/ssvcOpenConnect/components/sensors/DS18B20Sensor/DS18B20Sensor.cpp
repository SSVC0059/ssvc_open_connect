// DS18B20Sensor.cpp

#include "DS18B20Sensor.h"

#include "components/sensors/OneWireThermalSubsystem/OneWireThermalSubsystem.h"
#include "components/sensors/SensorManager/SensorManager.h"


// Тег для логирования в этом файле
static const char *TAG = "DS18B20_SENSOR";

DS18B20Sensor::DS18B20Sensor(
    const Address addr,
    const std::string& name,
    DallasTemperature* sensors,
    const SensorZone zone
)
    // Обязательный вызов конструктора родителя
    : AbstractSensor(addr, name, zone),
      dallasSensors(sensors),
      lastReading(DEVICE_DISCONNECTED_C)
{

    const std::string addressStr = SensorManager::addressToString(reinterpret_cast<const AbstractSensor::Address&>(addr));
    
    ESP_LOGV(TAG, "New DS18B20 Sensor created. Name: %s, Address: %s, Zone: %d.", 
             getName().c_str(), addressStr.c_str(), static_cast<int>(zone));
}

/**
 * @brief Запускает считывание температуры и сохраняет результат.
 */
void DS18B20Sensor::readValue() {
    // Получаем значение для конкретного адреса.
    const std::string addressStr = SensorManager::addressToString(address);
    
    // Считывание из кэша DallasTemperature
    lastReading = dallasSensors->getTempC(address);
    
    if (lastReading == DEVICE_DISCONNECTED_C) {
        ESP_LOGI(TAG, "Sensor %s (%s) read failed. Value: %.2f.",
                 getName().c_str(), addressStr.c_str(), lastReading);
        this->_dataValid = false;
    } else {
        ESP_LOGI(TAG, "Sensor %s (%s) updated. New temperature: %.2f C.",
                 getName().c_str(), addressStr.c_str(), lastReading);
        this->_dataValid = true;
    }
    this->_isInitialized = true;
}

/**
  * @brief Возвращает последнее считанное значение температуры.
  */
float DS18B20Sensor::getData() const {
    // При получении данных часто достаточно уровня DEBUG
    ESP_LOGI(TAG, "Returning last reading (%.2f C) for sensor %s.",
             lastReading, getName().c_str());
             
    return lastReading;
}

/**
 * @brief Реализация виртуального метода.
 * @return Всегда возвращает TEMPERATURE, так как DS18B20 измеряет только температуру.
 */
MeasuredValueType DS18B20Sensor::getMeasurementType() const {
    return MeasuredValueType::TEMPERATURE;
}