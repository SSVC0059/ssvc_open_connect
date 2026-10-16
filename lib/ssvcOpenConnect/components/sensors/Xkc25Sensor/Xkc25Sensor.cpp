#include "Xkc25Sensor.h"

#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "XKC25_SENSOR";

// Источник входа задаётся конфигом; коды должны совпадать с DigitalInputSourceKind.
static_assert(Xkc25SensorConfig::kPinSourceGpio == DigitalInputSourceKind::Gpio,
              "Xkc25SensorConfig and DigitalInputSourceKind disagree on GPIO source");
static_assert(Xkc25SensorConfig::kPinSourcePcf8574 == DigitalInputSourceKind::Pcf8574,
              "Xkc25SensorConfig and DigitalInputSourceKind disagree on PCF8574 source");

namespace {

DigitalInputSource sourceFromConfig(const Xkc25SensorConfig& config) {
    DigitalInputSource source;
    source.source = config.pinSource;
    source.gpio = config.gpio;
    source.pcfAddress = config.pcfAddress;
    source.pcfBit = config.pcfBit;
    return source;
}

} // namespace

AbstractSensor::Address& Xkc25Sensor::createAddr(const Xkc25SensorConfig& config) {
    // Адрес различает источник так, чтобы SensorManager не путал GPIO и бит расширителя:
    // GPIO → address[0] = pin; PCF8574 → address[0] = 0x80|bit, address[1] = адрес.
    // 0x80|bit (128..135) не пересекается с валидными GPIO (0..39).
    static Address a = {};
    a[0] = 0;
    a[1] = 0;
    if (config.pinSource == Xkc25SensorConfig::kPinSourcePcf8574) {
        a[0] = static_cast<uint8_t>(0x80u | (config.pcfBit & 0x07u));
        a[1] = config.pcfAddress;
    } else {
        a[0] = config.gpio;
    }
    return a;
}

SensorZone Xkc25Sensor::zoneFromIndex(const uint8_t zoneIndex) {
    switch (zoneIndex) {
        case 0: return SensorZone::RECEIVER_TANK_1;
        case 1: return SensorZone::RECEIVER_TANK_2;
        default: return SensorZone::RECEIVER_TANK_3;
    }
}

std::string Xkc25Sensor::makeName(const uint8_t index) {
    return "Уровень приёмной ёмкости " + std::to_string(static_cast<unsigned>(index) + 1);
}

Xkc25Sensor::Xkc25Sensor(const Xkc25SensorConfig& config, const uint8_t index)
    : AbstractSensor(createAddr(config), makeName(index), zoneFromIndex(config.zoneIndex)),
      _index(index),
      _src(sourceFromConfig(config)),
      _zoneIndex(config.zoneIndex),
      _activeLow(config.activeLow),
      _sharedAcrossTanks(config.sharedAcrossTanks) {}

void Xkc25Sensor::setZoneIndex(const uint8_t zoneIndex) {
    const uint8_t next = zoneIndex > 2 ? 0 : zoneIndex;
    if (next == _zoneIndex) {
        return;
    }
    _zoneIndex = next;
    setZone(zoneFromIndex(next));
    ESP_LOGI(TAG, "[%s] Zone switched to %s", getName().c_str(),
             SensorZoneHelper::toString(currentZone).c_str());
}

const std::string& Xkc25Sensor::getName() const {
    return AbstractSensor::getName();
}

MeasuredValueType Xkc25Sensor::getMeasurementType() const {
    return MeasuredValueType::LEVEL;
}

void Xkc25Sensor::applyInputMode() const {
    // GPIO: подтяжку линии делает монтаж (внешний резистор) — прошивка пин не тянет.
    // PCF8574: подготовка входов расширителя (write 0xFF). Оба случая — в DigitalInputPin.
    _pin.begin(_src);
}

bool Xkc25Sensor::initialize() {
    if (!_pin.begin(_src)) {
        ESP_LOGE(TAG, "[%s] input source unavailable — initialization failed",
                 pinLabel().c_str());
        return false;
    }
    _isInitialized = true;
    readValue();
    ESP_LOGI(TAG, "[%s] Initialized on %s (activeLow=%d)", getName().c_str(),
             pinLabel().c_str(), _activeLow ? 1 : 0);
    return true;
}

void Xkc25Sensor::poll() {
    readValue();
    logRawState();
}

void Xkc25Sensor::logRawState() {
    const uint32_t now = millis();
    // Первое чтение логируем сразу, дальше — не чаще kRawLogIntervalMs.
    if (_lastRawLogMs != 0 && (now - _lastRawLogMs) < kRawLogIntervalMs) {
        return;
    }
    _lastRawLogMs = now;

    bool ok = true;
    const int raw = _pin.read(ok);
    if (!ok) {
        ESP_LOGW(TAG, "[%s] read failed (transport error) — level unknown", pinLabel().c_str());
        return;
    }
    ESP_LOGI(TAG, "[%s] raw: %s = %d (%s), activeLow=%d -> filled=%s, value=%.0f",
             getName().c_str(), pinLabel().c_str(), raw, raw == HIGH ? "HIGH" : "LOW",
             _activeLow ? 1 : 0, isFilled() ? "YES" : "NO", static_cast<double>(_lastValue));
}

