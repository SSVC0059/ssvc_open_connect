#ifndef ThermalSensorZone_H
#define ThermalSensorZone_H

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

#include <stdexcept>
#include <string>
#include <unordered_map>

// Тип для зоны ответственности датчика
enum class SensorZone
{
    UNKNOWN, // Не назначено
    INLET_WATER, // Входящая вода
    OUTLET_WATER, // Исходящая вода
    ACT, // Трубка связи с атмосферой
    DELETED, // Удаленные датчики из системы
};

// Класс-хелпер для работы с ThermalSensorZone
class SensorZoneHelper
{
public:
    /**
     * @brief Получает строковое описание зоны.
     * @param zone Зона датчика.
     * @return Описание (например, "INLET_WATER").
     * @throw std::invalid_argument Если зона неизвестна.
     */
    static std::string toString(const SensorZone zone)
    {
        switch (zone)
        {
        case SensorZone::UNKNOWN: return "unknown";
        case SensorZone::INLET_WATER: return "inlet_water";
        case SensorZone::OUTLET_WATER: return "outlet_water";
        case SensorZone::ACT: return "act";
        default: throw std::invalid_argument("Unknown ThermalSensorZone value");
        }
    }

    /**
     * @brief Получает зону по её строковому имени.
     * @param name Название зоны (например, "INLET_WATER").
     * @return Соответствующая зона ThermalSensorZone.
     * @throw std::invalid_argument Если имя не распознано.
     */
    static SensorZone fromString(const std::string& name)
    {
        static const std::unordered_map<std::string, SensorZone> nameToZone = {
            {"unknown", SensorZone::UNKNOWN},
            {"inlet_water", SensorZone::INLET_WATER},
            {"outlet_water", SensorZone::OUTLET_WATER},
            {"act", SensorZone::ACT},
        };

        const auto it = nameToZone.find(name);
        if (it != nameToZone.end())
        {
            return it->second;
        }
        throw std::invalid_argument("Unknown ThermalSensorZone name: " + name);
    }

    static std::string translateZone(std::string zone)
    {
        if (zone == "unknown")
            return "Без зоны";
        if (zone == "inlet_water")
            return "Входящая вода";
        if (zone == "outlet_water")
            return "Исходящая вода";
        if (zone == "act")
            return "ТСА";
        else {
            return zone;
        }
    }
};


#endif //ThermalSensorZone_H
