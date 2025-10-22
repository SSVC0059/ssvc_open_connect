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

// components/sensors/SensorDataService/SensorDataState.cpp

#include "SensorDataService.h"

SensorDataService* SensorDataService::_instance = nullptr;

void SensorDataState::read(const SensorDataState& state, const JsonObject& root) {
    for (const auto& pair : state.readings_by_zone) {

        // 1. Извлекаем ключи и значения через .first и .second
        const SensorZone zone = pair.first;
        const std::map<std::string, float>& sensor_readings = pair.second;

        // Конвертируем SensorZone enum в строковое имя для ключа JSON
        const std::string zoneName = SensorZoneHelper::toString(zone);

        // Создаем вложенный JSON-объект для этой зоны
        const auto zone_obj = root[zoneName.c_str()].to<JsonObject>();

        // Теперь делаем то же самое для внутреннего цикла:
        for (const auto& sensor_pair : sensor_readings) {

            const std::string& address = sensor_pair.first;
            const float value = sensor_pair.second;

            zone_obj[address.c_str()] = value;
        }
    }
}

SensorDataService::SensorDataService(PsychicHttpServer *server,
                                    ESP32SvelteKit* sveltekit)
    : _httpEndpoint(
          SensorDataState::read,
          SensorDataState::update,
          this,
          server,
          SENSOR_DATA_ENDPOINT, // /rest/data
          sveltekit->getSecurityManager()
      ),
      _mqttEndpoint(
          SensorDataState::read,
          SensorDataState::update,
          this,
          sveltekit->getMqttClient(),
          SENSOR_DATA_PUB_TOPIC,
          ""
          )
{
    ESP_LOGI(TAG, "SensorDataService initialized (RAM-only, HTTP: %s, MQTT: %s)",
             SENSOR_DATA_ENDPOINT, SENSOR_DATA_PUB_TOPIC);
}


void SensorDataService::updateSensorData(
    const std::map<SensorZone, std::vector<AbstractSensor*>>& groupedSensors)
{
    const String originId = "SENSOR_READING_LOOP";

    // Используем атомарное обновление состояния
    this->update([&](SensorDataState& state) {

        SensorDataState newState; // Временное состояние для новых данных
        bool dataChanged = false;

        // Внешний цикл: итерация по std::map<SensorZone, std::vector<AbstractSensor*>>
        // 'pair' — это std::pair<SensorZone, std::vector<AbstractSensor*>>
        for (const auto& pair : groupedSensors) {

            const SensorZone zone = pair.first;
            const std::vector<AbstractSensor*>& sensors = pair.second;

            // Внутренний цикл: итерация по std::vector<AbstractSensor*>
            for (const auto& sensor : sensors) {

                // Используем только валидные данные
                if (sensor->isDataValid()) {
                    const std::string addressStr = SensorManager::addressToString(sensor->getAddress());
                    const float reading = sensor->getData();

                    // Добавляем данные в новое состояние
                    newState.readings_by_zone[zone][addressStr] = reading;

                    // Проверяем, изменилось ли значение по сравнению с текущим состоянием (state)

                    // 1. Проверка наличия зоны в старом состоянии
                    if (state.readings_by_zone.count(zone) == 0) {
                        dataChanged = true;
                        continue; // Переходим к следующему датчику
                    }

                    // Получаем ссылку на внутреннюю карту для текущей зоны
                    const std::map<std::string, float>& old_readings = state.readings_by_zone.at(zone);

                    // 2. Проверка наличия адреса в старом состоянии
                    if (old_readings.count(addressStr) == 0) {
                        dataChanged = true;
                        continue;
                    }

                    // 3. Проверка изменения значения (с учетом допуска для float)
                    const float old_reading = old_readings.at(addressStr);
                    if (std::abs(old_reading - reading) > 0.01f) // Допуск 0.01f
                    {
                        dataChanged = true;
                    }
                }
            }
        }

        // Дополнительная проверка на изменение структуры (датчик появился/пропал)
        // Если размеры карт зон или внутренние данные изменились
        if (newState.readings_by_zone.size() != state.readings_by_zone.size() ||
            (dataChanged == false && newState.readings_by_zone != state.readings_by_zone))
        {
            dataChanged = true;
        }

        if (dataChanged) {
            // Заменяем старое состояние новым, чтобы вызвать PUB/WS
            state.readings_by_zone = std::move(newState.readings_by_zone);
            return StateUpdateResult::CHANGED;
        }

        return StateUpdateResult::UNCHANGED;
    }, originId);
}