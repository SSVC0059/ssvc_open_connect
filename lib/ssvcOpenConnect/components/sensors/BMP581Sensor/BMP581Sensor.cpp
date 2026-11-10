#include "BMP581Sensor.h"
#include "core/AlarmMonitor/AlarmMonitor.h"

#include <cmath>

static const char* TAG = "BMP581_SENSOR";

// Программный EMA-фильтр. В диагностической сборке отключается (-DBMP581_USE_EMA=0),
// чтобы видеть реальные значения датчика, а не сглаженные.
#ifndef BMP581_USE_EMA
#define BMP581_USE_EMA 1
#endif

// Подробная диагностика (сырые регистры, конфигурация, единицы библиотеки).
#ifndef BMP581_DIAGNOSTICS
#define BMP581_DIAGNOSTICS 0
#endif

namespace {

// Регистры BMP581 (даташит Bosch, раздел 6.2 "Register map").
constexpr uint8_t REG_CHIP_ID = 0x01;
constexpr uint8_t REG_DATA_P_MSB = 0x1D;  // 0x1D..0x1F — давление, 24 бита
constexpr uint8_t REG_DATA_T_MSB = 0x20;  // 0x20..0x22 — температура, 24 бита
constexpr uint8_t REG_INT_STATUS = 0x30;
constexpr uint8_t REG_STATUS = 0x31;
constexpr uint8_t REG_OSR_CONFIG = 0x36;
constexpr uint8_t REG_ODR_CONFIG = 0x37;

// Масштабы BMP581: давление — 1/64 Па на LSB, температура — 1/2^16 °C на LSB.
constexpr float PRESSURE_LSB_PA = 1.0f / 64.0f;
constexpr float TEMPERATURE_LSB_C = 1.0f / 65536.0f;
// 1 Па = 0.00750061683 мм рт. ст.
constexpr float PA_TO_MMHG = 0.00750061683f;

// Маркер «данных нет» в регистрах данных BMP581.
constexpr uint32_t RAW_DATA_INVALID = 0x800000u;

// Рабочий диапазон BMP581, гПа (даташит: 300...1250 hPa).
constexpr float PRESSURE_MIN_HPA = 300.0f;
constexpr float PRESSURE_MAX_HPA = 1250.0f;

const char* powerModeName(uint8_t mode) {
    switch (mode & 0x03u) {
        case 0: return "standby";
        case 1: return "normal";
        case 2: return "forced";
        case 3: return "continuous";
        default: return "?";
    }
}

}  // namespace

BMP581Sensor::BMP581Sensor(const Address addr, const std::string& name, I2CBusSubsystem* bus,
                           const char* alarmDeviceRole)
    : AbstractSensor(addr, name, SensorZone::TANK),
      _bus(bus),
      _alarmDeviceRole(alarmDeviceRole ? alarmDeviceRole : "bmp581") {}

const std::string& BMP581Sensor::getName() const {
    return AbstractSensor::getName();
}

MeasuredValueType BMP581Sensor::getMeasurementType() const {
    return MeasuredValueType::PRESSURE;
}

