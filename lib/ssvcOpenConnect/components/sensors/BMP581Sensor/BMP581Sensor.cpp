#include "BMP581Sensor.h"
#include "core/AlarmMonitor/AlarmMonitor.h"

#include <Wire.h>
#include <esp_log.h>

static const char* TAG = "BMP581_SENSOR";

// Программный EMA-фильтр. В диагностической сборке можно отключить
// (-DBMP581_USE_EMA=0), чтобы видеть реальные значения датчика.
#ifndef BMP581_USE_EMA
#define BMP581_USE_EMA 1
#endif


namespace {

constexpr float PRESSURE_MIN_HPA = 300.0f;
constexpr float PRESSURE_MAX_HPA = 1250.0f;

const char* powerModeName(const bmp581::PowerMode mode) {
    switch (mode) {
        case bmp581::PowerMode::STANDBY: return "standby";
        case bmp581::PowerMode::NORMAL: return "normal";
        case bmp581::PowerMode::FORCED: return "forced";
        case bmp581::PowerMode::CONTINUOUS: return "continuous";
        default: return "?";
    }
}

}  // namespace

/**
 * Транспорт I2C для bmp581::Driver: переводит операции с регистрами в
 * транзакции Wire под мьютексом I2C-шины.
 *
 * Чтение использует repeated start (как того требует BMP581), запись — с STOP.
 */
class BMP581Sensor::BusTransport final : public bmp581::Transport {
public:
    BusTransport(I2CBusSubsystem& bus, const uint8_t address) : _bus(bus), _address(address) {}

    bool writeReg(const uint8_t reg, const uint8_t value) override {
        I2CBusSubsystem::Guard guard(_bus);
        TwoWire* wire = _bus.getWire();
        if (!wire) return false;

        wire->beginTransmission(_address);
        wire->write(reg);
        wire->write(value);
        return wire->endTransmission() == 0;
    }

    bool readRegs(const uint8_t reg, uint8_t* dst, const size_t len) override {
        if (dst == nullptr || len == 0) return false;

        I2CBusSubsystem::Guard guard(_bus);
        TwoWire* wire = _bus.getWire();
        if (!wire) return false;

        wire->beginTransmission(_address);
        wire->write(reg);
        if (wire->endTransmission(false) != 0) return false;

        const size_t got = wire->requestFrom(_address, static_cast<uint8_t>(len));
        if (got != len) return false;

        for (size_t i = 0; i < len; ++i) {
            dst[i] = static_cast<uint8_t>(wire->read());
        }
        return true;
    }

    void delayMs(const uint32_t ms) override { delay(ms); }

private:
    I2CBusSubsystem& _bus;
    uint8_t _address;
};

BMP581Sensor::BMP581Sensor(const Address addr, const std::string& name, I2CBusSubsystem* bus,
                           const char* alarmDeviceRole)
    : AbstractSensor(addr, name, SensorZone::TANK),
      _bus(bus),
      _alarmDeviceRole(alarmDeviceRole ? alarmDeviceRole : "bmp581"),
      _transport(bus ? new BusTransport(*bus, addr[0]) : nullptr),
      _driver(_transport ? new bmp581::Driver(*_transport) : nullptr) {}

BMP581Sensor::~BMP581Sensor() = default;

const std::string& BMP581Sensor::getName() const {
    return AbstractSensor::getName();
}

MeasuredValueType BMP581Sensor::getMeasurementType() const {
    return MeasuredValueType::PRESSURE;
}


bool BMP581Sensor::initialize() {
    if (_isInitialized) {
        return true;
    }

    const uint8_t i2cAddr = getI2CAddress();

    if (!_bus || !_transport || !_driver) {
        ESP_LOGE(TAG, "[%s] I2C bus pointer is null!", getName().c_str());
        return false;
    }

    if (!_bus->isActive()) {
        ESP_LOGE(TAG, "[%s] I2C bus is not active", getName().c_str());
        AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::I2C_BUS_DOWN, 0, "i2c_bus");
        return false;
    }

    ESP_LOGI(TAG, "[%s] Initializing BMP581 at address 0x%02X...", getName().c_str(), i2cAddr);
    const uint32_t startedMs = millis();

    // Софт-ресет + программирование и ПРОВЕРКА конфигурации. Раньше здесь
    // оставались press_en=0 и standby, из-за чего датчик не делал конверсий,
    // а в регистрах данных всегда лежал маркер пустого кадра 0x7F7F7F.
    if (!_driver->begin()) {
        ESP_LOGE(TAG, "[%s] BMP581 init failed: %s", getName().c_str(), _driver->lastError());
        AlarmMonitor::getInstance().raiseHardwareFault(
            _driver->lastErrorCode() == bmp581::Error::BUS ? HardwareFaultCode::I2C_NACK
                                                           : HardwareFaultCode::DEVICE_NOT_PRESENT,
            i2cAddr, _alarmDeviceRole);
        return false;
    }

    const bmp581::Config cfg = _driver->readConfig();
    uint8_t osrEff = 0;
    uint8_t intStatus = 0;
    uint8_t status = 0;
    _driver->readRegister(bmp581::REG_OSR_EFF, osrEff);
    _driver->readRegister(bmp581::REG_INT_STATUS, intStatus);
    _driver->readRegister(bmp581::REG_STATUS, status);

    ESP_LOGD(TAG,
             "[%s] [DIAG] config: chip_id=0x%02X osr=0x%02X (press_en=%u osr_p=%u osr_t=%u) "
             "odr=0x%02X (odr_idx=%u pwr=%s) osr_eff=0x%02X int_status=0x%02X status=0x%02X",
             getName().c_str(), _driver->chipId(), cfg.osrConfig,
             static_cast<unsigned>(cfg.pressEnabled), static_cast<unsigned>(cfg.osrPress),
             static_cast<unsigned>(cfg.osrTemp), cfg.odrConfig, static_cast<unsigned>(cfg.odr),
             powerModeName(cfg.power), osrEff, intStatus, status);

    _isInitialized = true;
    AlarmMonitor::getInstance().clearHardwareFault(_alarmDeviceRole, i2cAddr);
    AlarmMonitor::getInstance().clearHardwareFault("i2c_bus", 0);

    ESP_LOGI(TAG, "[%s] BMP581 successfully initialized (press_en=1, %s mode, writes=%lu, %lu ms)",
             getName().c_str(), powerModeName(cfg.power),
             static_cast<unsigned long>(_driver->writeAttempts()),
             static_cast<unsigned long>(millis() - startedMs));
    return true;
}

