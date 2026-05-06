#include "OpenConnectHardwareMigration.h"

#include "OpenConnectHardwareConfig.h"

#include "core/GlobalConfig/GlobalConfig.h"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr const char* TAG = "OC_HW_MIGRATION";
constexpr const char* FILE_PATH = "/config/oc_hardware.json";

bool writeAtomic(FS* fs, const char* path, const String& json) {
    if (!fs) {
        ESP_LOGE(TAG, "writeAtomic: fs is null");
        return false;
    }
    const String tmp = String(path) + ".tmp";
    File out = fs->open(tmp.c_str(), "w");
    if (!out) {
        ESP_LOGE(TAG, "Failed to open temp file for hardware config");
        return false;
    }
    const size_t n = out.print(json);
    out.close();
    if (n != json.length()) {
        ESP_LOGE(TAG, "Short write hardware config (%u vs %u)", static_cast<unsigned>(n), json.length());
        fs->remove(tmp.c_str());
        return false;
    }
    if (fs->exists(path)) {
        fs->remove(path);
    }
    if (!fs->rename(tmp.c_str(), path)) {
        ESP_LOGE(TAG, "rename failed for hardware config");
        fs->remove(tmp.c_str());
        return false;
    }
    return true;
}

bool fileLooksValid(FS* fs, JsonDocument& doc) {
    File f = fs->open(FILE_PATH, "r");
    if (!f) {
        return false;
    }
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err != DeserializationError::Ok || !doc.is<JsonObject>()) {
        return false;
    }
    const JsonObject o = doc.as<JsonObject>();
    const uint32_t ver = o["schemaVersion"] | 0;
    if (ver < 1) {
        return false;
    }
    const unsigned int bmp = o["bmp581I2cAddress"] | 0;
    if (!OpenConnectHardwareConfig::isValidI2c7Bit(bmp)) {
        return false;
    }
    const unsigned int rtc = o["ds3231I2cAddress"] | 0x68;
    if (!OpenConnectHardwareConfig::isValidI2c7Bit(rtc)) {
        return false;
    }
    const unsigned int lcd = o["lcd1602I2cAddress"] | 0x27;
    if (!OpenConnectHardwareConfig::isValidI2c7Bit(lcd)) {
        return false;
    }
    if (o["relayPcf8574Addresses"].is<JsonArray>()) {
        const JsonArray ar = o["relayPcf8574Addresses"].as<JsonArray>();
        if (ar.isNull() || ar.size() < 1 || ar.size() > OpenConnectHardwareConfig::kMaxRelayChips) {
            return false;
        }
        std::unordered_set<uint8_t> seen;
        for (JsonVariant v : ar) {
            if (!v.is<unsigned int>()) {
                return false;
            }
            const unsigned int raw = v.as<unsigned int>();
            if (!OpenConnectHardwareConfig::isValidI2c7Bit(raw)) {
                return false;
            }
            const uint8_t a = static_cast<uint8_t>(raw);
            if (seen.count(a)) {
                return false;
            }
            seen.insert(a);
        }
        return true;
    }
    if (o["relayPcf8574I2cAddress"].is<unsigned int>()) {
        const unsigned int legacy = o["relayPcf8574I2cAddress"].as<unsigned int>();
        return OpenConnectHardwareConfig::isValidI2c7Bit(legacy);
    }
    return false;
}

} // namespace

namespace OpenConnectHardwareMigration {

static void syncSubsystemNvs(bool pressureOn) {
    std::unordered_map<std::string, bool> map;
    if (!GlobalConfig::config().getObject("subsystem", "config", map, GlobalConfig::fromJson)) {
        map.clear();
    }
    map["atm_sensor"] = pressureOn;
    GlobalConfig::config().setObject("subsystem", "config", map, GlobalConfig::toJson);
}

void ensure(FS* fs) {
    if (!fs) {
        ESP_LOGE(TAG, "ensure: fs is null");
        return;
    }

    JsonDocument probe;
    if (fileLooksValid(fs, probe)) {
        ESP_LOGI(TAG, "Hardware config present and valid, skipping migration");
        return;
    }

    ESP_LOGW(TAG, "Migrating hardware config from NVS defaults");

    OpenConnectHardwareConfig cfg;

    std::unordered_map<std::string, bool> savedStates;
    if (GlobalConfig::config().getObject("subsystem", "config", savedStates, GlobalConfig::fromJson)) {
        const auto it = savedStates.find("atm_sensor");
        if (it != savedStates.end()) {
            cfg.pressureSensorEnabled = it->second;
        }
    }

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    OpenConnectHardwareConfig::read(cfg, root);

    String out;
    serializeJson(doc, out);
    if (!writeAtomic(fs, FILE_PATH, out)) {
        ESP_LOGE(TAG, "Atomic write failed; NVS unchanged — retry after fixing filesystem");
        return;
    }

    syncSubsystemNvs(cfg.pressureSensorEnabled);

    ESP_LOGI(TAG, "Migration wrote %s", FILE_PATH);
}

} // namespace OpenConnectHardwareMigration
