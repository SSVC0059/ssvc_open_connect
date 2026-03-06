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

#include "NotificationSubscriber.h"

// Принимаем указатель на SvelteKit и сохраняем его
NotificationSubscriber::NotificationSubscriber(ESP32SvelteKit* svelteKit)
    : _sveltekit(svelteKit) // Инициализация члена класса
{
    // Подписка на AlarmMonitor остается
    AlarmMonitor::getInstance().subscribe(this);
    ESP_LOGI(TAG, "AlarmLogger subscribed to AlarmMonitor events.");
}

NotificationSubscriber::~NotificationSubscriber() {
    // Отписка от AlarmMonitor
    AlarmMonitor::getInstance().unsubscribe(this);

    if (_reAlarmTimer != nullptr) {
        // Сначала останавливаем, если активен
        if (xTimerIsTimerActive(_reAlarmTimer) != pdFALSE) {
            xTimerStop(_reAlarmTimer, 0);
        }
        xTimerDelete(_reAlarmTimer, 0);
        _reAlarmTimer = nullptr;
        ESP_LOGI(TAG, "ReAlarmTimer deleted.");
    }
}

// NotificationSubscriber.cpp
void NotificationSubscriber::forceResetAlarm() {
    if (_reAlarmTimer != nullptr && xTimerIsTimerActive(_reAlarmTimer) != pdFALSE) {
        xTimerStop(_reAlarmTimer, 0);
        ESP_LOGW(TAG, "Alarm timer FORCIBLY STOPPED due to threshold update."); // <-- Лог
    }
    _isAlarmActive = false;
    _lastActiveAlarm = {}; // Очистка кэша (если нужно, но необязательно, т.к. таймер остановлен)
}


void NotificationSubscriber::reAlarmTimerAction() const
{
    if (!_isAlarmActive) {
        if (_reAlarmTimer != nullptr && xTimerIsTimerActive(_reAlarmTimer) != pdFALSE) {
            xTimerStop(_reAlarmTimer, 0);
        }
        ESP_LOGW(TAG, "Re-notification timer fired, but alarm is not active. Timer stopped.");
        return;
    }

    const AbstractSensor* sensor = _lastActiveAlarm.sensor;
    if (sensor == nullptr) {
        ESP_LOGE(TAG, "Re-notification failed: Sensor pointer is NULL.");
        return;
    }

    const float current_value = sensor->getData();

    ESP_LOGW(TAG, "reAlarmTimerAction(): [ALARM_LOGGER] Re-notification triggered by timer for sensor %s. Actual value: %.2f C (Cached: %.2f C).",
             sensor->getName().c_str(), current_value, _lastActiveAlarm.current_value);

    AlarmEvent currentEvent = _lastActiveAlarm; // Копируем старое событие
    currentEvent.current_value = current_value;  // Обновляем только значение на актуальное
    currentEvent.timestamp = time(nullptr);     // Обновляем время

    sendNotification(currentEvent);
}

void NotificationSubscriber::onAlarm(const AlarmEvent& event) {
    const bool eventIsAlarm = event.level != AlarmLevel::NORMAL;

    // 1. Инициализация таймера при первом вызове
    if (_reAlarmTimer == nullptr) {
        _reAlarmTimer = xTimerCreate(
            "ReAlarmTimer",
            RE_NOTIFICATION_INTERVAL_MS,
            pdTRUE, // pdTRUE для периодического
            (void*)this,
            reAlarmTimerCallback
        );
        ESP_LOGI(TAG, "ReAlarmTimer created.");
    }

    // 2. Логика управления тревогами
    if (eventIsAlarm) {
        // Тревога активна (CRITICAL/DANGEROUS/MIN)

        // Отправляем первое уведомление немедленно
        sendNotification(event);

        // Сохраняем событие для повторных уведомлений
        _lastActiveAlarm = event;
        _isAlarmActive = true;

        // Запускаем таймер, если он еще не запущен
        if (xTimerIsTimerActive(_reAlarmTimer) == pdFALSE) {
            xTimerStart(_reAlarmTimer, 0);
            ESP_LOGI(TAG, "Alarm timer started for periodic re-notifications.");
        }

    } else {
        // Тревога сброшена (NORMAL)

        // Останавливаем таймер
        if (xTimerIsTimerActive(_reAlarmTimer) != pdFALSE) {
            xTimerStop(_reAlarmTimer, 0);
            ESP_LOGI(TAG, "Alarm timer stopped: alarm level is NORMAL.");
        }

        _isAlarmActive = false;
        // О сбросе тревоги уведомляем только если это был переход из состояния тревоги
        if (_lastActiveAlarm.level != AlarmLevel::NORMAL) {
            // Здесь можно отправить специальное уведомление о сбросе,
            // но по ТЗ нужно только периодически уведомлять о превышении,
            // поэтому просто очищаем состояние.
            ESP_LOGI(TAG, "Alarm reset for sensor %s.", event.sensor->getName().c_str());
        }
    }
}

