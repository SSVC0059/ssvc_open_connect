#pragma once

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

#include <string>
#include <map>
#include "SensorConfigService.h"
#include "components/Zone/SensorZone.h"

/**
 * @brief Вспомогательный класс для конвертации SensorZone <-> string.
 *
 * Это критически важно для сериализации/десериализации в JSON/MQTT.
 * Предотвращает ошибки, связанные с использованием статического приведения int.
 */
class SensorConfigHelper {
public:
    /**
     * @brief Конвертирует SensorZone enum в строковое представление.
     * @param zone Зона для конвертации.
     * @return Строковое имя зоны (например, "ZONE_A", "UNKNOWN").
     */
    static const std::string& toString(SensorZone zone);

    /**
     * @brief Конвертирует строковое представление в SensorZone enum.
     * @param zoneStr Строка зоны (например, "ZONE_B").
     * @return SensorZone enum. Возвращает SensorZone::UNKNOWN, если строка не найдена.
     */
    static SensorZone fromString(const std::string& zoneStr);

private:
    // Приватная карта для хранения соответствия (создается один раз)
    static const std::map<SensorZone, std::string> zoneToStringMap;
    static const std::map<std::string, SensorZone> stringToZoneMap;

    /**
     * @brief Инициализирует статические карты соответствия.
     */
    static void initializeMaps();
};
