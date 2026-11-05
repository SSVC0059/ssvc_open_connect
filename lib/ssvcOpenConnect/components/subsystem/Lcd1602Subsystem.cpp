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
/**
 * Пауза перед первой повторной попыткой поднять дисплей. Модуль может выйти на режим
 * позже контроллера (медленный подъём питания, позднее подключение разъёма).
 */
constexpr TickType_t kInitRetryDelay = pdMS_TO_TICKS(2000);
/** Предел нарастания паузы между попытками: подключить дисплей могут и через минуты. */
constexpr TickType_t kInitRetryDelayMax = pdMS_TO_TICKS(30000);
/**
 * Профилактическая инициализация рабочего дисплея.
 *
 * Потеря настроек HD44780 (просадка питания, сброс контроллера) по шине не видна:
 * PCF8574 в том же модуле продолжает отвечать ACK, обмен проходит без ошибок, а экран
 * остаётся выключенным — пустым. Отличить это состояние от рабочего пробой невозможно,
 * поэтому настройки периодически переписываются заново.
 */
constexpr TickType_t kReinitInterval = pdMS_TO_TICKS(60000);

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

        // Первичное состояние экспандера (все выходы на нём после подачи питания — единицы,
        // то есть EN=1) и пауза на выход самого контроллера дисплея на режим. Без этого
        // серия 0x30 уходит в ещё не готовый HD44780: он остаётся в 8-битном режиме и с
        // выключенным экраном, а обмен по шине при этом выглядит полностью исправным.
        const bool powered = expanderWrite(0x00);
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!powered) {
            return false;
        }

        // HD44780 4-bit mode init sequence via PCF8574 backpack.
        bool ok = write4bits(0x03 << 4);
        vTaskDelay(pdMS_TO_TICKS(5));
        ok = write4bits(0x03 << 4) && ok;
        vTaskDelay(pdMS_TO_TICKS(5));
        ok = write4bits(0x03 << 4) && ok;
        vTaskDelay(pdMS_TO_TICKS(2));
        ok = write4bits(0x02 << 4) && ok;

        ok = command(0x28) && ok; // 4-bit, 2 lines, 5x8 font
        ok = command(0x08) && ok; // display off
        ok = command(0x01) && ok; // clear
        vTaskDelay(pdMS_TO_TICKS(2));
        ok = command(0x06) && ok; // entry mode
        ok = command(0x0C) && ok; // display on, cursor off
        return ok;
    }

    bool clear() {
        const bool cleared = command(0x01);
        vTaskDelay(pdMS_TO_TICKS(2));
        return cleared;
    }

    bool setCursor(uint8_t col, uint8_t row) {
        static const uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
        if (row > 3) {
            return false;
        }
        return command(static_cast<uint8_t>(0x80 | (col + rowOffsets[row])));
    }

    bool printPadded(const String& value) {
        String text = value;
        if (text.length() > kLcdCols) {
            text = text.substring(0, kLcdCols);
        }
        while (text.length() < kLcdCols) {
            text += ' ';
        }
        bool ok = true;
        for (size_t i = 0; i < text.length(); ++i) {
            ok = write(static_cast<uint8_t>(text[i])) && ok;
        }
        return ok;
    }

private:
    bool command(uint8_t value) { return send(value, 0x00); }
    bool write(uint8_t value) { return send(value, 0x01); }

    bool send(uint8_t value, uint8_t mode) {
        const uint8_t highNibble = value & 0xF0;
        const uint8_t lowNibble = static_cast<uint8_t>((value << 4) & 0xF0);
        const bool high = write4bits(static_cast<uint8_t>(highNibble | mode));
        const bool low = write4bits(static_cast<uint8_t>(lowNibble | mode));
        return high && low;
    }

    bool write4bits(uint8_t value) {
        const bool wrote = expanderWrite(value);
        const bool pulsed = pulseEnable(value);
        return wrote && pulsed;
    }

    bool pulseEnable(uint8_t value) {
        const bool up = expanderWrite(static_cast<uint8_t>(value | 0x04));
        delayMicroseconds(1);
        const bool down = expanderWrite(static_cast<uint8_t>(value & ~0x04));
        delayMicroseconds(50);
        return up && down;
    }

    bool expanderWrite(uint8_t value) {
        if (_wire == nullptr) {
            return false;
        }
        _wire->beginTransmission(_address);
        _wire->write(static_cast<uint8_t>(value | _backlightMask));
        return _wire->endTransmission() == 0;
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

    _enabled = true;
    const BaseType_t ok =
        xTaskCreatePinnedToCore(workerTaskEntry, "lcd1602_task", 4096, this, tskIDLE_PRIORITY + 1, &_worker, 1);
    if (ok != pdPASS) {
        _enabled = false;
        _worker = nullptr;
        ESP_LOGE(TAG, "Failed to create LCD1602 task");
        SubsystemManager::instance().disableSubsystem(getName());
        return;
    }

    ESP_LOGI(TAG, "LCD1602 subsystem enabled (addr=0x%02X)", static_cast<unsigned>(_addr));
}

