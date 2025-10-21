#ifndef SSVC_OPEN_CONNECT_ONEWIRETHERMALSUBSYSTEM_H
#define SSVC_OPEN_CONNECT_ONEWIRETHERMALSUBSYSTEM_H

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


#include <vector>
#include "components/sensors/PollingSubsystem/PollingSubsystem.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "components/sensors/DS18B20Sensor/DS18B20Sensor.h"

/**
 * @brief Подсистема, управляющая 1-Wire опросом (DS18B20).
 */
class OneWireThermalSubsystem final : public PollingSubsystem {
public:
    // Параметры специфичные для этой подсистемы
    static constexpr uint8_t ONEWIRE_PIN = 2;

    static OneWireThermalSubsystem& getInstance();

    // Реализация чистых виртуальных методов PollingSubsystem
    bool initialize() override;
    void poll() override; // Здесь будет вызываться requestTemperatures и считывание

private:
    // Константы для конструктора родителя
    static constexpr auto DEFAULT_TASK_NAME = "OneWireTask";
    static constexpr TickType_t DEFAULT_POLL_INTERVAL = pdMS_TO_TICKS(1000);

    // Приватный конструктор, вызывающий родительский
    OneWireThermalSubsystem()
        : PollingSubsystem(DEFAULT_TASK_NAME, DEFAULT_POLL_INTERVAL) {}

    OneWire* oneWireBus = nullptr;
    DallasTemperature* dallasTemp = nullptr;

    std::vector<DS18B20Sensor*> ds18b20Sensors;

    // Вспомогательная функция, специфичная для 1-Wire
    void discoverAndRegisterSensors();

    static constexpr auto TAG = "ONEWIRE_SUB";
};

#endif //SSVC_OPEN_CONNECT_ONEWIRETHERMALSUBSYSTEM_H