Xkc25Sensor::LineProbe Xkc25Sensor::probeLine(const char* stage) const {
    LineProbe probe;

    if (!_pin.supportsPullProbe()) {
        // У PCF8574 нет настраиваемых внутренних подтяжек, а его собственный слабый
        // ток подтяжки так не тестируется: проба неприменима. Полярность определит
        // сравнение двух сырых уровней на шаге «с жидкостью».
        probe.verdict = "проба подтяжкой неприменима (PCF8574)";
        ESP_LOGI(TAG, "[%s] line probe skipped (%s): %s", getName().c_str(), stage,
                 probe.verdict.c_str());
        return probe;
    }

    // Полярность на момент калибровки может быть ещё не определена, поэтому мало
    // одного рабочего режима: снимаем уровень при обеих подтяжках. Одинаковые
    // чтения означают, что линию держит внешний драйвер, разные — что линия
    // высокоомна и уровень в этом состоянии задаёт сама подтяжка.
    pinMode(_src.gpio, INPUT_PULLUP);
    delay(2); // перезаряд линии после смены подтяжки
    probe.pullUp = readLevelMajority();

    pinMode(_src.gpio, INPUT_PULLDOWN);
    delay(2);
    probe.pullDown = readLevelMajority();

    applyInputMode();

    if (probe.pullUp == HIGH && probe.pullDown == HIGH) {
        probe.verdict = "прижата к высокому уровню";
    } else if (probe.pullUp == LOW && probe.pullDown == LOW) {
        probe.verdict = "прижата к земле";
    } else if (probe.pullUp == HIGH && probe.pullDown == LOW) {
        probe.verdict = "высокоомная, состояние задаёт подтяжка";
    } else {
        probe.verdict = "нестабильна";
    }

    ESP_LOGI(TAG, "[%s] line probe (%s): pull-up=%d, pull-down=%d -> %s", getName().c_str(), stage,
             probe.pullUp, probe.pullDown, probe.verdict.c_str());
    return probe;
}

int Xkc25Sensor::readLevelMajority() const {
    // Три чтения подряд: одиночное может попасть на перезаряд линии.
    uint8_t high = 0;
    uint8_t valid = 0;
    for (uint8_t i = 0; i < 3; ++i) {
        bool ok = true;
        const int level = _pin.read(ok);
        if (ok) {
            ++valid;
            if (level == HIGH) {
                ++high;
            }
        }
        delay(1);
    }
    if (valid == 0) {
        return LOW; // сбой транспорта; проба всё равно помечается недействительной
    }
    return (static_cast<unsigned>(high) * 2 >= valid) ? HIGH : LOW;
}

int Xkc25Sensor::readRawPinLevel() const {
    bool ok = true;
    const int level = _pin.read(ok);
    return ok ? level : -1; // -1 — чтение не удалось (сбой шины), а не уровень 0
}

void Xkc25Sensor::applyCalibration(const bool activeLow) {
    _activeLow = activeLow;
    applyInputMode();

    // Пересчёт без генерации события фронта: калибровка не является срабатыванием.
    bool ok = true;
    const int raw = _pin.read(ok);
    if (ok) {
        _lastValue = (_activeLow ? (raw == LOW) : (raw == HIGH)) ? 1.0f : 0.0f;
    }
    _dataValid = ok;

    ESP_LOGI(TAG, "[%s] Calibration applied (activeLow=%d, level=%d)", getName().c_str(),
             _activeLow ? 1 : 0, raw);
}

bool Xkc25Sensor::sampleStable(const uint8_t samples, const uint16_t intervalMs, int& levelOut) const {
    if (samples == 0) {
        return false;
    }
    bool ok = true;
    const int first = _pin.read(ok);
    if (!ok) {
        ESP_LOGW(TAG, "[%s] read failed while sampling", pinLabel().c_str());
        return false;
    }
    for (uint8_t i = 1; i < samples; ++i) {
        if (intervalMs > 0) {
            delay(intervalMs);
        }
        bool stepOk = true;
        const int raw = _pin.read(stepOk);
        if (!stepOk) {
            ESP_LOGW(TAG, "[%s] read failed while sampling", pinLabel().c_str());
            return false;
        }
        if (raw != first) {
            ESP_LOGW(TAG, "[%s] Unstable signal: %d then %d", getName().c_str(), first, raw);
            return false;
        }
    }
    levelOut = first;
    return true;
}

void Xkc25Sensor::readValue() {
    if (!_isInitialized) {
        return;
    }
    bool ok = true;
    const int raw = _pin.read(ok);
    if (!ok) {
        // Сбой транспорта — не уровень: состояние сохраняем, фронт не генерируем.
        ESP_LOGW(TAG, "[%s] read failed — keeping previous state", pinLabel().c_str());
        return;
    }
    const bool active = _activeLow ? (raw == LOW) : (raw == HIGH);
    const float newValue = active ? 1.0f : 0.0f;

    // Первое измерение задаёт исходное состояние и событием не считается: иначе
    // загрузка с уже заполненной ёмкостью воспринималась бы как фронт.
    const bool hasPreviousValue = _dataValid;
    const bool valueChanged = hasPreviousValue && (newValue != _lastValue);

    _lastValue = newValue;
    _dataValid = true;

    if (valueChanged && _observer != nullptr) {
        ESP_LOGI(TAG, "[%s] Level changed: raw %d (%s) -> %s (value %.0f, activeLow=%d)",
                 getName().c_str(), raw, raw == HIGH ? "HIGH" : "LOW", active ? "FILLED" : "CLEAR",
                 static_cast<double>(_lastValue), _activeLow ? 1 : 0);
        _observer->onXkc25LevelChanged(*this, active);
    }
}

float Xkc25Sensor::getData() const {
    return _lastValue;
}

std::string Xkc25Sensor::getUnit() const {
    return "";
}

std::string Xkc25Sensor::getType() const {
    return "level";
}
