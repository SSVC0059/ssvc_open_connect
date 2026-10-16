#ifndef OPEN_CONNECT_HARDWARE_CONFIG_H
#define OPEN_CONNECT_HARDWARE_CONFIG_H

/**
 *   SSVC Open Connect — persisted hardware layout (LittleFS JSON).
 */

#include <ArduinoJson.h>
#include <vector>

#include "StatefulService.h"
#include "core/SsvcConnector.h"

/** Настройки одного датчика XKC-25-NPN (бесконтактный ёмкостный датчик уровня). */
struct Xkc25SensorConfig {
    bool enabled = false;           // датчик подключён и активен
    /**
     * Источник сигнала: 0 — нативный GPIO (поле gpio), 1 — бит входного расширителя
     * PCF8574 (поля pcfAddress/pcfBit). На KC868-A6 входные сухие контакты — на 0x22.
     * Значения должны совпадать с DigitalInputSourceKind (см. static_assert в Xkc25Sensor.cpp).
     */
    static constexpr uint8_t kPinSourceGpio = 0;
    static constexpr uint8_t kPinSourcePcf8574 = 1;
    uint8_t pinSource = kPinSourceGpio;
    uint8_t gpio = 0;               // GPIO-пин выхода OUT (pinSource == kPinSourceGpio)
    uint8_t pcfAddress = SSVC_XKC25_PCF8574_INPUT_ADDR; // 7-битный адрес входного расширителя (pinSource == kPinSourcePcf8574)
    uint8_t pcfBit = 0;             // бит 0..7 входного расширителя (pinSource == kPinSourcePcf8574)
    bool activeLow = true;          // true → «жидкость обнаружена» соответствует LOW на пине; результат калибровки
    uint8_t zoneIndex = 0;          // 0..2 → RECEIVER_TANK_1/2/3
    bool calibrated = false;        // калибровка пройдена и сохранена
    uint8_t levelWithoutLiquid = 1; // сырой уровень пина, когда жидкости напротив датчика НЕТ (0/1)
    uint8_t levelWithLiquid = 0;    // сырой уровень пина, когда жидкость напротив датчика ЕСТЬ (0/1)
    uint32_t calibratedAt = 0;      // unix-время последней калибровки (0 = никогда)

    /** Переносной датчик: фактическая зона — та ёмкость, на которой он стоит сейчас. */
    bool sharedAcrossTanks = false;

    static bool isValidPinSource(unsigned int v) { return v <= kPinSourcePcf8574; }

    /** Бит входного расширителя PCF8574 (0..7). */
    static bool isValidPcfBit(unsigned int bit) { return bit <= 7; }

    /**
     * @brief Сравнение полей подключения (без калибровочных).
     *
     * Изменение любого из них делает прежнюю калибровку недостоверной, поэтому
     * сравнивается именно эта группа (см. OpenConnectHardwareConfig::update).
     */
    static bool structurallyEqual(const Xkc25SensorConfig& a, const Xkc25SensorConfig& b) {
        return a.enabled == b.enabled && a.pinSource == b.pinSource && a.gpio == b.gpio &&
               a.pcfAddress == b.pcfAddress && a.pcfBit == b.pcfBit &&
               a.activeLow == b.activeLow && a.zoneIndex == b.zoneIndex;
    }
};

struct OpenConnectHardwareConfig {
    uint32_t schemaVersion = 1;

    bool pressureSensorEnabled = true;
    uint8_t bmp581I2cAddress = 0x47;

    bool userRelayPcfEnabled = false;
    /** Ordered list of PCF8574 7-bit addresses (one expander per entry). Max 8 chips (=64 lines). */
    std::vector<uint8_t> relayPcf8574Addresses = {0x24};

    /** Set when a reboot-required field changes via PUT; cleared on next boot after load. */
    bool pendingReboot = false;

    bool rtcEnabled = false;
    /** DS3231 default 7-bit address 0x68; stored UTC in chip. */
    uint8_t ds3231I2cAddress = 0x68;

    bool oledEnabled = false;
    bool lcd1602Enabled = false;
    uint8_t lcd1602I2cAddress = 0x27;

    /** XKC-25-NPN capacitive level sensors (0..kMaxXkc25Sensors), each on its own GPIO. */
    std::vector<Xkc25SensorConfig> xkc25Sensors;

    /** Global XKC-25 options shared by all sensors. */
    bool xkc25AutoPause = true;           // срабатывание → PAUSE отбора
    bool xkc25AbortRectification = false; // срабатывание → прерывание ректификации

    /** Режим реакции на срабатывание датчика: 0 = PAUSE, 1 = ABORT, 2 = NOTIFY. */
    uint8_t xkc25TriggerAction = 0;

    /** Ёмкость для переносного датчика (sharedAcrossTanks), 0..2. */
    uint8_t xkc25SharedTankIndex = 0;

    /** Режимы проверки перед стартом ректификации при некалиброванных датчиках. */
    static constexpr uint8_t kXkc25GuardOff = 0;   // проверок нет
    static constexpr uint8_t kXkc25GuardWarn = 1;  // старт разрешён, причина — в предупреждении
    static constexpr uint8_t kXkc25GuardBlock = 2; // старт отклоняется

    /** Режим реакции на срабатывание датчика. */
    static constexpr uint8_t kXkc25ActionPause = 0;  // PAUSE отбора
    static constexpr uint8_t kXkc25ActionAbort = 1;  // ABORT ректификации
    static constexpr uint8_t kXkc25ActionNotify = 2; // уведомление без остановки

    static bool isValidStartGuardMode(unsigned int mode) { return mode <= kXkc25GuardBlock; }

    uint8_t xkc25StartGuardMode = kXkc25GuardWarn;

    static bool isValidI2c7Bit(unsigned int addr) { return addr >= 0x08 && addr <= 0x77; }

    /** Valid range for a sensor GPIO on ESP32-S3 (0..39), excluding flash/PSRAM pins. */
    static bool isValidGpio(unsigned int pin) { return pin <= 39; }

    /** Zone index of an XKC-25 sensor maps to RECEIVER_TANK_1/2/3. */
    static bool isValidZoneIndex(unsigned int idx) { return idx <= 2; }

    /** Board default GPIO for the N-th XKC-25 slot (build flags SSVC_XKC25_GPIO_1/2/3). */
    static uint8_t defaultXkc25Gpio(size_t slot) {
        switch (slot) {
            case 0: return static_cast<uint8_t>(SSVC_XKC25_GPIO_1);
            case 1: return static_cast<uint8_t>(SSVC_XKC25_GPIO_2);
            default: return static_cast<uint8_t>(SSVC_XKC25_GPIO_3);
        }
    }

    static constexpr size_t kMaxRelayChips = 8;
    static constexpr size_t kMaxXkc25Sensors = 3;

    /** Serialize current state into JSON object (StatefulService reader). */
    static void read(OpenConnectHardwareConfig& s, JsonObject& root);

    /** Merge partial JSON into state; validates I²C addresses. */
    static StateUpdateResult update(JsonObject& root, OpenConnectHardwareConfig& s, const String& originId);
};

#endif
