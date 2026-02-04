#ifndef SSVC_OPEN_CONNECT_ABSTRACTSENSOR_H
#define SSVC_OPEN_CONNECT_ABSTRACTSENSOR_H
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

#include <cstring>
#include <string>
#include <utility>
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
                    std::string  name,
                    const SensorZone zone = SensorZone::UNKNOWN)
        : name(std::move(name)),
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
    /**
         * @brief Получить I2C адрес (первый байт общего массива Address)
         */
    uint8_t getI2CAddress() const {
        return address[0];
    }

    virtual const std::string& getName() const { return name; }
    SensorZone getZone() const { return currentZone; }

    /**
     * @brief Возвращает строковое представление единицы измерения (например, "°C" или "mmHg")
     */
    virtual std::string getUnit() const = 0;
    virtual std::string getType() const = 0;

    // Сеттер для зоны
    void setZone(const SensorZone newZone) { currentZone = newZone; }
    void setName(const std::string& newName) { this->name = newName; }
    /**

     * @brief Установить I2C адрес
     * @param newAddr 7-битный адрес устройства
     */
    void setI2CAddress(const uint8_t newAddr) {
        address[0] = newAddr;
        // Остальные байты для I2C обнуляем для чистоты,
        // чтобы SensorManager не выводил мусор
        for(int i = 1; i < 8; i++) address[i] = 0;
    }
    virtual MeasuredValueType getMeasurementType() const = 0;

protected:
    Address address{};
    std::string name;
    SensorZone currentZone;
    bool _dataValid;
    bool _isInitialized;
};


#endif //SSVC_OPEN_CONNECT_ABSTRACTSENSOR_H