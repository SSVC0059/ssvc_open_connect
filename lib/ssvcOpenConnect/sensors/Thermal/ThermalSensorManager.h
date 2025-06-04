#ifndef THERMALSENSORMANAGER_H
#define THERMALSENSORMANAGER_H

#include <unordered_map>
#include <unordered_set>
#include <drivers/DS18B20/DS18B20Sensor.h>
#include <drivers/DS18B20/DS18B20Bus.h>
#include "ThermalSensorZone.h"
#include "commons/commons.h"

#define THERMAL_SENSOR_PIN 2

class ThermalSensorManager
{
public:
    ThermalSensorManager();
    void begin();
    size_t scanAndAddAllSensors(ThermalSensorZone defaultZone);
    bool hasSensors() const;
    void applyConfig(const std::unordered_map<std::string, ThermalSensorZone>& config);

    std::unordered_map<std::string, ThermalSensorZone> getConfig() const;
    DS18B20Sensor* getSensorByAddress(const std::string& address) const;
    const std::vector<std::unique_ptr<DS18B20Sensor>>& getSensorsByZone(
        ThermalSensorZone zone) const;
    bool setSensorZone(const std::string& address, ThermalSensorZone zone);
    bool isValidAddress(const std::string& address) const;
    bool getSensorZone(const std::string& address, ThermalSensorZone& outZone) const;
    bool requestTemperaturesForAllZones();

    /**
 * @brief Получает температуры всех датчиков в указанной зоне
 * @param zone Зона датчиков
 * @return Вектор пар <адрес датчика, температура>
 */
    std::vector<std::pair<std::string, float>> getTemperaturesByZone(ThermalSensorZone zone);

private:
    DS18B20Bus _bus;
    OneWire _oneWire;
    DallasTemperature _dallasTemp;
    std::unordered_map<ThermalSensorZone, std::vector<std::unique_ptr<DS18B20Sensor>>> _sensorsByZone;
    std::unordered_map<std::string, ThermalSensorZone> _addressToZoneMap;

    static constexpr auto TAG = "ThermalSensorManager";
};

#endif //THERMALSENSORMANAGER_H
