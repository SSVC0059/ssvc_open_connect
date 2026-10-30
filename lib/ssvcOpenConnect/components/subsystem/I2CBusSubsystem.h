#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <Wire.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>
#include "core/SubsystemManager/SubsystemManager.h"
#include "core/SsvcConnector.h"
#if !PINOUT_USE_GPIO
#include "core/AlarmMonitor/AlarmMonitor.h"
#endif

#ifndef SSVC_I2C_SDA_GPIO
#define SSVC_I2C_SDA_GPIO 8
#endif
#ifndef SSVC_I2C_SCL_GPIO
#define SSVC_I2C_SCL_GPIO 9
#endif

class I2CBusSubsystem final : public Subsystem {
public:
    // --- Singleton Access ---
    static I2CBusSubsystem& getInstance() {
        static I2CBusSubsystem instance;
        return instance;
    }

    I2CBusSubsystem(const I2CBusSubsystem&) = delete;
    I2CBusSubsystem& operator=(const I2CBusSubsystem&) = delete;

    static std::string getName() { return "i2c_bus"; }

    /**
     * @brief RAII-защита шины I2C.
     *
     * Шину Wire делят между собой несколько задач: рабочий поток дисплея,
     * опрос RTC и PCF8574, HTTP-проверки из API и датчики. Без мьютекса
     * транзакции перемешиваются (один потребитель начинает передачу, другой
     * вклинивается между beginTransmission() и endTransmission()), и устройство
     * получает мусорные байты, включая команды расширителя PCF8574.
     *
     * Мьютекс рекурсивный: вложенные вызовы (enable() -> scan(), или проверка
     * дисплея внутри уже защищённой операции) не блокируются.
     *
     * Пример:
     * @code
     * I2CBusSubsystem::Guard bus(I2CBusSubsystem::getInstance());
     * if (TwoWire* w = bus.wire()) { ... }
     * @endcode
     */
    class Guard {
    public:
        explicit Guard(I2CBusSubsystem& bus);
        ~Guard();

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        /** Wire при активной шине, иначе nullptr. */
        TwoWire* wire() const { return _bus.getWire(); }
        bool active() const { return _bus.isActive(); }

    private:
        I2CBusSubsystem& _bus;
    };

    void lock();
    void unlock();

    // --- Методы Subsystem ---
    void initialize() override {
        if (!_initialized) {
            if (_mutex == nullptr) {
                _mutex = xSemaphoreCreateRecursiveMutex();
            }
            ESP_LOGI(TAG, "I2C Bus Subsystem initialized (Wait for enable)");
            _initialized = true;
        }
    }

    void enable() override {
        if (!_initialized) {
            ESP_LOGE(TAG, "Cannot enable: Subsystem not initialized");
            return;
        }

        if (!_enabled) {
            Guard guard(*this);
            // Defaults 8/9 (devkit). KinCony KC868-A6 v6: SSVC_I2C_SDA_GPIO=12, SSVC_I2C_SCL_GPIO=11
            if (Wire.begin(SSVC_I2C_SDA_GPIO, SSVC_I2C_SCL_GPIO, 100000)) {
                _enabled = true;
                ESP_LOGI(TAG, "I2C Bus hardware started (SDA=%d SCL=%d)",
                         SSVC_I2C_SDA_GPIO, SSVC_I2C_SCL_GPIO);
                if (scan() == 0) {
#if !PINOUT_USE_GPIO
                    AlarmMonitor::getInstance().raiseHardwareFault(
                        HardwareFaultCode::I2C_BUS_DOWN, 0, "i2c_bus");
#endif
                    ESP_LOGW(TAG, "No devices found on I2C bus. Disabling subsystem.");
                    SubsystemManager::instance().disableSubsystem(getName());
                }
            } else {
                ESP_LOGE(TAG, "Failed to start I2C Bus hardware");
            }
        }
    }

    void disable() override {
        if (_enabled) {
            ESP_LOGI(TAG, "I2C Bus hardware stopping...");
            // На ESP32 Wire.end() освобождает ресурсы и отключает пины
            // Wire.end(); 
            _enabled = false;
        }
    }

    // --- Ресурсный метод ---
    /**
     * @brief Возвращает указатель на объект Wire, если шина активна.
     * Используется датчиками и другими устройствами.
     */
    TwoWire* getWire() const {
        return _enabled ? &Wire : nullptr;
    }

    bool isActive() const { return _enabled; }

    /**
     * @brief Сканирует шину I2C и возвращает количество найденных устройств.
     */
    int scan() const {
        const std::vector<uint8_t> found = scanAddresses(true);
        return static_cast<int>(found.size());
    }

    /**
     * @brief Опрашивает адреса 1..126 и возвращает те, что подтвердили обращение.
     *
     * Адрес в списке означает, что устройство ответило ACK прямо сейчас: по нему
     * видно, выпало конкретное устройство или вся шина (диагностика через
     * /rest/oc/hardware-faults, без анализатора).
     *
     * @param verbose логировать каждый найденный адрес (для HTTP-диагностики шумно).
     */
    std::vector<uint8_t> scanAddresses(const bool verbose) const {
        std::vector<uint8_t> found;
        if (!_enabled) {
            if (verbose) {
                ESP_LOGW(TAG, "I2C bus is not active. Cannot scan.");
            }
            return found;
        }

        Guard guard(I2CBusSubsystem::getInstance());
        if (verbose) {
            ESP_LOGI(TAG, "Scanning I2C bus...");
        }
        for (uint8_t i = 1; i < 127; i++) {
            Wire.beginTransmission(i);
            const uint8_t error = Wire.endTransmission();
            if (error == 0) {
                found.push_back(i);
                if (verbose) {
                    ESP_LOGI(TAG, "I2C device found at address 0x%02X", i);
                }
            }
        }
        if (verbose) {
            ESP_LOGI(TAG, "Scan complete. Found %u devices.", static_cast<unsigned>(found.size()));
        }
        return found;
    }

private:
    I2CBusSubsystem() = default; // Приватный конструктор для Singleton

    static constexpr auto TAG = "I2CBus";
    bool _initialized = false;
    bool _enabled = false;
    SemaphoreHandle_t _mutex = nullptr;
};

inline I2CBusSubsystem::Guard::Guard(I2CBusSubsystem& bus) : _bus(bus) { _bus.lock(); }
inline I2CBusSubsystem::Guard::~Guard() { _bus.unlock(); }

inline void I2CBusSubsystem::lock() {
    if (_mutex != nullptr) {
        xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    }
}

inline void I2CBusSubsystem::unlock() {
    if (_mutex != nullptr) {
        xSemaphoreGiveRecursive(_mutex);
    }
}

#endif // I2C_BUS_H