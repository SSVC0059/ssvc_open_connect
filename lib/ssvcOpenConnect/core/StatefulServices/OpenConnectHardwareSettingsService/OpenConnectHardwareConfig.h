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
    bool oledEnabled = false;

    static bool isValidI2c7Bit(uint8_t addr) { return addr >= 0x08 && addr <= 0x77; }

    static constexpr size_t kMaxRelayChips = 8;

    /** Serialize current state into JSON object (StatefulService reader). */
    static void read(OpenConnectHardwareConfig& s, JsonObject& root);

    /** Merge partial JSON into state; validates I²C addresses. */
    static StateUpdateResult update(JsonObject& root, OpenConnectHardwareConfig& s, const String& originId);
};

#endif
