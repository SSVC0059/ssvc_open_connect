#ifndef SSVC_OPEN_CONNECT_POLLINGSUBSYSTEM_H
#define SSVC_OPEN_CONNECT_POLLINGSUBSYSTEM_H

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


#include <stdint.h>
#include "ESP32SvelteKit.h"

#ifndef POLLING_SUBSYSTEM_TAG
#define POLLING_SUBSYSTEM_TAG "POLLING_SUB"
#endif

class PollingSubsystem {
public:

    // Установка инициализации должна быть выполнена в наследнике (например, в OneWireThermalSubsystem::initialize())
    virtual bool initialize() = 0;

    // Чистый виртуальный метод для выполнения сбора данных.
    // Вызывается ОДНОКРАТНО за цикл из SensorCoordinator.
    virtual void poll() = 0;

    // Виртуальный деструктор для корректного освобождения памяти.
    virtual ~PollingSubsystem() = default;

    // Метод для регистрации/идентификации.
    virtual const char* getName() const = 0;

protected:
    // Защищенный конструктор по умолчанию, не требующий параметров RTOS.
    PollingSubsystem() = default;

};

#endif // SSVC_OPEN_CONNECT_POLLINGSUBSYSTEM_H