//
// Created by Admin on 15/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_ABSTRACTSENSOR_H
#define SSVC_OPEN_CONNECT_ABSTRACTSENSOR_H
#include <cstring>
#include <string>
#include "components/Zone/SensorZone.h"

enum class MeasuredValueType {
    GENERIC = 0,
    TEMPERATURE,
    PRESSURE,
    HUMIDITY,
};


/**
 * @brief Абстрактный базовый класс для всех датчиков.
 */
class AbstractSensor {
public:
    // 1-Wire адрес занимает 8 байт
    typedef uint8_t Address[8];

    // Конструктор
    AbstractSensor(const Address addr,
                    const std::string& name,
                    const SensorZone zone = SensorZone::UNKNOWN)
        : name(name),
          currentZone(zone),
        _dataValid(false),
        _isInitialized(false)
    {
        memcpy(this->address, addr, 8);
    }

    virtual ~AbstractSensor() = default;

    // Основные методы
    virtual void readValue() = 0;
    virtual float getData() const = 0;
    virtual bool isDataValid() const { return _dataValid; }
    virtual bool isInitialized() const { return _isInitialized; }

    // Геттеры
    const Address& getAddress() const { return address; }
    const std::string& getName() const { return name; }
    SensorZone getZone() const { return currentZone; }

    // Сеттер для зоны
    void setZone(const SensorZone newZone) { currentZone = newZone; }
    void setName(const std::string& newName) { this->name = newName; }

    virtual MeasuredValueType getMeasurementType() const = 0;

protected:
    Address address{};
    std::string name;
    SensorZone currentZone;
    bool _dataValid;
    bool _isInitialized;
};


#endif //SSVC_OPEN_CONNECT_ABSTRACTSENSOR_H