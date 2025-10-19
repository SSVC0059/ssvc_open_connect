#ifndef SSVC_OPEN_CONNECT_ALARMTHRESHOLDS_H
#define SSVC_OPEN_CONNECT_ALARMTHRESHOLDS_H

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

#include <string>
#include <map>
#include <set>

#include "ArduinoJson.h"

#include "ESP32SvelteKit.h"
#include "StatefulService.h"

#include "components/sensors/SensorManager/SensorManager.h"

#define ALARM_SETTINGS_ENDPOINT "/rest/alarms"
#define ALARM_SETTINGS_FILE "/config/alarms.json"

// Уровни тревоги
enum class AlarmLevel {
    NORMAL,
    MIN,      // Минимум превышен
    DANGEROUS, // Опасный уровень превышен
    CRITICAL   // Критический уровень превышен
};

// Настройки порогов для одного датчика
struct ThresholdSettings {
    bool enabled = true;
    float min_threshold = 0.0f;
    float dangerous_threshold = 50.0f;
    float critical_threshold = 80.0f;

    // Функция для сериализации в JSON
    void toJson(const JsonObject& obj) const {
        obj["enabled"] = enabled;
        obj["min"] = min_threshold;
        obj["dangerous"] = dangerous_threshold;
        obj["critical"] = critical_threshold;
    }

    // Функция для десериализации из JSON
    static ThresholdSettings fromJson(const JsonObject& obj) {
        ThresholdSettings settings;
        settings.enabled = obj["enabled"] | true;
        settings.min_threshold = obj["min"] | 0.0f;
        settings.dangerous_threshold = obj["dangerous"] | 80.0f;
        settings.critical_threshold = obj["critical"] | 100.0f;
        return settings;
    }
};

// Класс состояния, который будет управляться StatefulService
// Хранит карту: "адрес_датчика" -> ThresholdSettings
class AlarmThresholdsState {
public:
    std::map<std::string, ThresholdSettings> sensor_thresholds;

    // --- Функции для интеграции с StatefulService ---
    // Читает состояние в JSON для отправки клиенту или сохранения
    static void read(const AlarmThresholdsState& state, const JsonObject& root);

    // Обновляет состояние из JSON, полученного от клиента
    static StateUpdateResult update(const JsonObject& root, AlarmThresholdsState& state);
};

class AlarmThresholdService : public StatefulService<AlarmThresholdsState> {
public:
    // Конструктор
    AlarmThresholdService(PsychicHttpServer* server, ESP32SvelteKit* sveltekit) :
        _httpEndpoint(
            AlarmThresholdsState::read,
            AlarmThresholdsState::update,
            this,
            server,
            ALARM_SETTINGS_ENDPOINT,
            sveltekit->getSecurityManager()
            ),
        _fsPersistence(AlarmThresholdsState::read,
            AlarmThresholdsState::update,
            this,
            sveltekit->getFS(),
            ALARM_SETTINGS_FILE)
    {
    }

    void begin() {
        _fsPersistence.readFromFS();
        _httpEndpoint.begin();
    }

private:
    HttpEndpoint<AlarmThresholdsState> _httpEndpoint;
    FSPersistence<AlarmThresholdsState> _fsPersistence;
};

#endif //SSVC_OPEN_CONNECT_ALARMTHRESHOLDS_H