#ifndef DS18B20SENSOR_H
#define DS18B20SENSOR_H

#include "DallasTemperature.h"

#include <string>
#include <iomanip>
#include <sstream>
#include <drivers/DS18B20/SensorAddressUtils.h>

class DS18B20Sensor
{
public:
    using DeviceAddress = std::array<uint8_t, 8>;

    DS18B20Sensor(DallasTemperature& sensors, const DeviceAddress& address);
    // Получение температуры конкретного датчика
    float getTemperatureC() const;

    const DeviceAddress& getAddress() const;
    const std::string& getAddressString() const;
    std::string getAddressStringDelimited(char delimiter = '-') const;

private:
    DallasTemperature& sensors;
    DeviceAddress address;

    mutable std::string cachedAddress;

    static constexpr const char* TAG = "DS18B20Sensor";
};


#endif // DS18B20SENSOR_H
