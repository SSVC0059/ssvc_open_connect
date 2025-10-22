//
// Created by Admin on 22/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_SENSORDATASERVICE_H
#define SSVC_OPEN_CONNECT_SENSORDATASERVICE_H

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


#include <map>
#include <string>
#include <vector>
#include "ArduinoJson.h"
#include "ESP32SvelteKit.h"
#include "StatefulService.h"
#include "components/Zone/SensorZone.h"
#include "components/sensors/SensorManager/SensorManager.h" // Для AbstractSensor, addressToString

#include "core/StatefulServices/SensorConfigService/SensorConfigHelper.h"


#define SENSOR_DATA_PUB_TOPIC "openconnect/sensor/state"
#define SENSOR_DATA_ENDPOINT "/rest/sensor"


struct SensorDataState {
    /**
     * @brief Карта оперативных показаний:
     * Зона (enum) -> (Адрес датчика (string) -> Последнее значение (float))
     */
    std::map<SensorZone, std::map<std::string, float>> readings_by_zone;

    /**
     * @brief Сериализация состояния в JSON для HTTP GET, MQTT PUB и WebSocket.
     */
    static void read(const SensorDataState& state, const JsonObject& root);

    /**
     * @brief Десериализация JSON в состояние.
     * Мы не разрешаем клиентам устанавливать показания, поэтому метод неактивен.
     */
    static StateUpdateResult update(const JsonObject& root, SensorDataState& state) {
        // Данные датчиков являются READ-ONLY (только чтение) для внешних клиентов.
        return StateUpdateResult::UNCHANGED;
    }

    private:
        static constexpr auto TAG = "SensorDataState";
};

class SensorDataService final : public StatefulService<SensorDataState> {
public:

    explicit SensorDataService(PsychicHttpServer *server,
                               ESP32SvelteKit* sveltekit);
    ~SensorDataService() = default;

    static SensorDataService* getInstance() { return _instance; }
    static void setInstance(SensorDataService* instance) { _instance = instance; }

    void begin()
    {
        _httpEndpoint.begin();
    }

    /**
     * @brief Обновляет состояние сервиса новыми показаниями.
     * Вызывается из SensorManager.
     * @param groupedSensors Карта активных датчиков, сгруппированных по зонам.
     */
    void updateSensorData(
        const std::map<SensorZone, std::vector<AbstractSensor*>>& groupedSensors
    );

private:
    HttpEndpoint<SensorDataState> _httpEndpoint;
    MqttEndpoint<SensorDataState> _mqttEndpoint;

    static SensorDataService* _instance;

    static constexpr auto TAG = "SensorDataState";
};



#endif //SSVC_OPEN_CONNECT_SENSORDATASERVICE_H