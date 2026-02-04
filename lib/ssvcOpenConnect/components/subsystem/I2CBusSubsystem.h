#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <Wire.h>
#include <esp_log.h>
#include "core/SubsystemManager/SubsystemManager.h"

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

    // --- Методы Subsystem ---
    void initialize() override {
        if (!_initialized) {
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
            // Пины 8 (SDA) и 9 (SCL), частота 10кГц для стабильности на длинных проводах
            if (Wire.begin(8, 9, 10000)) {
                _enabled = true;
                ESP_LOGI(TAG, "I2C Bus hardware started successfully");
                if (scan() == 0) {
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
        if (!_enabled) {
            ESP_LOGW(TAG, "I2C bus is not active. Cannot scan.");
            return 0;
        }

        int device_count = 0;
        ESP_LOGI(TAG, "Scanning I2C bus...");
        for (uint8_t i = 1; i < 127; i++) {
            Wire.beginTransmission(i);
            const uint8_t error = Wire.endTransmission();
            if (error == 0) {
                ESP_LOGI(TAG, "I2C device found at address 0x%02X", i);
                device_count++;
            }
        }
        ESP_LOGI(TAG, "Scan complete. Found %d devices.", device_count);
        return device_count;
    }

private:
    I2CBusSubsystem() = default; // Приватный конструктор для Singleton
    
    static constexpr auto TAG = "I2CBus";
    bool _initialized = false;
    bool _enabled = false;
};

#endif // I2C_BUS_H