void Lcd1602Subsystem::disable() {
    if (!_enabled && _worker == nullptr) {
        return;
    }
    _enabled = false;
    if (_worker != nullptr) {
        if (_worker == xTaskGetCurrentTaskHandle()) {
            ESP_LOGI(TAG, "LCD1602 subsystem shutdown requested from worker");
            return;
        }
        for (int i = 0; i < 50 && _worker != nullptr; ++i) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (_worker != nullptr) {
            ESP_LOGW(TAG, "LCD1602 worker did not exit cleanly; forcing delete");
            vTaskDelete(_worker);
            _worker = nullptr;
            clearDisplay();
        }
    } else {
        clearDisplay();
    }
    ESP_LOGI(TAG, "LCD1602 subsystem disabled");
}

void Lcd1602Subsystem::workerTaskEntry(void* arg) {
    auto* self = static_cast<Lcd1602Subsystem*>(arg);
    self->workerLoop();
    self->_worker = nullptr;
    // Defensive: FreeRTOS task entry functions must not return normally.
    vTaskDelete(nullptr);
}

void Lcd1602Subsystem::workerLoop() {
    TwoWire* wire = (_bus != nullptr) ? _bus->getWire() : nullptr;
    if (wire == nullptr) {
        ESP_LOGE(TAG, "LCD1602: I2C bus is not available");
        AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::I2C_BUS_DOWN, _addr, "lcd1602");
        return;
    }

    Lcd1602Pcf8574 lcd;
    lcd.attach(wire, _addr);

    bool ready = false;
    TickType_t retryDelay = kInitRetryDelay;
    TickType_t uptime = 0;
    uint32_t page = 0;

    while (_enabled) {
        if (!ready) {
            bool inited = false;
            {
                // Шину делят дисплей, RTC, PCF8574 и HTTP-проверки: без блокировки
                // чужая транзакция вклинится между командами инициализации.
                I2CBusSubsystem::Guard bus(*_bus);
                inited = lcd.begin();
            }
            if (!inited) {
                // Один NACK — не приговор: модуль мог быть ещё не готов или подключён позже.
                // Не выключаем подсистему, а повторяем с нарастающей паузой, держа ошибку.
                AlarmMonitor::getInstance().raiseHardwareFault(
                    HardwareFaultCode::DEVICE_NOT_PRESENT, _addr, "lcd1602");
                ESP_LOGW(TAG, "LCD1602 not responding at 0x%02X; next attempt in %u ms",
                         static_cast<unsigned>(_addr),
                         static_cast<unsigned>(retryDelay * portTICK_PERIOD_MS));
                vTaskDelay(retryDelay);
                if (retryDelay < kInitRetryDelayMax) {
                    const TickType_t next = retryDelay * 2;
                    retryDelay = (next > kInitRetryDelayMax) ? kInitRetryDelayMax : next;
                }
                continue;
            }
            AlarmMonitor::getInstance().clearHardwareFault("lcd1602", _addr);
            ESP_LOGI(TAG, "LCD1602 initialized (addr=0x%02X)", static_cast<unsigned>(_addr));
            ready = true;
            retryDelay = kInitRetryDelay;
            uptime = 0;
        }

        if (!writeStatusPage(page)) {
            // Обмен сорвался (модуль потерял питание, контроллер дисплея сбросился):
            // возвращаемся к инициализации, экран без неё остаётся пустым.
            ESP_LOGW(TAG, "LCD1602 frame write failed; reinitializing");
            ready = false;
            retryDelay = kInitRetryDelay;
            vTaskDelay(kInitRetryDelay);
            continue;
        }

        page = (page + 1) % kLcdRows;
        vTaskDelay(kRefreshInterval);
        uptime += kRefreshInterval;

        if (uptime >= kReinitInterval) {
            ready = false;
            uptime = 0;
        }
    }
    clearDisplay();
    ESP_LOGI(TAG, "LCD1602 worker stopped");
}

bool Lcd1602Subsystem::writeStatusPage(const uint32_t pageIndex) {
    if (_bus == nullptr) {
        return false;
    }
    I2CBusSubsystem::Guard bus(*_bus);
    TwoWire* wire = _bus->getWire();
    Lcd1602Pcf8574 lcd;
    lcd.attach(wire, _addr);

    if (!lcd.probe()) {
        AlarmMonitor::getInstance().raiseHardwareFault(HardwareFaultCode::I2C_NACK, _addr, "lcd1602");
        return false;
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

    bool ok = lcd.setCursor(0, 0);
    ok = lcd.printPadded(line0) && ok;
    ok = lcd.setCursor(0, 1) && ok;
    ok = lcd.printPadded(line1) && ok;
    return ok;
}

void Lcd1602Subsystem::clearDisplay() {
    if (_bus == nullptr || !_bus->isActive()) {
        return;
    }
    I2CBusSubsystem::Guard bus(*_bus);
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
