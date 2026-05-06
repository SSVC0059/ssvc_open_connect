#include "Lcd1602Subsystem.h"

#include "components/subsystem/I2CBusSubsystem.h"
#include "core/AlarmMonitor/AlarmMonitor.h"
#include "core/SsvcConnector.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareSettingsService.h"

#include <WiFi.h>
#include <Wire.h>
#include <esp_log.h>

namespace {

constexpr uint8_t kLcdCols = 16;
constexpr uint8_t kLcdRows = 2;
constexpr TickType_t kRefreshInterval = pdMS_TO_TICKS(1500);

class Lcd1602Pcf8574 {
public:
    void attach(TwoWire* wire, uint8_t address) {
        _wire = wire;
        _address = address;
    }

    bool probe() const {
        if (_wire == nullptr) {
            return false;
        }
        _wire->beginTransmission(_address);
        return _wire->endTransmission() == 0;
    }

    bool begin() {
        if (!probe()) {
            return false;
        }

        // HD44780 4-bit mode init sequence via PCF8574 backpack.
        write4bits(0x03 << 4);
        vTaskDelay(pdMS_TO_TICKS(5));
        write4bits(0x03 << 4);
        vTaskDelay(pdMS_TO_TICKS(5));
        write4bits(0x03 << 4);
        vTaskDelay(pdMS_TO_TICKS(2));
        write4bits(0x02 << 4);

        command(0x28); // 4-bit, 2 lines, 5x8 font
        command(0x08); // display off
        command(0x01); // clear
        vTaskDelay(pdMS_TO_TICKS(2));
        command(0x06); // entry mode
        command(0x0C); // display on, cursor off
        return true;
    }

    void clear() {
        command(0x01);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    void setCursor(uint8_t col, uint8_t row) {
        static const uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
        command(static_cast<uint8_t>(0x80 | (col + rowOffsets[row])));
    }

    void printPadded(const String& value) {
        String text = value;
        if (text.length() > kLcdCols) {
            text = text.substring(0, kLcdCols);
        }
        while (text.length() < kLcdCols) {
            text += ' ';
        }
        for (size_t i = 0; i < text.length(); ++i) {
            write(static_cast<uint8_t>(text[i]));
        }
    }

private:
    void command(uint8_t value) { send(value, 0x00); }
    void write(uint8_t value) { send(value, 0x01); }

    void send(uint8_t value, uint8_t mode) {
        const uint8_t highNibble = value & 0xF0;
        const uint8_t lowNibble = static_cast<uint8_t>((value << 4) & 0xF0);
        write4bits(static_cast<uint8_t>(highNibble | mode));
        write4bits(static_cast<uint8_t>(lowNibble | mode));
    }

    void write4bits(uint8_t value) {
        expanderWrite(value);
        pulseEnable(value);
    }

    void pulseEnable(uint8_t value) {
        expanderWrite(static_cast<uint8_t>(value | 0x04));
        delayMicroseconds(1);
        expanderWrite(static_cast<uint8_t>(value & ~0x04));
        delayMicroseconds(50);
    }

    void expanderWrite(uint8_t value) {
        if (_wire == nullptr) {
            return;
        }
        _wire->beginTransmission(_address);
        _wire->write(static_cast<uint8_t>(value | _backlightMask));
        _wire->endTransmission();
    }

    TwoWire* _wire = nullptr;
    uint8_t _address = 0x27;
    uint8_t _backlightMask = 0x08;
};

const char* wifiModeLabel(const wifi_mode_t mode) {
    switch (mode) {
        case WIFI_MODE_STA:
            return "STA";
        case WIFI_MODE_AP:
            return "AP";
        case WIFI_MODE_APSTA:
            return "AP+STA";
        case WIFI_MODE_NULL:
        default:
            return "OFF";
    }
}

} // namespace

void Lcd1602Subsystem::initialize() {
    if (_initialized) {
        return;
    }
    _initialized = true;
    ESP_LOGI(TAG, "LCD1602 subsystem initialized");
}

void Lcd1602Subsystem::enable() {
    if (!_initialized || _enabled) {
        return;
    }

    _bus = &I2CBusSubsystem::getInstance();
    if (!_bus->isActive()) {
        ESP_LOGW(TAG, "I2C bus is inactive. LCD1602 cannot start.");
        SubsystemManager::instance().disableSubsystem(getName());
        return;
    }

    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        _addr = cfg.lcd1602I2cAddress;
    });

    const BaseType_t ok =
        xTaskCreatePinnedToCore(workerTaskEntry, "lcd1602_task", 4096, this, tskIDLE_PRIORITY + 1, &_worker, 1);
    if (ok != pdPASS) {
        _worker = nullptr;
        ESP_LOGE(TAG, "Failed to create LCD1602 task");
        SubsystemManager::instance().disableSubsystem(getName());
        return;
    }

    _enabled = true;
    ESP_LOGI(TAG, "LCD1602 subsystem enabled (addr=0x%02X)", static_cast<unsigned>(_addr));
}

