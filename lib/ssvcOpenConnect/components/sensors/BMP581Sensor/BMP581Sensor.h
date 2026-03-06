#ifndef BMP581_SENSOR_H
#define BMP581_SENSOR_H

#include "components/sensors/AbstractSensor/AbstractSensor.h"
#include "components/sensors/PollingSubsystem/PollingSubsystem.h"
#include "components/subsystem/I2CBusSubsystem.h"
#include <Adafruit_BMP5xx.h>

class BMP581Sensor final : public AbstractSensor, public PollingSubsystem {
public:
    // Новый удобный конструктор
    BMP581Sensor(const uint8_t i2cAddr, const std::string& name, I2CBusSubsystem* bus)
        : BMP581Sensor(createAddr(i2cAddr), name, bus) {}

    BMP581Sensor(const Address addr, const std::string& name, I2CBusSubsystem* bus);

    // Реализация для PollingSubsystem (виртуальный метод)
    // Теперь это "override" виртуальной функции, а не "hiding" статической
    const std::string& getName() const override;

    std::string getUnit() const override;
    std::string getType() const override;

    // Реализация для AbstractSensor
    MeasuredValueType getMeasurementType() const override;

    bool initialize() override;
    void poll() override;
    void readValue() override;
    float getData() const override;

private:
    static Address& createAddr(const uint8_t i2c) {
        static Address a = {};
        a[0] = i2c;
        return a;
    }

    I2CBusSubsystem* _bus;
    Adafruit_BMP5xx _bmp;
    float _lastPressure = 0.0f;
    float _filteredPressure = 0.0f;

    static constexpr auto TAG = "BMP581Sensor";
};

#endif