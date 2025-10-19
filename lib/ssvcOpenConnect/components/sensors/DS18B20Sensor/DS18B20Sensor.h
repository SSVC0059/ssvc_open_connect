//
// Created by Admin on 15/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_DS18B20SENSOR_H
#define SSVC_OPEN_CONNECT_DS18B20SENSOR_H
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