bool BMP581Sensor::initialize() {
    if (!_bus) {
        ESP_LOGE(TAG, "[%s] I2C Bus pointer is null!", getName().c_str());
        return false;
    }

    // Библиотека BMP581 делает многословные транзакции, их нельзя прерывать
    // чужой передачей по той же шине.
    I2CBusSubsystem::Guard bus(*_bus);
    TwoWire* wire = bus.wire();
    const uint8_t i2c_addr = getI2CAddress();

    ESP_LOGI(TAG, "[%s] Initializing BMP581 at address 0x%02X...", getName().c_str(), i2c_addr);

    if (!wire) {
        ESP_LOGE(TAG, "[%s] I2C wire unavailable", getName().c_str());
        AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::I2C_BUS_DOWN, 0, "i2c_bus");
        return false;
    }

    if (!_bmp.begin(i2c_addr, wire)) {
        ESP_LOGE(TAG, "[%s] Sensor NOT FOUND at address 0x%02X", getName().c_str(), i2c_addr);
        AlarmMonitor::getInstance().raiseHardwareFault(
            HardwareFaultCode::DEVICE_NOT_PRESENT, i2c_addr, _alarmDeviceRole);
        return false;
    }

    _bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_64X);
    _bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    _bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_64X);
    // Без явного ODR датчик может остаться без новых конверсий: регистры данных
    // тогда не обновляются, и в программу попадает одно и то же значение.
    _bmp.setOutputDataRate(BMP5XX_ODR_50_HZ);
    _bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);

    // Диагностика: что реально записалось в конфигурационные регистры.
    const uint8_t chipId = readRegister(REG_CHIP_ID);
    const uint8_t osrConfig = readRegister(REG_OSR_CONFIG);
    const uint8_t odrConfig = readRegister(REG_ODR_CONFIG);
    const uint8_t intStatus = readRegister(REG_INT_STATUS);
    const uint8_t status = readRegister(REG_STATUS);

    ESP_LOGI(TAG,
             "[%s] [DIAG] config: chip_id=0x%02X osr=0x%02X (press_en=%u temp_en=%u) "
             "odr=0x%02X (odr_idx=%u pwr=%s) int_status=0x%02X status=0x%02X",
             getName().c_str(), chipId, osrConfig, (osrConfig >> 6) & 0x01u,
             (osrConfig >> 5) & 0x01u, odrConfig, (odrConfig >> 2) & 0x1Fu,
             powerModeName(odrConfig), intStatus, status);

    if (((osrConfig >> 6) & 0x01u) == 0) {
        ESP_LOGE(TAG, "[%s] [DIAG] pressure measurement is DISABLED (press_en=0)", getName().c_str());
    }
    if (((odrConfig >> 2) & 0x1Fu) == 0) {
        ESP_LOGE(TAG, "[%s] [DIAG] output data rate is 0 — датчик не будет выдавать новые данные",
                 getName().c_str());
    }
    if ((odrConfig & 0x03u) != 0x01u) {
        ESP_LOGW(TAG, "[%s] [DIAG] power mode is %s, expected normal", getName().c_str(),
                 powerModeName(odrConfig));
    }

    _isInitialized = true;
    AlarmMonitor::getInstance().clearHardwareFault(_alarmDeviceRole, i2c_addr);
    AlarmMonitor::getInstance().clearHardwareFault("i2c_bus", 0);
    ESP_LOGI(TAG, "[%s] BMP581 successfully initialized", getName().c_str());
    return true;
}

void BMP581Sensor::poll() {
    ESP_LOGI(TAG, "Poll called for BMP581");
    this->readValue();
}