void NotificationSubscriber::reAlarmTimerCallback(TimerHandle_t xTimer) {
    auto* self = static_cast<NotificationSubscriber*>(pvTimerGetTimerID(xTimer));
    if (!self || !self->_isAlarmActive) return;

    const float current_value = self->_lastActiveAlarm.sensor->getData();
    const float threshold = self->_lastActiveAlarm.threshold_value;
    const AlarmLevel level = self->_lastActiveAlarm.level;

    // Если текущее значение больше не нарушает порог, который вызвал эту тревогу
    // Проверяем, нарушен ли порог
    const bool still_violating = (
        (level == AlarmLevel::MIN && current_value <= threshold) ||
        ((level == AlarmLevel::DANGEROUS || level == AlarmLevel::CRITICAL) && current_value >= threshold)
    );

    if (!still_violating) {
        ESP_LOGI(self->TAG, "Timer check: Sensor %s recovered (Val: %.2f). Stopping timer.",
                 self->_lastActiveAlarm.sensor->getName().c_str(), current_value);
        self->_isAlarmActive = false;
        xTimerStop(xTimer, 0);
        return;
    }

    AlarmEvent currentEvent = self->_lastActiveAlarm;
    currentEvent.current_value = current_value;
    self->sendNotification(currentEvent);
}

void NotificationSubscriber::sendNotification(const AlarmEvent& event) const
{
    const char* level_str;
    pushType push_level;

    switch(event.level) {
    case AlarmLevel::MIN:
        level_str = "MINIMUM";
        push_level = PUSHINFO;
        break;
    case AlarmLevel::DANGEROUS:
        level_str = "DANGEROUS";
        push_level = PUSHWARNING;
        break;
    case AlarmLevel::CRITICAL:
        level_str = "CRITICAL";
        push_level = PUSHERROR;
        break;
    case AlarmLevel::NORMAL:
    default:
        return; // Не уведомляем о нормальном уровне
    }

    char message_buffer[128];

    // Проверяем, является ли событие CRITICAL, вызванным СБОЕМ ДАТЧИКА
    // (на основе символического порога).
    const bool is_sensor_failure = (event.level == AlarmLevel::CRITICAL && event.threshold_value < -998.0f);

    if (is_sensor_failure) {
        // Сообщение о СБОЕ ДАТЧИКА
        snprintf(message_buffer, sizeof(message_buffer),
            "%s: КРИТИЧЕСКИЙ СБОЙ! Датчик '%s' неисправен или отключен. Value: %.2f",
            level_str, event.sensor->getName().c_str(), event.current_value);
    } else {
        // Стандартное сообщение о ПЕРЕХОДЕ ПОРОГА
        snprintf(message_buffer, sizeof(message_buffer),
                 "%s: Тревога! для датчика '%s'! Value: %.2f, Threshold: %.2f",
                 level_str, event.sensor->getName().c_str(), event.current_value, event.threshold_value);
    }

    if (_sveltekit && _sveltekit->getNotificationService()) {
        _sveltekit->getNotificationService()->pushNotification(message_buffer, push_level);
        ESP_LOGW("ALARM_NOTIF", "%s: %s", level_str, message_buffer);
    } else {
        ESP_LOGE("ALARM_NOTIF", "Failed to send notification: ESP32SvelteKit or NotificationService is NULL.");
    }
}
