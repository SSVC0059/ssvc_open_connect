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

#ifndef SSVC_OPEN_CONNECT_LOGSUBSCRIBER_H
#define SSVC_OPEN_CONNECT_LOGSUBSCRIBER_H

#include <Arduino.h>
#include "core/IAlarmSubscriber/IAlarmSubscriber.h"
#include "ESP32SvelteKit.h"
#include "core/AlarmMonitor/AlarmMonitor.h"
#include "NotificationService.h"
#include <freertos/timers.h>

class NotificationSubscriber final : public IAlarmSubscriber {
public:
    // Измените конструктор для приема указателя на ESP32SvelteKit
    explicit NotificationSubscriber(ESP32SvelteKit* svelteKit);
    ~NotificationSubscriber() override;
    void onAlarm(const AlarmEvent& event) override;
    void forceResetAlarm() override;

private:
    // Интервал повторного уведомления
    static constexpr TickType_t RE_NOTIFICATION_INTERVAL_MS = 10000 / portTICK_PERIOD_MS;

    ESP32SvelteKit* _sveltekit; // Сохраняем указатель для использования в onAlarm
    static constexpr auto TAG = "ALARM_LOGGER";

    TimerHandle_t _reAlarmTimer = nullptr; // Хэндл таймера FreeRTOS
    AlarmEvent _lastActiveAlarm{};           // Сохраняем последнее активное событие тревоги
    bool _isAlarmActive = false;           // Флаг, что тревога активна (уровень != NORMAL)

    // Статическая функция-коллбэк для таймера FreeRTOS
    static void reAlarmTimerCallback(TimerHandle_t xTimer); // <-- Должен принимать TimerHandle_t
    void reAlarmTimerAction() const;

    // Вспомогательный метод для отправки фактического уведомления
    void sendNotification(const AlarmEvent& event) const;
};

#endif //SSVC_OPEN_CONNECT_LOGSUBSCRIBER_H