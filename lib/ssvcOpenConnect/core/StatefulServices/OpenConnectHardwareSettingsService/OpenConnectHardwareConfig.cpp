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

/** Structural diff only: calibration fields are runtime data and never force a reboot. */
bool xkc25SensorsDiffer(const std::vector<Xkc25SensorConfig>& a, const std::vector<Xkc25SensorConfig>& b) {
    if (a.size() != b.size()) {
        return true;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (!Xkc25SensorConfig::structurallyEqual(a[i], b[i])) {
            return true;
        }
    }
    return false;
}

bool xkc25SensorsEqual(const std::vector<Xkc25SensorConfig>& a, const std::vector<Xkc25SensorConfig>& b) {
    if (xkc25SensorsDiffer(a, b)) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        // sharedAcrossTanks меняет поведение датчика (переносной), но калибровку не
        // затрагивает — поэтому входит в обнаружение изменений, но не в structurallyEqual.
        if (a[i].sharedAcrossTanks != b[i].sharedAcrossTanks ||
            a[i].calibrated != b[i].calibrated ||
            a[i].levelWithoutLiquid != b[i].levelWithoutLiquid ||
            a[i].levelWithLiquid != b[i].levelWithLiquid ||
            a[i].calibratedAt != b[i].calibratedAt) {
            return false;
        }
    }
    return true;
}

/**
 * Смена признака «переносной датчик» требует пересоздания датчика при включении
 * подсистемы, поэтому это reboot-поле — но калибровку оно не сбрасывает.
 */
bool xkc25SharedFlagsDiffer(const std::vector<Xkc25SensorConfig>& a,
                            const std::vector<Xkc25SensorConfig>& b) {
    if (a.size() != b.size()) {
        return true;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].sharedAcrossTanks != b[i].sharedAcrossTanks) {
            return true;
        }
    }
    return false;
}

bool anyXkc25Enabled(const std::vector<Xkc25SensorConfig>& sensors) {
    for (const auto& sensor : sensors) {
        if (sensor.enabled) {
            return true;
        }
    }
    return false;
}

bool rebootFieldsDiffer(const OpenConnectHardwareConfig& a, const OpenConnectHardwareConfig& b) {
    return a.pressureSensorEnabled != b.pressureSensorEnabled || a.bmp581I2cAddress != b.bmp581I2cAddress ||
           a.userRelayPcfEnabled != b.userRelayPcfEnabled || !relayAddrVecEqual(a.relayPcf8574Addresses, b.relayPcf8574Addresses) ||
           a.rtcEnabled != b.rtcEnabled || a.ds3231I2cAddress != b.ds3231I2cAddress ||
           a.oledEnabled != b.oledEnabled ||
           a.lcd1602Enabled != b.lcd1602Enabled || a.lcd1602I2cAddress != b.lcd1602I2cAddress ||
           xkc25SensorsDiffer(a.xkc25Sensors, b.xkc25Sensors) ||
           xkc25SharedFlagsDiffer(a.xkc25Sensors, b.xkc25Sensors);
}

void syncSubsystemNvsAtmSensor(bool pressureOn) {
    std::unordered_map<std::string, bool> map;
    if (!GlobalConfig::config().getObject("subsystem", "config", map, GlobalConfig::fromJson)) {
        map.clear();
    }
    map["atm_sensor"] = pressureOn;
    GlobalConfig::config().setObject("subsystem", "config", map, GlobalConfig::toJson);
}

void syncSubsystemNvsLcd1602(bool lcdOn) {
    std::unordered_map<std::string, bool> map;
    if (!GlobalConfig::config().getObject("subsystem", "config", map, GlobalConfig::fromJson)) {
        map.clear();
    }
    map["lcd1602_display"] = lcdOn;
    GlobalConfig::config().setObject("subsystem", "config", map, GlobalConfig::toJson);
}

void syncSubsystemNvsXkc25(bool xkc25On) {
    std::unordered_map<std::string, bool> map;
    if (!GlobalConfig::config().getObject("subsystem", "config", map, GlobalConfig::fromJson)) {
        map.clear();
    }
    map["xkc25_sensor"] = xkc25On;
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
    root["lcd1602Enabled"] = s.lcd1602Enabled;
    root["lcd1602I2cAddress"] = s.lcd1602I2cAddress;
    {
        JsonArray arr = root["xkc25Sensors"].to<JsonArray>();
        arr.clear();
        for (const auto& sensor : s.xkc25Sensors) {
            JsonObject o = arr.add<JsonObject>();
            o["enabled"] = sensor.enabled;
            o["pinSource"] = sensor.pinSource;
            o["gpio"] = sensor.gpio;
            o["pcfAddress"] = sensor.pcfAddress;
            o["pcfBit"] = sensor.pcfBit;
            o["activeLow"] = sensor.activeLow;
            o["zoneIndex"] = sensor.zoneIndex;
            o["calibrated"] = sensor.calibrated;
            o["levelWithoutLiquid"] = sensor.levelWithoutLiquid;
            o["levelWithLiquid"] = sensor.levelWithLiquid;
            o["calibratedAt"] = sensor.calibratedAt;
            o["sharedAcrossTanks"] = sensor.sharedAcrossTanks;
        }
    }
    root["xkc25AutoPause"] = s.xkc25AutoPause;
    root["xkc25AbortRectification"] = s.xkc25AbortRectification;
    root["xkc25TriggerAction"] = s.xkc25TriggerAction;
    root["xkc25SharedTankIndex"] = s.xkc25SharedTankIndex;
    root["xkc25StartGuardMode"] = s.xkc25StartGuardMode;
}

