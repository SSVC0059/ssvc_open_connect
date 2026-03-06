#pragma once

/**
*   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive SvelteKit web interface for
 *   monitoring and controlling the distillation process.
 *   https://github.com/SSVC0059/ssvc_open_connect
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 *
 *   This software is independent and not affiliated with SSVC0059 company.
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *
 *   Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/

#include <Preferences.h>
#include <ArduinoJson.h>
#include <climits>
#include <mutex>
#include <type_traits>
#include <unordered_map>

/*
Пример структуры с сериализацией:
struct WiFiSettings {
    String ssid;
    String password;
};

void toJson(const WiFiSettings& cfg, JsonObject& json) {
    json["ssid"] = cfg.ssid;
    json["password"] = cfg.password;
}

void fromJson(const JsonObject& json, WiFiSettings& cfg) {
    cfg.ssid = json["ssid"] | "";
    cfg.password = json["password"] | "";
}

// Использование
// Сохраняем структуру
    WiFiSettings wifi = {"MySSID", "MyPass"};
    GlobalConfig::getInstance().setObject("wifi", "config", wifi, toJson);

// Загружаем
    WiFiSettings restored;
if (GlobalConfig::getInstance().getObject("wifi", "config", restored, fromJson)) {
    Serial.println("SSID: " + restored.ssid);
}

// Сохраняем настройки
    GlobalConfig::config().set("app", "counter", 0);          // int
    GlobalConfig::config().set("app", "speed", 2.5f);         // float
    GlobalConfig::config().set("app", "active", true);        // bool
    GlobalConfig::config().set("app", "name", "ESP32-CONFIG"); // String
    GlobalConfig::config().set("test", "timestamp", 9223372036854775807LL);

    // Читаем настройки
    int counter = GlobalConfig::config().get("app", "counter", -1);
    float speed = GlobalConfig::config().get("app", "speed", 1.0f);
    bool active = GlobalConfig::config().get("app", "active", false);
    String name = GlobalConfig::config().get("app", "name", "default");
    int64_t restored = GlobalConfig::config().get("test", "timestamp", 0LL);


*/
class GlobalConfig {
public:
    static GlobalConfig& config() {
        static GlobalConfig instance;
        return instance;
    }

    // Удалённые копия и присваивание — должны быть public в C++14
    GlobalConfig(const GlobalConfig&) = delete;
    GlobalConfig& operator=(const GlobalConfig&) = delete;

    // --- Примитивы: чтение ---
    template<typename T>
    typename std::enable_if<std::is_same<T, int>::value, T>::type
    get(const String& ns, const String& key, T defaultValue) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (!prefs.begin(ns.c_str(), true)) return defaultValue;
        T val = prefs.getInt(key.c_str(), defaultValue);
        prefs.end();
        return val;
    }

    template<typename T>
    typename std::enable_if<std::is_same<T, float>::value, T>::type
    get(const String& ns, const String& key, T defaultValue) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (!prefs.begin(ns.c_str(), true)) return defaultValue;
        T val = prefs.getFloat(key.c_str(), defaultValue);
        prefs.end();
        return val;
    }

    template<typename T>
    typename std::enable_if<std::is_same<T, bool>::value, T>::type
    get(const String& ns, const String& key, T defaultValue) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (!prefs.begin(ns.c_str(), true)) return defaultValue;
        T val = prefs.getBool(key.c_str(), defaultValue);
        prefs.end();
        return val;
    }

    template<typename T>
    typename std::enable_if<std::is_same<T, String>::value, T>::type
    get(const String& ns, const String& key, const T& defaultValue) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (!prefs.begin(ns.c_str(), true)) return defaultValue;
        T val = prefs.getString(key.c_str(), defaultValue);
        prefs.end();
        return val;
    }

    // --- Примитивы: запись ---
    template<typename T>
    typename std::enable_if<std::is_same<T, int>::value>::type
    set(const String& ns, const String& key, T value) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            if (prefs.getInt(key.c_str(), INT32_MIN) != value) {  // INT32_MIN — гарантировано "не сохранено"
                prefs.putInt(key.c_str(), value);
            }
            prefs.end();
        }
    }

    template<typename T>
    typename std::enable_if<std::is_same<T, float>::value>::type
    set(const String& ns, const String& key, T value) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            if (prefs.getFloat(key.c_str(), NAN) != value) {  // NAN — гарантировано "не сохранено"
                prefs.putFloat(key.c_str(), value);
            }
            prefs.end();
        }
    }

    template<typename T>
    typename std::enable_if<std::is_same<T, bool>::value>::type
    set(const String& ns, const String& key, T value) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            if (prefs.getBool(key.c_str(), false) != value) {
                prefs.putBool(key.c_str(), value);
            }
            prefs.end();
        }
    }

    template<typename T>
    typename std::enable_if<std::is_same<T, String>::value>::type
    set(const String& ns, const String& key, const T& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            if (prefs.getString(key.c_str(), "") != value) {
                prefs.putString(key.c_str(), value);
            }
            prefs.end();
        }
    }

    template<typename T>
       typename std::enable_if<std::is_same<T, int64_t>::value, T>::type
       get(const String& ns, const String& key, T defaultValue) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (!prefs.begin(ns.c_str(), true)) return defaultValue;
        // Используем getLongLong для int64_t
        T val = prefs.getLong64(key.c_str(), defaultValue);
        prefs.end();
        return val;
    }

    // --- Запись для int64_t ---
    template<typename T>
    typename std::enable_if<std::is_same<T, int64_t>::value>::type
    set(const String& ns, const String& key, T value) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            // Сравниваем с минимальным значением int64_t как индикатором "не сохранено"
            if (prefs.getLong64(key.c_str(), LLONG_MIN) != value) {
                prefs.putLong64(key.c_str(), value);
            }
            prefs.end();
        }
    }

    template<typename T>
        typename std::enable_if<std::is_same<T, char*>::value, String>::type
        get(const String& ns, const String& key, const char* defaultValue) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (!prefs.begin(ns.c_str(), true)) return String(defaultValue);

        String result = prefs.getString(key.c_str(), defaultValue);
        prefs.end();
        return result;
    }

    // --- Запись для char* ---
    template<typename T>
    typename std::enable_if<std::is_same<T, char*>::value>::type
    set(const String& ns, const String& key, const char* value) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            const String current = prefs.getString(key.c_str(), "");
            if (current != value) {
                prefs.putString(key.c_str(), value);
            }
            prefs.end();
        }
    }

    void set(const String& ns, const String& key, const char* value) {
        set<String>(ns, key, String(value));
    }

    // --- Сериализация объекта ---