void BMP581Sensor::readValue() {
    if (!_isInitialized || _bus == nullptr) return;

    const uint8_t i2c_addr = getI2CAddress();

    // Считывание регистров BMP581 — одна непрерываемая транзакция.
    I2CBusSubsystem::Guard bus(*_bus);

    const uint32_t startedUs = micros();
    const bool readOk = _bmp.performReading();
    const uint32_t elapsedUs = micros() - startedUs;

    if (!readOk) {
        _dataValid = false;
        ESP_LOGE(TAG, "[%s] Failed to perform reading! (%.1f ms)", getName().c_str(),
                 elapsedUs / 1000.0f);
        AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::I2C_NACK, i2c_addr,
                                                       _alarmDeviceRole);
        return;
    }

    // Сырые регистры: результат не зависит от того, в каких единицах
    // конкретная версия Adafruit_BMP5xx отдаёт поле .pressure.
    const uint32_t rawPressure = readRaw24(REG_DATA_P_MSB);
    const uint32_t rawTemperature = readRaw24(REG_DATA_T_MSB);

    // Валидные данные давления всегда < 0x800000; 0x800000 — маркер «данных нет».
    const bool rawValid = (rawPressure < RAW_DATA_INVALID);

    int32_t tempRaw = static_cast<int32_t>(rawTemperature);
    if (tempRaw & 0x800000) tempRaw -= 0x1000000;  // знаковое 24-битное значение
    const float temperatureC = static_cast<float>(tempRaw) * TEMPERATURE_LSB_C;

    const float pressurePa = static_cast<float>(rawPressure) * PRESSURE_LSB_PA;
    const float pressureHpa = pressurePa / 100.0f;
    const float pressureMmHg = pressurePa * PA_TO_MMHG;

    // Счётчик подряд идущих одинаковых сырых значений — детектор «залипания».
    if (_hasLastRawPressure && rawPressure == _lastRawPressure) {
        _stuckCount++;
    } else {
        _stuckCount = 0;
    }
    _lastRawPressure = rawPressure;
    _hasLastRawPressure = true;
    _readCount++;

    if (!rawValid) {
        _dataValid = false;
        ESP_LOGE(TAG,
                 "[%s] [DIAG] invalid raw pressure 0x%06lX — датчик не выполнил конверсию "
                 "(stuck=%lu, %.1f ms)",
                 getName().c_str(), static_cast<unsigned long>(rawPressure),
                 static_cast<unsigned long>(_stuckCount), elapsedUs / 1000.0f);
        AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::I2C_NACK, i2c_addr,
                                                       _alarmDeviceRole);
        return;
    }

    if (pressureHpa < PRESSURE_MIN_HPA || pressureHpa > PRESSURE_MAX_HPA) {
        ESP_LOGW(TAG, "[%s] [DIAG] pressure %.1f hPa outside BMP581 range %.0f..%.0f hPa",
                 getName().c_str(), pressureHpa, PRESSURE_MIN_HPA, PRESSURE_MAX_HPA);
    }

    if (_stuckCount >= 3) {
        ESP_LOGW(TAG,
                 "[%s] [DIAG] raw pressure not changing for %lu readings (raw=0x%06lX, %.2f hPa)",
                 getName().c_str(), static_cast<unsigned long>(_stuckCount + 1),
                 static_cast<unsigned long>(rawPressure), pressureHpa);
    }

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

    AlarmMonitor::getInstance().clearHardwareFault(_alarmDeviceRole, i2c_addr);
    AlarmMonitor::getInstance().clearHardwareFault("i2c_bus", 0);

    ESP_LOGI(TAG,
             "[%s] Pressure: %.3f mmHg (raw=0x%06lX -> %.2f hPa, %.1f Pa, T=%.2f C, %.1f ms)",
             getName().c_str(), _lastPressure, static_cast<unsigned long>(rawPressure),
             pressureHpa, pressurePa, temperatureC, elapsedUs / 1000.0f);

#if BMP581_DIAGNOSTICS
    logDiagnostics(rawPressure, rawTemperature, pressureMmHg);
#endif
}

uint8_t BMP581Sensor::readRegister(const uint8_t reg) {
    TwoWire* wire = _bus ? _bus->getWire() : nullptr;
    if (!wire) return 0xFF;

    const uint8_t i2cAddr = getI2CAddress();
    wire->beginTransmission(i2cAddr);
    wire->write(reg);
    if (wire->endTransmission(false) != 0) return 0xFF;
    if (wire->requestFrom(i2cAddr, static_cast<uint8_t>(1)) != 1) return 0xFF;
    return wire->read();
}

