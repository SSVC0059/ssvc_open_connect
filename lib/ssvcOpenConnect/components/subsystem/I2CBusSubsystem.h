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
            // Мьютекс создаётся не здесь: ранний вызов из другой задачи не должен
            // увидеть nullptr и пройти мимо защиты шины.
            (void)recursiveMutex();
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
        const std::vector<uint8_t> found = scanAddresses(true, true);
        return static_cast<int>(found.size());
    }

    /**
     * @brief Опрашивает адреса 1..126 и возвращает те, что подтвердили обращение.
     *
     * Адрес в списке означает, что устройство ответило ACK прямо сейчас: по нему
     * видно, выпало конкретное устройство или вся шина (диагностика через
     * /rest/oc/hardware-faults, без анализатора).
     *
     * Результат кэшируется на SCAN_CACHE_TTL_MS: 126 транзакций под мьютексом
     * занимают до секунды (и больше, если устройство удерживает SDA), а запрос
     * диагностики не должен каждый раз блокировать дисплей, RTC и датчики.
     *
     * @param verbose логировать каждый найденный адрес (для HTTP-диагностики шумно).
     * @param forceRefresh игнорировать кэш (нужно при включении подсистемы).
     */
    std::vector<uint8_t> scanAddresses(const bool verbose, const bool forceRefresh = false) const {
        std::vector<uint8_t> found;
        if (!_enabled) {
            if (verbose) {
                ESP_LOGW(TAG, "I2C bus is not active. Cannot scan.");
            }
            return found;
        }

        Guard guard(I2CBusSubsystem::getInstance());

        const uint32_t now = millis();
        if (!forceRefresh && _scanCacheValid &&
            (now - _scanCacheAt) < SCAN_CACHE_TTL_MS) {
            if (verbose) {
                ESP_LOGD(TAG, "I2C scan taken from cache (%u devices)",
                         static_cast<unsigned>(_scanCache.size()));
            }
            return _scanCache;
        }

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
        _scanCache = found;
        _scanCacheAt = millis();
        _scanCacheValid = true;

        if (verbose) {
            ESP_LOGI(TAG, "Scan complete. Found %u devices.", static_cast<unsigned>(found.size()));
        }
        return found;
    }

private:
    I2CBusSubsystem() = default; // Приватный конструктор для Singleton

    /**
     * @brief Рекурсивный мьютекс шины, создаваемый при первом обращении.
     *
     * Статическая инициализация потокобезопасна (C++11), поэтому ни initialize(),
     * ни lock() не могут увидеть nullptr: ленивое создание под блокировкой само
     * требовало блокировки и оставляло окно, в котором защита шины отсутствовала.
     */
    static SemaphoreHandle_t recursiveMutex() {
        static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
        return mutex;
    }

    static constexpr auto TAG = "I2CBus";
    /// Кэш сканирования: 126 транзакций под мьютексом не должны выполняться на
    /// каждый запрос диагностики.
    static constexpr uint32_t SCAN_CACHE_TTL_MS = 15000;
    bool _initialized = false;
    bool _enabled = false;
    mutable std::vector<uint8_t> _scanCache;
    mutable uint32_t _scanCacheAt = 0;
    mutable bool _scanCacheValid = false;
};

inline I2CBusSubsystem::Guard::Guard(I2CBusSubsystem& bus) : _bus(bus) { _bus.lock(); }
inline I2CBusSubsystem::Guard::~Guard() { _bus.unlock(); }

inline void I2CBusSubsystem::lock() {
    const SemaphoreHandle_t busMutex = recursiveMutex();
    if (busMutex == nullptr) {
        // Без мьютекса шина не защищена: молчать об этом нельзя, но и работать
        // с нулевым дескриптором нельзя — FreeRTOS уйдёт в assert.
        ESP_LOGE(TAG, "I2C mutex create failed: bus access is unprotected");
        return;
    }
    xSemaphoreTakeRecursive(busMutex, portMAX_DELAY);
}

inline void I2CBusSubsystem::unlock() {
    const SemaphoreHandle_t busMutex = recursiveMutex();
    if (busMutex == nullptr) {
        return;
    }
    xSemaphoreGiveRecursive(busMutex);
}

#endif // I2C_BUS_H