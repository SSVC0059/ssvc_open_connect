#include "BMP581Sensor.h"

static const char* TAG = "BMP581_SENSOR";

BMP581Sensor::BMP581Sensor(const Address addr, const std::string& name, I2CBusSubsystem* bus)
    : AbstractSensor(addr, name, SensorZone::TANK), _bus(bus) {}

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

    TwoWire* wire = _bus->getWire();
    const uint8_t i2c_addr = getI2CAddress();

    ESP_LOGI(TAG, "[%s] Initializing BMP581 at address 0x%02X...", getName().c_str(), i2c_addr);

    if (!wire || !_bmp.begin(i2c_addr, wire)) {
        ESP_LOGE(TAG, "[%s] Sensor NOT FOUND at address 0x%02X", getName().c_str(), i2c_addr);
        return false;
    }

    _bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_64X);
    _bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    _bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_64X);
    _bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);

    _isInitialized = true;
    ESP_LOGI(TAG, "[%s] BMP581 successfully initialized", getName().c_str());
    return true;
}

void BMP581Sensor::poll() {
    ESP_LOGI(TAG, "Poll called for BMP581");
    this->readValue();
}

void BMP581Sensor::readValue() {
    if (!_isInitialized) return;

    if (_bmp.performReading()) {
        // Прямой перевод из Паскалей в мм рт. ст.
        // 1 Pa * 0.00750061683 = 1 mmHg
        const float rawPressure = _bmp.pressure * 0.750061683F;

        // Программный фильтр (EMA) для стабильности 3-го знака
        if (_filteredPressure < 1.0f) {
            _filteredPressure = rawPressure; // Первое чтение
        } else {
            constexpr float alpha = 0.15f; // Коэффициент фильтрации
            _filteredPressure = (alpha * rawPressure) + (1.0f - alpha) * _filteredPressure;
        }

        _lastPressure = _filteredPressure;
        _dataValid = true;

        // Логируем с 3 знаками. Теперь они будут очень стабильными.
        ESP_LOGI(TAG, "[%s] Pressure: %.3f mmHg", getName().c_str(), _lastPressure);
    } else {
        _dataValid = false;
        ESP_LOGE(TAG, "[%s] Failed to perform reading!", getName().c_str());
    }
}

float BMP581Sensor::getData() const {
    return _lastPressure;
}

std::string BMP581Sensor::getUnit() const {
    return "ммРс";
}

std::string BMP581Sensor::getType() const {
    return "pressure";
}