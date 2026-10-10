#include "Xkc25Sensor.h"

#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "XKC25_SENSOR";

AbstractSensor::Address& Xkc25Sensor::createAddr(const uint8_t pin) {
    static Address a = {};
    a[0] = pin;
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
    : AbstractSensor(createAddr(config.gpio), makeName(index), zoneFromIndex(config.zoneIndex)),
      _index(index),
      _gpio(config.gpio),
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
    // Подтяжку линии делает монтаж: внешний резистор к земле для высокоуровневого
    // выхода (PNP) или к 3.3 В для открытого коллектора (NPN). Прошивка пин не
    // тянет — иначе внутренние 45 кОм сдвигали бы уровни внешнего делителя.
    pinMode(_gpio, INPUT);
}

bool Xkc25Sensor::initialize() {
    applyInputMode();
    _isInitialized = true;
    readValue();
    ESP_LOGI(TAG, "[%s] Initialized on GPIO %u (activeLow=%d, mode=INPUT, подтяжка внешняя)",
             getName().c_str(), static_cast<unsigned>(_gpio), _activeLow ? 1 : 0);
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

    const int raw = digitalRead(_gpio);
    ESP_LOGI(TAG, "[%s] raw: GPIO%u = %d (%s), activeLow=%d -> filled=%s, value=%.0f",
             getName().c_str(), static_cast<unsigned>(_gpio), raw, raw == HIGH ? "HIGH" : "LOW",
             _activeLow ? 1 : 0, isFilled() ? "YES" : "NO", static_cast<double>(_lastValue));
}

Xkc25Sensor::LineProbe Xkc25Sensor::probeLine(const char* stage) const {
    LineProbe probe;

    // Полярность на момент калибровки может быть ещё не определена, поэтому мало
    // одного рабочего режима: снимаем уровень при обеих подтяжках. Одинаковые
    // чтения означают, что линию держит внешний драйвер, разные — что линия
    // высокоомна и уровень в этом состоянии задаёт сама подтяжка.
    pinMode(_gpio, INPUT_PULLUP);
    delay(2); // перезаряд линии после смены подтяжки
    probe.pullUp = readLevelMajority();

    pinMode(_gpio, INPUT_PULLDOWN);
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
    for (uint8_t i = 0; i < 3; ++i) {
        if (digitalRead(_gpio) == HIGH) {
            ++high;
        }
        delay(1);
    }
    return high >= 2 ? HIGH : LOW;
}

int Xkc25Sensor::readRawPinLevel() const {
    return digitalRead(_gpio);
}

void Xkc25Sensor::applyCalibration(const bool activeLow) {
    _activeLow = activeLow;
    applyInputMode();

    // Пересчёт без генерации события фронта: калибровка не является срабатыванием.
    const int raw = digitalRead(_gpio);
    _lastValue = (_activeLow ? (raw == LOW) : (raw == HIGH)) ? 1.0f : 0.0f;
    _dataValid = true;

    ESP_LOGI(TAG, "[%s] Calibration applied (activeLow=%d, level=%d)", getName().c_str(),
             _activeLow ? 1 : 0, raw);
}

bool Xkc25Sensor::sampleStable(const uint8_t samples, const uint16_t intervalMs, int& levelOut) const {
    if (samples == 0) {
        return false;
    }
    int first = digitalRead(_gpio);
    for (uint8_t i = 1; i < samples; ++i) {
        if (intervalMs > 0) {
            delay(intervalMs);
        }
        const int raw = digitalRead(_gpio);
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
    const int raw = digitalRead(_gpio);
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
