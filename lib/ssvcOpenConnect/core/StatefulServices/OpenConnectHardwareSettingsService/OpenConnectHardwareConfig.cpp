#include "OpenConnectHardwareConfig.h"

#include "core/GlobalConfig/GlobalConfig.h"

#include <unordered_map>
#include <unordered_set>

namespace {

bool relayAddrVecEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

bool rebootFieldsDiffer(const OpenConnectHardwareConfig& a, const OpenConnectHardwareConfig& b) {
    return a.pressureSensorEnabled != b.pressureSensorEnabled || a.bmp581I2cAddress != b.bmp581I2cAddress ||
           a.userRelayPcfEnabled != b.userRelayPcfEnabled || !relayAddrVecEqual(a.relayPcf8574Addresses, b.relayPcf8574Addresses) ||
           a.rtcEnabled != b.rtcEnabled || a.ds3231I2cAddress != b.ds3231I2cAddress ||
           a.oledEnabled != b.oledEnabled;
}

void syncSubsystemNvsAtmSensor(bool pressureOn) {
    std::unordered_map<std::string, bool> map;
    if (!GlobalConfig::config().getObject("subsystem", "config", map, GlobalConfig::fromJson)) {
        map.clear();
    }
    map["atm_sensor"] = pressureOn;
    GlobalConfig::config().setObject("subsystem", "config", map, GlobalConfig::toJson);
}

bool validateAddressList(std::vector<uint8_t>& addrs, bool& changed, const std::vector<uint8_t>& beforeVec) {
    if (addrs.empty()) {
        return false;
    }
    if (addrs.size() > OpenConnectHardwareConfig::kMaxRelayChips) {
        return false;
    }
    std::unordered_set<uint8_t> uniq;
    for (uint8_t a : addrs) {
        if (!OpenConnectHardwareConfig::isValidI2c7Bit(a)) {
            return false;
        }
        if (uniq.count(a)) {
            return false;
        }
        uniq.insert(a);
    }
    if (!relayAddrVecEqual(addrs, beforeVec)) {
        changed = true;
    }
    return true;
}

} // namespace

void OpenConnectHardwareConfig::read(OpenConnectHardwareConfig& s, JsonObject& root) {
    root["schemaVersion"] = s.schemaVersion;
    root["pressureSensorEnabled"] = s.pressureSensorEnabled;
    root["bmp581I2cAddress"] = s.bmp581I2cAddress;
    root["userRelayPcfEnabled"] = s.userRelayPcfEnabled;
    {
        JsonArray arr = root["relayPcf8574Addresses"].to<JsonArray>();
        arr.clear();
        for (uint8_t a : s.relayPcf8574Addresses) {
            arr.add(a);
        }
    }
    root["pendingReboot"] = s.pendingReboot;
    root["rtcEnabled"] = s.rtcEnabled;
    root["ds3231I2cAddress"] = s.ds3231I2cAddress;
    root["oledEnabled"] = s.oledEnabled;
}

StateUpdateResult OpenConnectHardwareConfig::update(JsonObject& root, OpenConnectHardwareConfig& s, const String& originId) {
    const OpenConnectHardwareConfig before = s;
    bool changed = false;

    // Migrate legacy JSON: single relayPcf8574I2cAddress once (no relayPcf8574Addresses array in file)
    if (root["relayPcf8574I2cAddress"].is<unsigned int>() && !root["relayPcf8574Addresses"].is<JsonArray>()) {
        const uint8_t legacy = static_cast<uint8_t>(root["relayPcf8574I2cAddress"].as<unsigned int>());
        if (isValidI2c7Bit(legacy)) {
            s.relayPcf8574Addresses.clear();
            s.relayPcf8574Addresses.push_back(legacy);
            changed = true;
        }
    }

    if (root["pressureSensorEnabled"].is<bool>()) {
        const bool v = root["pressureSensorEnabled"].as<bool>();
        if (v != s.pressureSensorEnabled) {
            s.pressureSensorEnabled = v;
            changed = true;
        }
    }

    if (!root["bmp581I2cAddress"].isNull()) {
        if (!root["bmp581I2cAddress"].is<unsigned int>()) {
            return StateUpdateResult::ERROR;
        }
        const uint8_t addr = static_cast<uint8_t>(root["bmp581I2cAddress"].as<unsigned int>());
        if (!isValidI2c7Bit(addr)) {
            return StateUpdateResult::ERROR;
        }
        if (addr != s.bmp581I2cAddress) {
            s.bmp581I2cAddress = addr;
            changed = true;
        }
    }

    if (root["userRelayPcfEnabled"].is<bool>()) {
        const bool v = root["userRelayPcfEnabled"].as<bool>();
        if (v != s.userRelayPcfEnabled) {
            s.userRelayPcfEnabled = v;
            changed = true;
        }
    }

    if (root["relayPcf8574Addresses"].is<JsonArray>()) {
        JsonArray ja = root["relayPcf8574Addresses"].as<JsonArray>();
        std::vector<uint8_t> next;
        for (JsonVariant v : ja) {
            if (!v.is<unsigned int>()) {
                return StateUpdateResult::ERROR;
            }
            next.push_back(static_cast<uint8_t>(v.as<unsigned int>()));
        }
        std::vector<uint8_t> beforeRelay = s.relayPcf8574Addresses;
        bool listChanged = false;
        if (!validateAddressList(next, listChanged, beforeRelay)) {
            return StateUpdateResult::ERROR;
        }
        s.relayPcf8574Addresses = std::move(next);
        if (listChanged) {
            changed = true;
        }
    }

    if (root["rtcEnabled"].is<bool>()) {
        const bool v = root["rtcEnabled"].as<bool>();
        if (v != s.rtcEnabled) {
            s.rtcEnabled = v;
            changed = true;
        }
    }

    if (!root["ds3231I2cAddress"].isNull()) {
        if (!root["ds3231I2cAddress"].is<unsigned int>()) {
            return StateUpdateResult::ERROR;
        }
        const uint8_t addr = static_cast<uint8_t>(root["ds3231I2cAddress"].as<unsigned int>());
        if (!isValidI2c7Bit(addr)) {
            return StateUpdateResult::ERROR;
        }
        if (addr != s.ds3231I2cAddress) {
            s.ds3231I2cAddress = addr;
            changed = true;
        }
    }

    if (root["oledEnabled"].is<bool>()) {
        const bool v = root["oledEnabled"].as<bool>();
        if (v != s.oledEnabled) {
            s.oledEnabled = v;
            changed = true;
        }
    }

    (void)originId;

    if (s.relayPcf8574Addresses.empty()) {
        s.relayPcf8574Addresses.push_back(0x24);
        changed = true;
    }

    if (changed && rebootFieldsDiffer(before, s)) {
        s.pendingReboot = true;
    }

    if (changed && before.pressureSensorEnabled != s.pressureSensorEnabled) {
        syncSubsystemNvsAtmSensor(s.pressureSensorEnabled);
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}
