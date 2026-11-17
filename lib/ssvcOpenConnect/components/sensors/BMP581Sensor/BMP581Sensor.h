#ifndef BMP581_SENSOR_H
#define BMP581_SENSOR_H

#include "components/sensors/AbstractSensor/AbstractSensor.h"
#include "components/sensors/PollingSubsystem/PollingSubsystem.h"
#include "components/sensors/BMP581Sensor/BMP581Driver.h"
#include "components/subsystem/I2CBusSubsystem.h"
#include <memory>

/**
 * @brief Датчик абсолютного давления BMP581 на шине I2C.
 *
 * Работа с регистрами вынесена в `bmp581::Driver` (см. BMP581Driver.h):
 * он проверяет идентификатор чипа, программирует и **верифицирует**
 * конфигурацию (включённый канал давления, normal mode) и отбраковывает
 * пустые кадры (0x7F7F7F / 0x800000), которые раньше превращались в
 * «замороженные» 979.266 mmHg.
 */
class BMP581Sensor final : public AbstractSensor, public PollingSubsystem {
public:
    /** @param alarmDeviceRole стабильный идентификатор для `AlarmMonitor` (строковый литерал). */
    BMP581Sensor(const uint8_t i2cAddr, const std::string& name, I2CBusSubsystem* bus,
                 const char* alarmDeviceRole = "bmp581")
        : BMP581Sensor(createAddr(i2cAddr), name, bus, alarmDeviceRole) {}

    BMP581Sensor(const Address addr, const std::string& name, I2CBusSubsystem* bus,
                 const char* alarmDeviceRole = "bmp581");

    ~BMP581Sensor() override;

    BMP581Sensor(const BMP581Sensor&) = delete;
    BMP581Sensor& operator=(const BMP581Sensor&) = delete;

    // Реализация для PollingSubsystem
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
    /** Транспорт I2C (Wire) для bmp581::Driver; определён в .cpp. */
    class BusTransport;

    static Address& createAddr(const uint8_t i2c) {
        static Address a = {};
        a[0] = i2c;
        return a;
    }

    I2CBusSubsystem* _bus;
    const char* _alarmDeviceRole;
    std::unique_ptr<BusTransport> _transport;
    std::unique_ptr<bmp581::Driver> _driver;

    float _lastPressure = 0.0f;
    float _filteredPressure = 0.0f;

    // Число подряд идущих одинаковых сырых значений (детектор «датчик не
    // выдаёт новые конверсии»).
    uint32_t _stuckCount = 0;
    uint32_t _lastRawPressure = 0;
    bool _hasLastRawPressure = false;

    static constexpr auto TAG = "BMP581Sensor";
};

#endif
