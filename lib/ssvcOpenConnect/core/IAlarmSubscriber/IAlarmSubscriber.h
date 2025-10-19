#ifndef SSVC_OPEN_CONNECT_IALARMSUBSCRIBER_H
#define SSVC_OPEN_CONNECT_IALARMSUBSCRIBER_H

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

#include "components/sensors/AbstractSensor/AbstractSensor.h"
#include "core/StatefulServices/AlarmThresholdService/AlarmThresholdService.h" // Для AlarmLevel

// Структура, описывающая событие тревоги
struct AlarmEvent {
    const AbstractSensor* sensor; // Указатель на сам датчик
    float current_value;
    float threshold_value;
    AlarmLevel level;
    time_t timestamp;
};

// Абстрактный класс (интерфейс) для всех подписчиков
class IAlarmSubscriber {
public:
    virtual ~IAlarmSubscriber() = default;
    // Чистый виртуальный метод, который должен будет реализовать каждый подписчик
    virtual void onAlarm(const AlarmEvent& event) = 0;

    virtual void forceResetAlarm() {}

};

#endif //SSVC_OPEN_CONNECT_IALARMSUBSCRIBER_H