void BMP581Sensor::poll() {
    ESP_LOGV(TAG, "Poll called for BMP581");
    readValue();
}

void BMP581Sensor::readValue() {
    if (!_isInitialized || !_bus || !_driver) return;

    const uint8_t i2cAddr = getI2CAddress();
    const uint32_t startedUs = micros();

    bmp581::Frame frame;
    const bool readOk = _driver->read(frame);
    const uint32_t elapsedUs = micros() - startedUs;

    if (!readOk) {
        // Пустой кадр (0x7F7F7F) или ошибка шины — значение не обновляем.
        _dataValid = false;
        ESP_LOGW(TAG, "[%s] No valid BMP581 frame: %s (invalid=%lu, reconfigures=%lu, %.1f ms)",
                 getName().c_str(), _driver->lastError(),
                 static_cast<unsigned long>(_driver->invalidFrames()),
                 static_cast<unsigned long>(_driver->reconfigures()),
                 elapsedUs / 1000.0f);
        AlarmMonitor::getInstance().raiseHardwareFault(
            _driver->lastErrorCode() == bmp581::Error::BUS ? HardwareFaultCode::I2C_NACK
                                                           : HardwareFaultCode::DEVICE_NOT_PRESENT,
            i2cAddr, _alarmDeviceRole);
        return;
    }

    const uint32_t rawPressure = frame.rawPressure;
    const uint32_t rawTemperature = frame.rawTemperature;
    const float pressureHpa = frame.pressureHpa;
    const float temperatureC = frame.temperatureC;

    // Счётчик подряд идущих одинаковых сырых значений — детектор «залипания».
    if (_hasLastRawPressure && rawPressure == _lastRawPressure) {
        _stuckCount++;
    } else {
        _stuckCount = 0;
    }
    _lastRawPressure = rawPressure;
    _hasLastRawPressure = true;

    if (pressureHpa < PRESSURE_MIN_HPA || pressureHpa > PRESSURE_MAX_HPA) {
        ESP_LOGD(TAG, "[%s] [DIAG] pressure %.1f hPa outside BMP581 range %.0f..%.0f hPa",
                 getName().c_str(), pressureHpa, PRESSURE_MIN_HPA, PRESSURE_MAX_HPA);
    }

    if (_stuckCount >= 3) {
        ESP_LOGV(TAG,
                 "[%s] [DIAG] raw pressure not changing for %lu readings (raw=0x%06lX, %.2f hPa)",
                 getName().c_str(), static_cast<unsigned long>(_stuckCount + 1),
                 static_cast<unsigned long>(rawPressure), pressureHpa);
    }

    const float pressureMmHg = bmp581::hpaToMmHg(pressureHpa);

#if BMP581_USE_EMA
    // Программный фильтр (EMA) для стабильности 3-го знака.
    if (_filteredPressure < 1.0f) {
        _filteredPressure = pressureMmHg;  // Первое чтение
    } else {
        constexpr float alpha = 0.15f;  // Коэффициент фильтрации
        _filteredPressure = (alpha * pressureMmHg) + (1.0f - alpha) * _filteredPressure;
    }
    _lastPressure = _filteredPressure;
#else
    _filteredPressure = pressureMmHg;
    _lastPressure = pressureMmHg;
#endif

    _dataValid = true;

    AlarmMonitor::getInstance().clearHardwareFault(_alarmDeviceRole, i2cAddr);
    AlarmMonitor::getInstance().clearHardwareFault("i2c_bus", 0);

    ESP_LOGV(TAG,
             "[%s] Pressure: %.3f mmHg (raw=0x%06lX -> %.2f hPa, %.1f Pa, T=%.2f C, %.1f ms)",
             getName().c_str(), _lastPressure, static_cast<unsigned long>(rawPressure),
             pressureHpa, bmp581::pressureFromRaw(rawPressure), temperatureC, elapsedUs / 1000.0f);

}

float BMP581Sensor::getData() const {
    return _lastPressure;
}

std::string BMP581Sensor::getUnit() const {
    return "мм рт. ст.";
}

std::string BMP581Sensor::getType() const {
    return "pressure";
}