StateUpdateResult OpenConnectHardwareConfig::update(JsonObject& root, OpenConnectHardwareConfig& s, const String& originId) {
    const OpenConnectHardwareConfig before = s;
    auto fail = [&]() {
        s = before;
        return StateUpdateResult::ERROR;
    };
    bool changed = false;

    // Migrate legacy JSON: single relayPcf8574I2cAddress once (no relayPcf8574Addresses array in file)
    if (root["relayPcf8574I2cAddress"].is<unsigned int>() && !root["relayPcf8574Addresses"].is<JsonArray>()) {
        const unsigned int legacyRaw = root["relayPcf8574I2cAddress"].as<unsigned int>();
        if (isValidI2c7Bit(legacyRaw)) {
            const uint8_t legacy = static_cast<uint8_t>(legacyRaw);
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
            return fail();
        }
        const unsigned int rawAddr = root["bmp581I2cAddress"].as<unsigned int>();
        if (!isValidI2c7Bit(rawAddr)) {
            return fail();
        }
        const uint8_t addr = static_cast<uint8_t>(rawAddr);
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
                return fail();
            }
            const unsigned int rawAddr = v.as<unsigned int>();
            if (!isValidI2c7Bit(rawAddr)) {
                return fail();
            }
            next.push_back(static_cast<uint8_t>(rawAddr));
        }
        std::vector<uint8_t> beforeRelay = s.relayPcf8574Addresses;
        bool listChanged = false;
        if (!validateAddressList(next, listChanged, beforeRelay)) {
            return fail();
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
            return fail();
        }
        const unsigned int rawAddr = root["ds3231I2cAddress"].as<unsigned int>();
        if (!isValidI2c7Bit(rawAddr)) {
            return fail();
        }
        const uint8_t addr = static_cast<uint8_t>(rawAddr);
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

    if (root["lcd1602Enabled"].is<bool>()) {
        const bool v = root["lcd1602Enabled"].as<bool>();
        if (v != s.lcd1602Enabled) {
            s.lcd1602Enabled = v;
            changed = true;
        }
    }

    if (!root["lcd1602I2cAddress"].isNull()) {
        if (!root["lcd1602I2cAddress"].is<unsigned int>()) {
            return fail();
        }
        const unsigned int rawAddr = root["lcd1602I2cAddress"].as<unsigned int>();
        if (!isValidI2c7Bit(rawAddr)) {
            return fail();
        }
        const uint8_t addr = static_cast<uint8_t>(rawAddr);
        if (addr != s.lcd1602I2cAddress) {
            s.lcd1602I2cAddress = addr;
            changed = true;
        }
    }

    if (!root["xkc25Sensors"].isNull()) {
        if (!root["xkc25Sensors"].is<JsonArray>()) {
            return fail();
        }
        const JsonArray ja = root["xkc25Sensors"].as<JsonArray>();
        if (ja.size() > kMaxXkc25Sensors) {
            return fail();
        }
        std::vector<Xkc25SensorConfig> next;
        // Уникальность источника сигнала: GPIO → 0x100|pin, PCF8574 → 0x200|(addr<<3)|bit.
        // Ключи разных диапазонов не пересекаются, поэтому GPIO и бит расширителя не «схлопываются».
        std::unordered_set<uint32_t> usedInputs;
        size_t slot = 0;
        for (JsonVariant v : ja) {
            if (!v.is<JsonObject>()) {
                return fail();
            }
            const JsonObject o = v.as<JsonObject>();
            Xkc25SensorConfig sensor;
            sensor.enabled = o["enabled"] | false;

            const unsigned int rawSource = o["pinSource"] | 0;
            if (!Xkc25SensorConfig::isValidPinSource(rawSource)) {
                return fail();
            }
            sensor.pinSource = static_cast<uint8_t>(rawSource);

            uint32_t inputKey = 0;
            if (sensor.pinSource == Xkc25SensorConfig::kPinSourcePcf8574) {
                const unsigned int rawAddr = o["pcfAddress"] | 0x22;
                if (!isValidI2c7Bit(rawAddr)) {
                    return fail();
                }
                sensor.pcfAddress = static_cast<uint8_t>(rawAddr);
                const unsigned int rawBit = o["pcfBit"] | 0;
                if (!Xkc25SensorConfig::isValidPcfBit(rawBit)) {
                    return fail();
                }
                sensor.pcfBit = static_cast<uint8_t>(rawBit);

                // Входной расширитель не может совпадать с релейным: prepareInputs() пишет
                // 0xFF в тот же чип, что и RelayPortCoordinator, — иначе конфликт записей.
                for (uint8_t relayAddr : s.relayPcf8574Addresses) {
                    if (relayAddr == sensor.pcfAddress) {
                        return fail();
                    }
                }
                inputKey =
                    0x200u | (static_cast<uint32_t>(sensor.pcfAddress) << 3) | sensor.pcfBit;
            } else {
                const unsigned int rawPin = o["gpio"] | 0;
                if (!isValidGpio(rawPin)) {
                    return fail();
                }
                sensor.gpio = static_cast<uint8_t>(rawPin);
                inputKey = 0x100u | sensor.gpio;
            }
            if (!usedInputs.insert(inputKey).second) {
                return fail();
            }

            sensor.activeLow = o["activeLow"] | true;
            const unsigned int rawZone = o["zoneIndex"] | 0;
            if (!isValidZoneIndex(rawZone)) {
                return fail();
            }
            sensor.zoneIndex = static_cast<uint8_t>(rawZone);
            sensor.sharedAcrossTanks = o["sharedAcrossTanks"] | false;

            // Calibration is runtime data produced by the calibration wizard: PUT can
            // never write it (see 6.1.3). Taking it from the live state keeps the
            // wizard's values from being overwritten by a stale snapshot from the UI.
            const Xkc25SensorConfig previous =
                slot < s.xkc25Sensors.size() ? s.xkc25Sensors[slot] : Xkc25SensorConfig{};
            sensor.calibrated = previous.calibrated;
            sensor.levelWithoutLiquid = previous.levelWithoutLiquid;
            sensor.levelWithLiquid = previous.levelWithLiquid;
            sensor.calibratedAt = previous.calibratedAt;

            // Rewiring invalidates a calibration measured on the old wiring.
            if (!Xkc25SensorConfig::structurallyEqual(sensor, previous)) {
                sensor.calibrated = false;
                sensor.calibratedAt = 0;
            }

            next.push_back(sensor);
            ++slot;
        }
        if (!xkc25SensorsEqual(s.xkc25Sensors, next)) {
            s.xkc25Sensors = std::move(next);
            changed = true;
        }
    }

    if (root["xkc25AutoPause"].is<bool>()) {
        const bool v = root["xkc25AutoPause"].as<bool>();
        if (v != s.xkc25AutoPause) {
            s.xkc25AutoPause = v;
            changed = true;
        }
    }

    if (root["xkc25AbortRectification"].is<bool>()) {
        const bool v = root["xkc25AbortRectification"].as<bool>();
        if (v != s.xkc25AbortRectification) {
            s.xkc25AbortRectification = v;
            changed = true;
        }
    }

    if (!root["xkc25TriggerAction"].isNull()) {
        if (!root["xkc25TriggerAction"].is<unsigned int>()) {
            return fail();
        }
        const unsigned int rawAction = root["xkc25TriggerAction"].as<unsigned int>();
        if (!isValidTriggerAction(rawAction)) {
            return fail();
        }
        const uint8_t action = static_cast<uint8_t>(rawAction);
        if (action != s.xkc25TriggerAction) {
            s.xkc25TriggerAction = action;
            changed = true;
        }
    }

    if (!root["xkc25SharedTankIndex"].isNull()) {
        if (!root["xkc25SharedTankIndex"].is<unsigned int>()) {
            return fail();
        }
        const unsigned int rawTank = root["xkc25SharedTankIndex"].as<unsigned int>();
        if (!isValidZoneIndex(rawTank)) {
            return fail();
        }
        const uint8_t tank = static_cast<uint8_t>(rawTank);
        if (tank != s.xkc25SharedTankIndex) {
            s.xkc25SharedTankIndex = tank;
            changed = true;
        }
    }

    if (!root["xkc25StartGuardMode"].isNull()) {
        if (!root["xkc25StartGuardMode"].is<unsigned int>()) {
            return fail();
        }
        const unsigned int rawMode = root["xkc25StartGuardMode"].as<unsigned int>();
        if (!isValidStartGuardMode(rawMode)) {
            return fail();
        }
        const uint8_t mode = static_cast<uint8_t>(rawMode);
        if (mode != s.xkc25StartGuardMode) {
            s.xkc25StartGuardMode = mode;
            changed = true;
        }
    } else if (root["xkc25BlockStartUntilCalibrated"].is<bool>()) {
        // Legacy boolean from configs written before the three-mode guard:
        // true meant "block the start", false meant "no check at all".
        const uint8_t mode = root["xkc25BlockStartUntilCalibrated"].as<bool>()
                                 ? kXkc25GuardBlock
                                 : kXkc25GuardOff;
        if (mode != s.xkc25StartGuardMode) {
            s.xkc25StartGuardMode = mode;
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
    if (changed && before.lcd1602Enabled != s.lcd1602Enabled) {
        syncSubsystemNvsLcd1602(s.lcd1602Enabled);
    }
    if (changed && anyXkc25Enabled(before.xkc25Sensors) != anyXkc25Enabled(s.xkc25Sensors)) {
        syncSubsystemNvsXkc25(anyXkc25Enabled(s.xkc25Sensors));
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}
