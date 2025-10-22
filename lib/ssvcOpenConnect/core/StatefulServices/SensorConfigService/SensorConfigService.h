#ifndef SSVC_OPEN_CONNECT_SENSORZONESERVICE_H
#define SSVC_OPEN_CONNECT_SENSORZONESERVICE_H

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

#include <Arduino.h>
#include <string>
#include <map>
#include <set>
#include "ArduinoJson.h"
#include "ESP32SvelteKit.h"
#include "StatefulService.h"
#include "components/Zone/SensorZone.h"
#include "components/sensors/SensorManager/SensorManager.h"

#include "core/StatefulServices/SensorConfigService/SensorConfigHelper.h"

#define ZONE_SETTINGS_ENDPOINT "/rest/zones"
#define ZONE_SETTINGS_FILE "/config/zones.json"
#define SENSOR_ZONE_PUB_TOPIC "openconnect/sensor/state"
#define SENSOR_ZONE_SET_TOPIC "openconnect/sensor/set"
/**
 * @brief Класс состояния для хранения настроек зон.
 * Хранит карту: "адрес_датчика" -> SensorZone
 */
class SensorConfigState {
public:
    std::map<std::string, SensorZone> sensor_zones;

    // Читает состояние в JSON для отправки клиенту или сохранения
    static void read(const SensorConfigState& state, const JsonObject& root);

    // Обновляет состояние из JSON, полученного от клиента
    static StateUpdateResult update(const JsonObject& root, SensorConfigState& state);

    /**
     * @brief Метод для применения всех сохраненных зон к SensorManager (используется при запуске)
     */
    void applyZonesToSensors() const;

private:
    static constexpr auto TAG = "SENSOR_ZONE_STAGE";
};

/**
 * @brief Сервис, управляющий зонами датчиков.
 * Отвечает за сохранение/загрузку из FS и REST API.
 */
class SensorConfigService : public StatefulService<SensorConfigState> {
public:
    SensorConfigService(PsychicHttpServer* server, ESP32SvelteKit* sveltekit) :
        _httpEndpoint(
            SensorConfigState::read,
            SensorConfigState::update,
            this,
            server,
            ZONE_SETTINGS_ENDPOINT,
            sveltekit->getSecurityManager()
            ),
        _fsPersistence(
            SensorConfigState::read,
            SensorConfigState::update,
            this,
            sveltekit->getFS(),
            ZONE_SETTINGS_FILE
            ),
        _mqttEndpoint(SensorConfigState::read,
                   SensorConfigState::update,
                   this,
                   sveltekit->getMqttClient(),
                   SENSOR_ZONE_PUB_TOPIC,
                   SENSOR_ZONE_SET_TOPIC,
                   0, // QoS (Quality of Service)
                   false)
    {
    }

    void begin(){
        _httpEndpoint.begin();
        _fsPersistence.readFromFS();
        this->read([&](const SensorConfigState& state) {
            state.applyZonesToSensors();
        });
    }

    /**
     * @brief Обновляет зону для одного датчика в состоянии сервиса и
     * запускает механизм сохранения, если зона изменилась.
     * @param addressStr Строковый адрес датчика.
     * @param newZone Новая зона.
     * @return True, если зона успешно обновлена (даже если не изменилась).
     */
    bool setZoneForSensor(const std::string& addressStr, SensorZone newZone);

private:
    HttpEndpoint<SensorConfigState> _httpEndpoint;
    FSPersistence<SensorConfigState> _fsPersistence;
    MqttEndpoint<SensorConfigState> _mqttEndpoint;

    static constexpr auto TAG = "SENSOR_ZONE_SERVICE";
};


#endif //SSVC_OPEN_CONNECT_SENSORZONESERVICE_H