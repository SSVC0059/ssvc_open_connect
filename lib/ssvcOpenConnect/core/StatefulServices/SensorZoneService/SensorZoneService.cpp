#include "SensorZoneService.h"

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

void SensorZoneState::read(const SensorZoneState& state, const JsonObject& root) {
    // Создаем вложенный объект "zones"
    const auto zones_obj = root["zones"].to<JsonObject>();
    for (const auto& pair : state.sensor_zones) {
        zones_obj[pair.first] = static_cast<int>(pair.second);
    }
}
StateUpdateResult SensorZoneState::update(const JsonObject& root, SensorZoneState& state){

    // Определяем фазу загрузки: если SensorManager пуст, то это, скорее всего, ранний boot.
    const bool isEarlyBoot = SensorManager::getInstance().getRegisteredSensorCount() == 0;

    if (!root["zones"].is<JsonObject>()) return StateUpdateResult::UNCHANGED;

    bool changed = false;
    const JsonObject zones = root["zones"];

    // 1. Создаем временный набор для входящих адресов
    std::set<std::string> incoming_addresses;

    // 2. Обновление/добавление зон
    for (JsonPair kv : zones) {
        const std::string addr_str = kv.key().c_str();

        // Проверка: значение должно быть целым числом (Zone ID)
        if (kv.value().is<int>()) {
            const SensorZone newZone = static_cast<SensorZone>(kv.value().as<int>());

            // Проверка на фактическое изменение перед установкой 'changed = true'
            if (state.sensor_zones.count(addr_str) == 0 || state.sensor_zones.at(addr_str) != newZone) {
                state.sensor_zones[addr_str] = newZone;
                changed = true; // Зона изменилась, отмечаем для сохранения!
            }
            incoming_addresses.insert(addr_str);
        }
    }

    // 3. Логика удаления (очистка зон для незарегистрированных датчиков или удаленных в запросе)
    // Удаляем зоны только после загрузки датчиков (не в режиме раннего бута)
    if (!isEarlyBoot)
    {
        int deleted_count = 0;
        for (auto it = state.sensor_zones.begin(); it != state.sensor_zones.end(); ) {
            const std::string& current_addr = it->first;

            // Проверяем, если датчик отключен (не зарегистрирован) ИЛИ отсутствует во входящем запросе
            const bool unregistered = !SensorManager::getInstance().isSensorRegistered(current_addr);
            const bool not_in_incoming = incoming_addresses.count(current_addr) == 0;

            if (unregistered || not_in_incoming) {
                ESP_LOGD(TAG, "Zone %s deleted: Unregistered (%d) OR absent from request (%d).",
                         current_addr.c_str(), unregistered, not_in_incoming);
                it = state.sensor_zones.erase(it);
                changed = true;
                deleted_count++;
            } else {
                ++it;
            }
        }
    } else {
        ESP_LOGV(TAG, "Skipping deletion logic: SensorManager is empty (likely during early boot).");
    }

    // Логируем результат работы update-метода
    ESP_LOGV(TAG, "Finished processing incoming settings. Deleted: %d. Result: %s",
             deleted_count, changed ? "CHANGED" : "UNCHANGED");

    if (changed) return StateUpdateResult::CHANGED;
    return StateUpdateResult::UNCHANGED;
}

void SensorZoneState::applyZonesToSensors() const {
    const SensorManager& sm = SensorManager::getInstance();
    int applied_count = 0;

    // Итерируемся по всем зонам, которые хранятся в состоянии
    for (const auto& pair : sensor_zones) {
        const std::string& addr_str = pair.first;
        const SensorZone zone = pair.second;

        // Пытаемся преобразовать строку в адрес и применить зону
        AbstractSensor::Address addrBytes;
        if (SensorManager::stringToAddress(addr_str, addrBytes)) {
            // Фактически применяем зону через SensorManager
            if (sm.assignZone(addrBytes, zone)) {
                applied_count++;
            }
        }
    }
    ESP_LOGV(TAG, "[ZONE_APPLY] Applied %d saved zones to sensors.", applied_count);
}

bool SensorZoneService::setZoneForSensor(const std::string& addressStr, SensorZone newZone) {

    // КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: Добавляем originId
    const String originId = "HTTP_ZONE_UPDATE";

    const StateUpdateResult result = this->update([&](SensorZoneState& state) {

        // 1. Проверяем, изменилась ли зона
        if (state.sensor_zones.count(addressStr) &&
            state.sensor_zones.at(addressStr) == newZone)
        {
            // Успешно, но без изменений
            ESP_LOGV("SET_ZONE", "[SET_ZONE] Zone %d already set for %s. Skipping save.",
                     static_cast<int>(newZone), addressStr.c_str());
            return StateUpdateResult::UNCHANGED;
        }

        // 2. Обновляем карту в состоянии сервиса
        state.sensor_zones[addressStr] = newZone;

        // Добавляем лог, подтверждающий, что изменения произошли и будет вызван FSPersistence
        ESP_LOGV("SET_ZONE", "[SET_ZONE] Zone %d set for %s. Triggering persistence.",
                 static_cast<int>(newZone), addressStr.c_str());

        // 3. Возвращаем CHANGED, чтобы уведомить FSPersistence и WebSocket
        return StateUpdateResult::CHANGED;

    }, originId); // <-- Обязательный второй аргумент originId

    // Немедленно обновляем состояние живого объекта датчика (SensorManager)
    AbstractSensor::Address addrBytes;
    if (SensorManager::stringToAddress(addressStr, addrBytes)) {
        (void)SensorManager::getInstance().assignZone(addrBytes, newZone);
    }

    // Возвращаем результат операции
    return result != StateUpdateResult::UNCHANGED;
}
