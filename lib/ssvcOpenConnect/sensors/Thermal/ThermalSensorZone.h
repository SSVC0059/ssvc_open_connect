//
// Created by demoncat on 19.04.25.
//

#ifndef ThermalSensorZone_H
#define ThermalSensorZone_H
#include <stdexcept>
#include <string>
#include <unordered_map>

// Тип для зоны ответственности датчика
enum class ThermalSensorZone
{
    UNKNOWN, // Не назначено
    INLET_WATER, // Входящая вода
    OUTLET_WATER, // Исходящая вода
    ACT, // Трубка связи с атмосферой
    DELETED, // Удаленные датчики из системы
};

// Класс-хелпер для работы с ThermalSensorZone
class ThermalSensorZoneHelper
{
public:
    /**
     * @brief Получает строковое описание зоны.
     * @param zone Зона датчика.
     * @return Описание (например, "INLET_WATER").
     * @throw std::invalid_argument Если зона неизвестна.
     */
    static std::string toString(ThermalSensorZone zone)
    {
        switch (zone)
        {
        case ThermalSensorZone::UNKNOWN: return "unknown";
        case ThermalSensorZone::INLET_WATER: return "inlet_water";
        case ThermalSensorZone::OUTLET_WATER: return "outlet_water";
        case ThermalSensorZone::ACT: return "act";
        default: throw std::invalid_argument("Unknown ThermalSensorZone value");
        }
    }

    /**
     * @brief Получает зону по её строковому имени.
     * @param name Название зоны (например, "INLET_WATER").
     * @return Соответствующая зона ThermalSensorZone.
     * @throw std::invalid_argument Если имя не распознано.
     */
    static ThermalSensorZone fromString(const std::string& name)
    {
        static const std::unordered_map<std::string, ThermalSensorZone> nameToZone = {
            {"unknown", ThermalSensorZone::UNKNOWN},
            {"inlet_water", ThermalSensorZone::INLET_WATER},
            {"outlet_water", ThermalSensorZone::OUTLET_WATER},
            {"act", ThermalSensorZone::ACT},
        };

        auto it = nameToZone.find(name);
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
