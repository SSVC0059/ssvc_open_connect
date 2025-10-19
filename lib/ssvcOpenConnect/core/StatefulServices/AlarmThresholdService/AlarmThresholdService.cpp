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

#include "AlarmThresholdService.h"


// Статический тег для ESP_LOG
static auto TAG = "ALARM_THRESHOLDS_SVC";

/**
 * @brief Сравнивает два объекта ThresholdSettings.
 * @return True, если настройки идентичны.
 */
static bool areThresholdsEqual(const ThresholdSettings& a, const ThresholdSettings& b) {
    // Сравниваем все поля. Используем небольшую дельту для сравнения float.
    constexpr float EPSILON = 0.001f;

    return a.enabled == b.enabled &&
           (std::abs(a.min_threshold - b.min_threshold) < EPSILON) &&
           (std::abs(a.dangerous_threshold - b.dangerous_threshold) < EPSILON) &&
           (std::abs(a.critical_threshold - b.critical_threshold) < EPSILON);
}

// Читает состояние в JSON для отправки клиенту или сохранения
// Реализация AlarmThresholdsState::read
void AlarmThresholdsState::read(const AlarmThresholdsState& state, const JsonObject& root) {
    ESP_LOGI(TAG, "Starting JSON serialization (READ) for Alarm Thresholds.");

    // Исправление: Используем createNestedObject для создания и заполнения.
    const auto thresholds = root["thresholds"].to<JsonObject>();

    int count = 0;
    for (const auto& pair : state.sensor_thresholds) {
        // Создаем объект для настроек каждого датчика
        auto sensor_obj = thresholds[pair.first].to<JsonObject>();
        // Сериализуем данные
        pair.second.toJson(sensor_obj);
        ESP_LOGD(TAG, "Serializing settings for sensor %s: Enabled=%s, Critical=%.2f",
                 pair.first.c_str(), pair.second.enabled ? "True" : "False", pair.second.critical_threshold);
        count++;
    }
    ESP_LOGI(TAG, "Finished JSON serialization. Total sensors serialized: %d", count);
}

// AlarmThresholdService.cpp

// Реализация AlarmThresholdsState::update
StateUpdateResult AlarmThresholdsState::update(const JsonObject& root, AlarmThresholdsState& state) {
    ESP_LOGI(TAG, "Starting JSON deserialization (UPDATE) for Alarm Thresholds.");

    // Определение фазы загрузки: если SensorManager пуст, то это, скорее всего, ранний boot.
    // Это позволяет нам загрузить настройки, даже если датчики еще не обнаружены.
    const bool isEarlyBoot = SensorManager::getInstance().getRegisteredSensorCount() == 0;

    if (root["thresholds"].isNull() || !root["thresholds"].is<JsonObject>()) {
        ESP_LOGW(TAG, "UPDATE rejected: 'thresholds' field is missing or not a JSON object.");
        return StateUpdateResult::UNCHANGED;
    }

    bool changed = false;
    const JsonObject thresholds = root["thresholds"];

    // 1. Создаем временный набор для хранения адресов датчиков из входящего JSON
    std::set<std::string> incoming_addresses;
    int updated_count = 0;

    // 2. Обрабатываем входящие данные: обновляем/добавляем настройки и заполняем incoming_addresses
    for (JsonPair kv : thresholds) {
        if (kv.value().is<JsonObject>()) {
            std::string sensor_addr = kv.key().c_str();

            // Если это НЕ ранний boot (система полностью инициализирована)
            // И датчик НЕ зарегистрирован, тогда пропускаем обновление.
            // В случае isEarlyBoot == true, мы загружаем настройки в любом случае.
            if (!isEarlyBoot && !SensorManager::getInstance().isSensorRegistered(sensor_addr)) {
                ESP_LOGW(TAG, "Sensor %s update skipped: Not registered in SensorManager.", sensor_addr.c_str());
                continue; // Пропускаем, если система уже загружена, но датчик отсутствует
            }
            // -----------------------------------------------------------

            const JsonObject settings_obj = kv.value().as<JsonObject>();
            if (!settings_obj.isNull()) {

                // Включаем адрес в список, только если это не ранний бут.
                // Во время раннего бута этот список не нужен для очистки.
                if (!isEarlyBoot) {
                   incoming_addresses.insert(sensor_addr);
                }

                const ThresholdSettings new_settings = ThresholdSettings::fromJson(settings_obj);
                auto it = state.sensor_thresholds.find(sensor_addr);

                if (isEarlyBoot) {
                    // НА РАННЕМ БУТЕ: Мы просто восстанавливаем состояние из файла.
                    // Не устанавливаем changed = true, чтобы избежать записи в ФС.
                    state.sensor_thresholds[sensor_addr] = new_settings;
                    updated_count++;
                }
                else if (it == state.sensor_thresholds.end() || !areThresholdsEqual(it->second, new_settings)) {
                    // В ОБЫЧНОМ РЕЖИМЕ: Настройки изменились или это новый датчик -> обновляем состояние и ставим changed
                    state.sensor_thresholds[sensor_addr] = new_settings;
                    changed = true;
                    updated_count++;
                    ESP_LOGD(TAG, "Sensor %s settings CHANGED/ADDED. Crit: %.2f", sensor_addr.c_str(), new_settings.critical_threshold);
                } else {
                    // В ОБЫЧНОМ РЕЖИМЕ: Настройки не изменились -> пропускаем
                    ESP_LOGV(TAG, "Sensor %s settings UNCHANGED.", sensor_addr.c_str());
                }
            } else {
                ESP_LOGW(TAG, "Sensor %s update skipped: Settings object is null.", sensor_addr.c_str());
            }

        } else {
            ESP_LOGW(TAG, "Key '%s' update skipped: Value is not a JSON object (Malicious/bad data).", kv.key().c_str());
        }
    }

    ESP_LOGI(TAG, "Finished processing incoming settings. Updated/added %d sensors.", updated_count);

    // Удаление настроек для датчиков, которые больше не зарегистрированы
    int deleted_count = 0;
    if (!isEarlyBoot)
    {
        for (auto it = state.sensor_thresholds.begin(); it != state.sensor_thresholds.end(); ) {
            const std::string& current_addr = it->first;

            // Проверяем, если датчик отключен (не зарегистрирован)
            const bool unregistered = !SensorManager::getInstance().isSensorRegistered(current_addr);

            // Очищаем, если он не зарегистрирован ИЛИ отсутствует во входящем запросе (только не в режиме раннего бута)
            if (unregistered) {
                ESP_LOGW(TAG, "Sensor %s deleted: Absent from request OR unregistered.", current_addr.c_str());
                it = state.sensor_thresholds.erase(it);
                changed = true;
                deleted_count++;
            } else {
                ++it;
            }
        }
    } else {
        ESP_LOGI(TAG, "Skipping deletion logic: SensorManager is empty (likely during early boot).");
    }

    ESP_LOGI(TAG, "Finished UPDATE. Deleted %d sensor settings. Result: %s",
             deleted_count, changed ? "CHANGED" : "UNCHANGED");
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}