void Lcd1602Subsystem::disable() {
    if (!_enabled) {
        return;
    }
    _enabled = false;
    if (_worker != nullptr) {
        if (_worker == xTaskGetCurrentTaskHandle()) {
            _worker = nullptr;
            clearDisplay();
            ESP_LOGI(TAG, "LCD1602 subsystem disabled");
            vTaskDelete(nullptr);
            return;
        }
        vTaskDelete(_worker);
        _worker = nullptr;
    }
    clearDisplay();
    ESP_LOGI(TAG, "LCD1602 subsystem disabled");
}

void Lcd1602Subsystem::workerTaskEntry(void* arg) {
    auto* self = static_cast<Lcd1602Subsystem*>(arg);
    self->workerLoop();
}

void Lcd1602Subsystem::workerLoop() {
    TwoWire* wire = (_bus != nullptr) ? _bus->getWire() : nullptr;
    Lcd1602Pcf8574 lcd;
    lcd.attach(wire, _addr);
    if (!lcd.begin()) {
        ESP_LOGE(TAG, "LCD1602 not responding at 0x%02X", static_cast<unsigned>(_addr));
        AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::DEVICE_NOT_PRESENT, _addr, "lcd1602");
        SubsystemManager::instance().disableSubsystem(getName());
        return;
    }

    AlarmMonitor::getInstance().clearHardwareFault("lcd1602", _addr);
    uint32_t page = 0;
    for (;;) {
        writeStatusPage(page);
        page = (page + 1) % 2;
        vTaskDelay(kRefreshInterval);
    }
}

void Lcd1602Subsystem::writeStatusPage(const uint32_t pageIndex) {
    TwoWire* wire = (_bus != nullptr) ? _bus->getWire() : nullptr;
    Lcd1602Pcf8574 lcd;
    lcd.attach(wire, _addr);

    if (!lcd.probe()) {
        AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::I2C_NACK, _addr, "lcd1602");
        return;
    }

    AlarmMonitor::getInstance().clearHardwareFault("lcd1602", _addr);

    String line0;
    String line1;
    if (pageIndex == 0) {
        const wifi_mode_t mode = WiFi.getMode();
        line0 = String("WiFi: ") + wifiModeLabel(mode);
        line1 = String("IP: ") + (WiFi.isConnected() ? WiFi.localIP().toString() : String("-"));
    } else {
        const bool uartOk = !SsvcConnector::getConnector().uartCommunicationError;
        const auto faults = AlarmMonitor::getInstance().getActiveHardwareFaults();
        line0 = String("UART: ") + (uartOk ? "OK" : "LOST");
        if (faults.empty()) {
            line1 = "Errors: none";
        } else {
            const auto& first = faults.front();
            line1 = String("Err: ") + first.role.c_str();
        }
    }

    lcd.setCursor(0, 0);
    lcd.printPadded(line0);
    lcd.setCursor(0, 1);
    lcd.printPadded(line1);
}

void Lcd1602Subsystem::clearDisplay() {
    if (_bus == nullptr || !_bus->isActive()) {
        return;
    }
    TwoWire* wire = _bus->getWire();
    if (!wire) {
        return;
    }
    Lcd1602Pcf8574 lcd;
    lcd.attach(wire, _addr);
    if (lcd.probe()) {
        lcd.clear();
    }
}
