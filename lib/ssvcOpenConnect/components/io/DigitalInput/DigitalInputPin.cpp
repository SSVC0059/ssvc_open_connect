#include "DigitalInputPin.h"

#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>

#include "components/subsystem/I2CBusSubsystem.h"
#include "core/AlarmMonitor/AlarmMonitor.h"

namespace {
constexpr auto TAG = "DigitalInputPin";
constexpr const char* kPcfInputRole = "pcf8574_in";
} // namespace

std::string DigitalInputSource::label() const {
    if (isPcf()) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "PCF8574 0x%02X · P%u", static_cast<unsigned>(pcfAddress),
                      static_cast<unsigned>(pcfBit));
        return std::string(buf);
    }
    return "GPIO " + std::to_string(static_cast<unsigned>(gpio));
}

bool DigitalInputPin::begin(const DigitalInputSource& source) {
    _source = source;
    _label = _source.label();

    if (_source.isPcf()) {
        I2CBusSubsystem::Guard bus(I2CBusSubsystem::getInstance());
        TwoWire* wire = bus.wire();
        if (wire == nullptr) {
            ESP_LOGE(TAG, "[%s] I2C bus is not active", _label.c_str());
            return false;
        }
        _pcf.setBus(wire);
        _pcf.setAddress(_source.pcfAddress);
        if (!_pcf.prepareInputs()) {
            ESP_LOGE(TAG, "[%s] prepareInputs failed — check the expander address", _label.c_str());
            return false;
        }
        ESP_LOGI(TAG, "[%s] Input expander ready (bits armed high)", _label.c_str());
        return true;
    }

    // Подтяжку линии делает монтаж: внешний резистор к земле для высокоуровневого
    // выхода (PNP) или к 3.3 В для открытого коллектора (NPN). Прошивка пин не тянет.
    pinMode(_source.gpio, INPUT);
    ESP_LOGI(TAG, "[%s] GPIO input ready (mode=INPUT, подтяжка внешняя)", _label.c_str());
    return true;
}

int DigitalInputPin::read(bool& ok) const {
    if (_source.isPcf()) {
        I2CBusSubsystem::Guard bus(I2CBusSubsystem::getInstance());
        _pcf.setBus(bus.wire());
        const int level = _pcf.readBit(_source.pcfBit, ok);
        if (ok) {
            AlarmMonitor::getInstance().clearHardwareFault(kPcfInputRole, _source.pcfAddress);
        } else {
            AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::I2C_NACK,
                                                           _source.pcfAddress, kPcfInputRole);
        }
        return level;
    }
    ok = true;
    return digitalRead(_source.gpio);
}

int DigitalInputPin::readPortByte(bool& ok) const {
    if (!_source.isPcf()) {
        ok = false;
        return -1;
    }
    I2CBusSubsystem::Guard bus(I2CBusSubsystem::getInstance());
    _pcf.setBus(bus.wire());
    uint8_t value = 0;
    ok = _pcf.readPort(value);
    return ok ? static_cast<int>(value) : -1;
}

int DigitalInputPin::readOnce(const DigitalInputSource& source, bool& ok) {    if (source.isPcf()) {
        I2CBusSubsystem::Guard bus(I2CBusSubsystem::getInstance());
        Pcf8574Input pcf;
        pcf.setBus(bus.wire());
        pcf.setAddress(source.pcfAddress);
        return pcf.readBit(source.pcfBit, ok);
    }
    ok = true;
    return digitalRead(source.gpio);
}