uint32_t BMP581Sensor::readRaw24(const uint8_t reg) {
    TwoWire* wire = _bus ? _bus->getWire() : nullptr;
    if (!wire) return RAW_DATA_INVALID;

    const uint8_t i2cAddr = getI2CAddress();
    wire->beginTransmission(i2cAddr);
    wire->write(reg);
    if (wire->endTransmission(false) != 0) return RAW_DATA_INVALID;
    if (wire->requestFrom(i2cAddr, static_cast<uint8_t>(3)) != 3) return RAW_DATA_INVALID;

    uint32_t value = static_cast<uint32_t>(wire->read()) << 16;
    value |= static_cast<uint32_t>(wire->read()) << 8;
    value |= static_cast<uint32_t>(wire->read());
    return value;
}

void BMP581Sensor::logDiagnostics(const uint32_t rawPressure, const uint32_t rawTemperature,
                                  const float pressureMmHg) {
    const uint8_t osrConfig = readRegister(REG_OSR_CONFIG);
    const uint8_t odrConfig = readRegister(REG_ODR_CONFIG);
    const uint8_t intStatus = readRegister(REG_INT_STATUS);
    const uint8_t status = readRegister(REG_STATUS);
    const uint8_t chipId = readRegister(REG_CHIP_ID);

    const float rawPa = static_cast<float>(rawPressure) * PRESSURE_LSB_PA;
    const float rawHpa = rawPa / 100.0f;

    int32_t tempRaw = static_cast<int32_t>(rawTemperature);
    if (tempRaw & 0x800000) tempRaw -= 0x1000000;
    const float temperatureC = static_cast<float>(tempRaw) * TEMPERATURE_LSB_C;

    // Проверяем, в каких единицах библиотека отдаёт .pressure: сравниваем
    // её значение с посчитанным из сырых регистров в Па и в гПа.
    const float libPressure = _bmp.pressure;
    const float diffPa = fabsf(libPressure - rawPa);
    const float diffHpa = fabsf(libPressure - rawHpa);
    const char* libUnit = (diffPa <= diffHpa) ? "Pa" : "hPa";
    const float diffRel = (rawPa > 0.0f) ? (fabsf(libPressure - rawPa) / rawPa) : 0.0f;

    ESP_LOGI(TAG,
             "[%s] [DIAG] #%lu chip_id=0x%02X raw_p=0x%06lX raw_t=0x%06lX | "
             "P=%.3f hPa (%.1f Pa, %.3f mmHg) T=%.2f C | "
             "lib.pressure=%.3f (%s, rel.err=%.1f%%) lib.temperature=%.2f C | "
             "osr=0x%02X press_en=%u temp_en=%u odr=0x%02X odr_idx=%u pwr=%s | "
             "int_status=0x%02X status=0x%02X | stuck=%lu same_raw",
             getName().c_str(), static_cast<unsigned long>(_readCount), chipId,
             static_cast<unsigned long>(rawPressure), static_cast<unsigned long>(rawTemperature),
             rawHpa, rawPa, pressureMmHg, temperatureC, libPressure, libUnit, diffRel * 100.0f,
             _bmp.temperature, osrConfig, (osrConfig >> 6) & 0x01u, (osrConfig >> 5) & 0x01u,
             odrConfig, (odrConfig >> 2) & 0x1Fu, powerModeName(odrConfig), intStatus, status,
             static_cast<unsigned long>(_stuckCount));

    if (((osrConfig >> 6) & 0x01u) == 0) {
        ESP_LOGE(TAG, "[%s] [DIAG] press_en=0 — измерение давления выключено", getName().c_str());
    }
    if ((odrConfig & 0x03u) != 0x01u) {
        ESP_LOGW(TAG, "[%s] [DIAG] pwr_mode=%s — ожидался normal", getName().c_str(),
                 powerModeName(odrConfig));
    }
    if (diffRel > 0.5f) {
        ESP_LOGW(TAG,
                 "[%s] [DIAG] lib.pressure %.1f не совпадает с raw %.1f Pa (rel.err=%.1f%%) — "
                 "проверьте единицы библиотеки",
                 getName().c_str(), libPressure, rawPa, diffRel * 100.0f);
    }
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
