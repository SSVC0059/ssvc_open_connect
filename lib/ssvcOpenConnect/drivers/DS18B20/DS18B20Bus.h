#ifndef DS18B20BUS_H
#define DS18B20BUS_H

#include "OneWire.h"
#include "DallasTemperature.h"
#include <vector>
#include <array>

#include "DS18B20Sensor.h"

class DS18B20Bus
{
public:
    using DeviceAddress = std::array<uint8_t, 8>;

    explicit DS18B20Bus(uint8_t pin);
    void begin();
    std::vector<std::unique_ptr<DS18B20Sensor>> discoverDevices();
    DallasTemperature& getSensorsInterface();

    // Запрос температуры для всех датчиков на шине
    bool requestTemperatures(bool waitForConversion = true);

    // Проверка готовности данных
    bool isConversionComplete() const;

private:
    OneWire oneWire;
    DallasTemperature sensors;

    TickType_t _lastRequestTime = 0;
    TickType_t _conversionStartTime = 0;
    TickType_t _conversionTimeout = 0;
    bool _waitForConversion = false;

    static constexpr uint32_t MAX_CONVERSION_TIME_MS = 750; // Для 12-bit разрешения

    static constexpr const char* TAG = "DS18B20Bus";
};

#endif // DS18B20BUS_H