template<typename T>
void setObject(const String& ns, const String& key, const T& obj, void (*toJson)(const T&, JsonObject&)) {
    std::lock_guard<std::mutex> lock(_mutex);
    ESP_LOGD("Preferences", "Setting object in namespace '%s' with key '%s'", ns.c_str(), key.c_str());

    Preferences prefs;
    if (!prefs.begin(ns.c_str(), false)) {
        ESP_LOGD("Preferences", "Failed to begin namespace '%s'", ns.c_str());
        return;
    }

    JsonDocument doc;
    auto json = doc.to<JsonObject>();
    toJson(obj, json);

    String newJsonStr;
    serializeJson(doc, newJsonStr);
    ESP_LOGD("Preferences", "Serialized JSON: %s", newJsonStr.c_str());

    const String existing = prefs.getString(key.c_str(), "");
    if (existing != newJsonStr) {
        ESP_LOGD("Preferences", "Value changed, updating preference");
        if (prefs.putString(key.c_str(), newJsonStr)) {
            ESP_LOGD("Preferences", "Successfully saved preference");
        } else {
            ESP_LOGD("Preferences", "Failed to save preference");
        }
    } else {
        ESP_LOGD("Preferences", "Value unchanged, skipping update");
    }

    prefs.end();
}

template<typename T>
bool getObject(const String& ns, const String& key, T& obj, void (*fromJson)(const JsonObject&, T&)) {
    std::lock_guard<std::mutex> lock(_mutex);
    ESP_LOGD("Preferences", "Getting object from namespace '%s' with key '%s'", ns.c_str(), key.c_str());

    Preferences prefs;
    if (!prefs.begin(ns.c_str(), true)) {
        ESP_LOGD("Preferences", "Failed to begin namespace '%s'", ns.c_str());
        return false;
    }

    String jsonStr = prefs.getString(key.c_str(), "");
    prefs.end();

    if (jsonStr.isEmpty()) {
        ESP_LOGD("Preferences", "No data found for key '%s'", key.c_str());
        return false;
    }

    ESP_LOGD("Preferences", "Retrieved JSON: %s", jsonStr.c_str());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error != DeserializationError::Ok) {
        ESP_LOGD("Preferences", "JSON deserialization failed: %s", error.c_str());
        return false;
    }

    const auto json = doc.as<JsonObject>();
    fromJson(json, obj);
    ESP_LOGD("Preferences", "Successfully parsed object");
    return true;
}

    // --- Удаление / очистка ---
    void remove(const String& ns, const String& key) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            prefs.remove(key.c_str());
            prefs.end();
        }
    }

    void clearNamespace(const String& ns) {
        std::lock_guard<std::mutex> lock(_mutex);
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            prefs.clear();
            prefs.end();
        }
    }

    static void toJson(const std::unordered_map<std::string, bool>& map, ArduinoJson::JsonObject& json) {
        for (const auto& pair : map) {
            json[pair.first.c_str()] = pair.second;
        }
    }

    static void fromJson(const JsonObject& json, std::unordered_map<std::string, bool>& map) {
        map.clear();
        for (JsonPair kv : json) {
            map[kv.key().c_str()] = kv.value().as<bool>();
        }
    }

    static void toJson(const std::unordered_map<std::string, std::string>& map, ArduinoJson::JsonObject& json) {
        for (const auto& pair : map) {
            json[pair.first.c_str()] = pair.second;
        }
    }

    static void fromJson(const JsonObject& json, std::unordered_map<std::string, std::string>& map) {
        map.clear();
        for (JsonPair kv : json) {
            map[kv.key().c_str()] = kv.value().as<std::string>();
        }
    }

    static void toJson(const std::unordered_map<std::string, int64_t>& map, ArduinoJson::JsonObject& json) {
        for (const auto& pair : map) {
            json[pair.first.c_str()] = pair.second;
        }
    }

    static void fromJson(const JsonObject& json, std::unordered_map<std::string, int64_t>& map) {
        map.clear();
        for (JsonPair kv : json) {
            map[kv.key().c_str()] = kv.value().as<int64_t>();
        }
    }

private:
    GlobalConfig() = default;
    std::mutex _mutex;
};
