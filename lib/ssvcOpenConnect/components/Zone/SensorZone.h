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
    UNKNOWN,      // Не назначено
    INLET_WATER,  // Входящая вода
    OUTLET_WATER, // Исходящая вода
    ACT,          // ТСА (Трубка связи с атмосферой)
    TANK,         // Куб
    OUTDOOR,      // Атмосфера
    DELETED,      // Удаленные датчики
};

// Класс-хелпер для работы с ThermalSensorZone
class SensorZoneHelper
{
public:
    // SensorZone.h

    static std::string toString(SensorZone zone)
    {
        switch (zone)
        {
            case SensorZone::UNKNOWN: return "unknown";
            case SensorZone::INLET_WATER: return "inlet_water";
            case SensorZone::OUTLET_WATER: return "outlet_water";
            case SensorZone::ACT: return "act";
            case SensorZone::TANK: return "tank";          // Добавлено
            case SensorZone::OUTDOOR: return "outdoor";    // Добавлено
            case SensorZone::DELETED: return "deleted";    // Добавлено
            default: return "unknown"; // Безопасный возврат вместо throw
        }
    }

    static SensorZone fromString(const std::string& name)
    {
        static const std::unordered_map<std::string, SensorZone> nameToZone = {
            {"unknown", SensorZone::UNKNOWN},
            {"inlet_water", SensorZone::INLET_WATER},
            {"outlet_water", SensorZone::OUTLET_WATER},
            {"act", SensorZone::ACT},
            {"tank", SensorZone::TANK},       // Добавлено
            {"outdoor", SensorZone::OUTDOOR}, // Добавлено
            {"deleted", SensorZone::DELETED}  // Добавлено
        };

        const auto it = nameToZone.find(name);
        if (it != nameToZone.end())
        {
            return it->second;
        }
        return SensorZone::UNKNOWN; // Безопасный возврат вместо throw
    }

    static std::string translateZone(const std::string& zone)
    {
        if (zone == "unknown")      return "Без зоны";
        if (zone == "inlet_water")  return "Входящая вода";
        if (zone == "outlet_water") return "Исходящая вода";
        if (zone == "act")          return "ТСА";
        if (zone == "tank")         return "Куб";
        if (zone == "outdoor")      return "Атмосфера";
        if (zone == "deleted")      return "Удален";

        return "Неизвестно";
    }
};


#endif //ThermalSensorZone_H
