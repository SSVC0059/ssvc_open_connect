#ifndef OPEN_CONNECT_HARDWARE_CONFIG_H
#define OPEN_CONNECT_HARDWARE_CONFIG_H

/**
 *   SSVC Open Connect — persisted hardware layout (LittleFS JSON).
 */

#include <ArduinoJson.h>
#include <vector>

#include "StatefulService.h"

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

    static bool isValidI2c7Bit(uint8_t addr) { return addr >= 0x08 && addr <= 0x77; }

    static constexpr size_t kMaxRelayChips = 8;

    /** Serialize current state into JSON object (StatefulService reader). */
    static void read(OpenConnectHardwareConfig& s, JsonObject& root);

    /** Merge partial JSON into state; validates I²C addresses. */
    static StateUpdateResult update(JsonObject& root, OpenConnectHardwareConfig& s, const String& originId);
};

#endif
