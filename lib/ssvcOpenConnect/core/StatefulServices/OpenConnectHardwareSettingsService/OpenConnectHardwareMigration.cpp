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

/**
 * Сохраняет прежний файл рядом, прежде чем заменить его настройками по умолчанию.
 *
 * Без этого несоответствие валидации (например, новое правило, появившееся вместе с
 * обновлением прошивки) молча стирало пользователю адреса и включённость всего
 * оборудования: оставался только отказ устройства в работе.
 */
void keepBackup(FS* fs, const char* path) {
    if (!fs->exists(path)) {
        return;
    }
    const String backup = String(path) + ".bak";
    if (fs->exists(backup.c_str())) {
        fs->remove(backup.c_str());
    }
    if (!fs->rename(path, backup.c_str())) {
        ESP_LOGW(TAG, "Failed to keep a backup of %s", path);
        return;
    }
    ESP_LOGW(TAG, "Previous hardware config kept as %s", backup.c_str());
}

bool fileLooksValid(FS* fs, JsonDocument& doc, const char** reason = nullptr) {
    const auto fail = [&reason](const char* why) {
        if (reason != nullptr) {
            *reason = why;
        }
        return false;
    };

    File f = fs->open(FILE_PATH, "r");
    if (!f) {
        return fail("file not found");
    }
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err != DeserializationError::Ok || !doc.is<JsonObject>()) {
        return fail("not a JSON object");
    }
    const JsonObject o = doc.as<JsonObject>();
    const uint32_t ver = o["schemaVersion"] | 0;
    if (ver < 1) {
        return fail("schemaVersion missing");
    }
    const unsigned int bmp = o["bmp581I2cAddress"] | 0;
    if (!OpenConnectHardwareConfig::isValidI2c7Bit(bmp)) {
        return fail("bmp581I2cAddress out of 7-bit range");
    }
    const unsigned int rtc = o["ds3231I2cAddress"] | 0x68;
    if (!OpenConnectHardwareConfig::isValidI2c7Bit(rtc)) {
        return fail("ds3231I2cAddress out of 7-bit range");
    }
    const unsigned int lcd = o["lcd1602I2cAddress"] | 0x27;
    if (!OpenConnectHardwareConfig::isValidI2c7Bit(lcd)) {
        return fail("lcd1602I2cAddress out of 7-bit range");
    }
    // XKC-25 fields are optional: a config written before the sensors existed has none of them.
    if (!o["xkc25Sensors"].isNull()) {
        if (!o["xkc25Sensors"].is<JsonArray>()) {
            return fail("xkc25Sensors is not an array");
        }
        const JsonArray sensors = o["xkc25Sensors"].as<JsonArray>();
        if (sensors.size() > OpenConnectHardwareConfig::kMaxXkc25Sensors) {
            return fail("too many xkc25Sensors");
        }
        std::unordered_set<uint8_t> usedPins;
        for (JsonVariant v : sensors) {
            if (!v.is<JsonObject>()) {
                return fail("xkc25Sensors entry is not an object");
            }
            const JsonObject sensor = v.as<JsonObject>();
            const unsigned int pin = sensor["gpio"] | 0;
            if (!OpenConnectHardwareConfig::isValidGpio(pin)) {
                return fail("xkc25Sensors gpio out of range");
            }
            if (!OpenConnectHardwareConfig::isValidZoneIndex(sensor["zoneIndex"] | 0)) {
                return fail("xkc25Sensors zoneIndex out of range");
            }
            if (!usedPins.insert(static_cast<uint8_t>(pin)).second) {
                return fail("xkc25Sensors gpio duplicated");
            }
        }
    }
    if (o["relayPcf8574Addresses"].is<JsonArray>()) {
        const JsonArray ar = o["relayPcf8574Addresses"].as<JsonArray>();
        if (ar.isNull() || ar.size() < 1 || ar.size() > OpenConnectHardwareConfig::kMaxRelayChips) {
            return fail("relayPcf8574Addresses count out of range");
        }
        std::unordered_set<uint8_t> seen;
        for (JsonVariant v : ar) {
            if (!v.is<unsigned int>()) {
                return fail("relayPcf8574Addresses entry is not a number");
            }
            const unsigned int raw = v.as<unsigned int>();
            if (!OpenConnectHardwareConfig::isValidI2c7Bit(raw)) {
                return fail("relayPcf8574Addresses entry out of 7-bit range");
            }
            const uint8_t a = static_cast<uint8_t>(raw);
            if (seen.count(a)) {
                return fail("relayPcf8574Addresses entry duplicated");
            }
            seen.insert(a);
        }
        return true;
    }
    if (o["relayPcf8574I2cAddress"].is<unsigned int>()) {
        const unsigned int legacy = o["relayPcf8574I2cAddress"].as<unsigned int>();
        if (!OpenConnectHardwareConfig::isValidI2c7Bit(legacy)) {
            return fail("relayPcf8574I2cAddress out of 7-bit range");
        }
        return true;
    }
    return fail("no relay address in config");
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
    const char* reason = nullptr;
    if (fileLooksValid(fs, probe, &reason)) {
        ESP_LOGI(TAG, "Hardware config present and valid, skipping migration");
        return;
    }

    ESP_LOGW(TAG, "Hardware config rejected (%s): writing defaults", (reason != nullptr) ? reason : "unknown");
    keepBackup(fs, FILE_PATH);

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
