
#ifndef SSVC_OPEN_CONNECT_DS18B20SENSOR_H
#define SSVC_OPEN_CONNECT_DS18B20SENSOR_H
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

#include "DallasTemperature.h"
#include "components/sensors/AbstractSensor/AbstractSensor.h"
#include "components/Zone/SensorZone.h"


/**
 * @brief Конкретный класс для управления датчиком DS18B20.
 */
class DS18B20Sensor final : public AbstractSensor {
public:
    /**
     * @brief Конструктор.
     * @param addr Адрес датчика 1-Wire.
     * @param name Дружественное имя.
     * @param sensors Указатель на объект DallasTemperature, управляющий шиной.
     * @param zone Зона ответственности (по умолчанию UNKNOWN).
     */
    DS18B20Sensor(
        const Address addr,
        const std::string& name,
        DallasTemperature* sensors, // Указатель
        SensorZone zone = SensorZone::UNKNOWN
    );

    void readValue() override;
    float getData() const override;
    MeasuredValueType getMeasurementType() const override;

private:
    DallasTemperature* dallasSensors;
    float lastReading;

};


#endif //SSVC_OPEN_CONNECT_DS18B20SENSOR_H