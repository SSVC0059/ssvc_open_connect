#ifndef SSVC_OPEN_CONNECT_SENSORMANAGER_H
#define SSVC_OPEN_CONNECT_SENSORMANAGER_H

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
#include <map>
#include <vector>
#include <string>

#include <iostream>

#include "components/sensors/AbstractSensor/AbstractSensor.h"
#include "components/Zone/SensorZone.h"

/**
 * @brief Абстрактный менеджер, не зависящий от конкретного типа датчиков.
 * Реализован как Одиночка (Singleton).
 * Ответственен за хранение, поиск и управление метаданными датчиков.
 */
class SensorManager {
public:

    // --- Singleton Access ---
    static SensorManager& getInstance();

    // --- Sensor Management ---

    /**
     * @brief Регистрирует новый датчик в реестре.
     * @param newSensor Указатель на новый объект AbstractSensor*.
     */
    void registerSensor(AbstractSensor* newSensor);

    /**
     * @brief Возвращает всех зарегистрированных датчиков (любого типа).
     */
    const std::map<std::string, AbstractSensor*>& getAllSensors() const;

    /**
     * @brief Ищет датчик по его адресу (массив байт).
     */
    AbstractSensor* getSensorByAddress(const AbstractSensor::Address& addr) const;

    /**
     * @brief Возвращает количество зарегистрированных датчиков.
     * Используется для проверки готовности системы.
     * @return Количество датчиков.
     */
    size_t getRegisteredSensorCount() const;

    /**
     * @brief Запускает считывание (readValue) на датчике и возвращает его значение (getData).
     * Это может быть блокирующая операция, если датчик требует синхронного опроса.
     * @param addressStr Строковый адрес датчика.
     * @return Float Значение датчика или код ошибки (-999.0f).
     */
    float getValueByAddress(const std::string& addressStr) const;

    /**
     * @brief Проверяет, зарегистрирован ли датчик с данным строковым адресом.
     * @param addressStr Строковый адрес датчика.
     * @return True, если датчик зарегистрирован, false в противном случае.
     */
    bool isSensorRegistered(const std::string& addressStr) const;

    // --- Metadata Management ---

    /**
     * @brief Назначает новую зону датчику.
     */
    bool assignZone(const AbstractSensor::Address& addr, SensorZone newZone) const;

    /**
     * @brief Группирует все активные датчики по их SensorZone.
     */
    std::map<SensorZone, std::vector<AbstractSensor*>> getAllSensorsGroupedByZone() const;

    /**
 * @brief Группирует все активные датчики по их SensorZone, применяя фильтр по типу измерения.
 * @param filterType Тип измерения, по которому нужно отфильтровать датчики.
 * @return Карта сгруппированных датчиков.
 */
    std::map<SensorZone, std::vector<AbstractSensor*>> getAllSensorsGroupedByZone(
        MeasuredValueType filterType) const;

    // --- Static Helpers (для работы с 1-Wire адресами) ---
    static std::string addressToString(const AbstractSensor::Address& addr);
    static bool stringToAddress(const std::string& addrStr, AbstractSensor::Address& addr);



    // --- Lifecycle ---
    ~SensorManager();

private:
    // Приватный конструктор для паттерна Одиночка
    SensorManager() {}

    // Основной реестр датчиков (владение указателями)
    std::map<std::string, AbstractSensor*> sensors;

    // Вспомогательная функция для освобождения памяти
    void cleanup();

    static constexpr auto TAG = "SENSOR_MANAGER";
};
#endif //SSVC_OPEN_CONNECT_SENSORMANAGER_H