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

#include "PinOutSubscriber.h"

// Принимаем указатель на SvelteKit и сохраняем его
PinOutSubscriber::PinOutSubscriber()
{
    // Подписка на AlarmMonitor остается
    AlarmMonitor::getInstance().subscribe(this);
    ESP_LOGI(TAG, "AlarmLogger subscribed to AlarmMonitor events.");
    pinMode(DANGEROUS_PIN, OUTPUT);
    pinMode(CRITICAL_PIN, OUTPUT);
    digitalWrite(DANGEROUS_PIN, LOW);
    digitalWrite(CRITICAL_PIN, LOW);
}

PinOutSubscriber::~PinOutSubscriber() {
    // Отписка от AlarmMonitor
    AlarmMonitor::getInstance().unsubscribe(this);

}

// PinOutSubscriber.cpp
void PinOutSubscriber::forceResetAlarm() {
    digitalWrite(DANGEROUS_PIN, LOW);
    digitalWrite(CRITICAL_PIN, LOW);
}


void PinOutSubscriber::onAlarm(const AlarmEvent& event) {
    // Сбрасываем оба пина перед установкой нового состояния
    digitalWrite(DANGEROUS_PIN, LOW);
    digitalWrite(CRITICAL_PIN, LOW);

    if (event.level == AlarmLevel::DANGEROUS) {
        digitalWrite(DANGEROUS_PIN, HIGH);
    } else if (event.level == AlarmLevel::CRITICAL) {
        digitalWrite(CRITICAL_PIN, HIGH);
    }
}

