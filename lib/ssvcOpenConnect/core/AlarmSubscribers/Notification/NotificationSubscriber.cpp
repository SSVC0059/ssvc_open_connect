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

NotificationSubscriber::NotificationSubscriber(ESP32SvelteKit* svelteKit)
    : _sveltekit(svelteKit)
{
    AlarmMonitor::getInstance().subscribe(this);
    ESP_LOGI(TAG, "AlarmLogger subscribed to AlarmMonitor events.");
}

NotificationSubscriber::~NotificationSubscriber() {
    AlarmMonitor::getInstance().unsubscribe(this);

    if (_reAlarmTimer != nullptr) {
        if (xTimerIsTimerActive(_reAlarmTimer) != pdFALSE) {
            xTimerStop(_reAlarmTimer, 0);
        }
        xTimerDelete(_reAlarmTimer, 0);
        _reAlarmTimer = nullptr;
        ESP_LOGI(TAG, "ReAlarmTimer deleted.");
    }
}

void NotificationSubscriber::forceResetAlarm() {
    if (_reAlarmTimer != nullptr && xTimerIsTimerActive(_reAlarmTimer) != pdFALSE) {
        xTimerStop(_reAlarmTimer, 0);
        ESP_LOGW(TAG, "Alarm timer FORCIBLY STOPPED due to threshold update.");
    }
    _isAlarmActive = false;
    _lastActiveAlarm = {};
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

    if (_lastActiveAlarm.source_kind == AlarmSourceKind::HARDWARE_FAULT) {
        sendNotification(_lastActiveAlarm);
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

    AlarmEvent currentEvent = _lastActiveAlarm;
    currentEvent.current_value = current_value;
    currentEvent.timestamp = time(nullptr);

    sendNotification(currentEvent);
}

void NotificationSubscriber::onAlarm(const AlarmEvent& event) {
    const bool eventIsAlarm = event.level != AlarmLevel::NORMAL;

    if (_reAlarmTimer == nullptr) {
        _reAlarmTimer = xTimerCreate(
            "ReAlarmTimer",
            RE_NOTIFICATION_INTERVAL_MS,
            pdTRUE,
            (void*)this,
            reAlarmTimerCallback
        );
        ESP_LOGI(TAG, "ReAlarmTimer created.");
    }

    if (eventIsAlarm) {
        sendNotification(event);

        _lastActiveAlarm = event;
        _isAlarmActive = true;

        if (xTimerIsTimerActive(_reAlarmTimer) == pdFALSE) {
            xTimerStart(_reAlarmTimer, 0);
            ESP_LOGI(TAG, "Alarm timer started for periodic re-notifications.");
        }

    } else {
        if (xTimerIsTimerActive(_reAlarmTimer) != pdFALSE) {
            xTimerStop(_reAlarmTimer, 0);
            ESP_LOGI(TAG, "Alarm timer stopped: alarm level is NORMAL.");
        }

        _isAlarmActive = false;
        if (_lastActiveAlarm.level != AlarmLevel::NORMAL) {
            if (event.source_kind == AlarmSourceKind::HARDWARE_FAULT) {
                ESP_LOGI(TAG, "Hardware fault cleared: role=%s",
                         event.hw_device_role ? event.hw_device_role : "?");
            } else if (event.sensor != nullptr) {
                ESP_LOGI(TAG, "Alarm reset for sensor %s.", event.sensor->getName().c_str());
            }
        }
    }
}

void NotificationSubscriber::reAlarmTimerCallback(TimerHandle_t xTimer) {
    auto* self = static_cast<NotificationSubscriber*>(pvTimerGetTimerID(xTimer));
    if (!self || !self->_isAlarmActive) return;

    if (self->_lastActiveAlarm.source_kind == AlarmSourceKind::HARDWARE_FAULT) {
        self->sendNotification(self->_lastActiveAlarm);
        return;
    }

    if (self->_lastActiveAlarm.sensor == nullptr) {
        ESP_LOGE(self->TAG, "Timer: last alarm has null sensor");
        return;
    }

    const float current_value = self->_lastActiveAlarm.sensor->getData();
    const float threshold = self->_lastActiveAlarm.threshold_value;
    const AlarmLevel level = self->_lastActiveAlarm.level;

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
    if (event.source_kind == AlarmSourceKind::HARDWARE_FAULT) {
        if (event.level == AlarmLevel::NORMAL) {
            return;
        }
        char message_buffer[160];
        snprintf(message_buffer, sizeof(message_buffer),
                 "CRITICAL: I2C/аппаратный сбой: %s @ 0x%02X code=%u",
                 event.hw_device_role ? event.hw_device_role : "?",
                 static_cast<unsigned>(event.hw_i2c_address),
                 static_cast<unsigned>(event.hw_code));
        if (_sveltekit && _sveltekit->getNotificationService()) {
            _sveltekit->getNotificationService()->pushNotification(message_buffer, PUSHERROR);
            ESP_LOGW("ALARM_NOTIF", "%s", message_buffer);
        }
        return;
    }

    if (event.sensor == nullptr) {
        ESP_LOGE("ALARM_NOTIF", "SENSOR notification with null sensor");
        return;
    }

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
        return;
    }

    char message_buffer[128];

    const bool is_sensor_failure = (event.level == AlarmLevel::CRITICAL && event.threshold_value < -998.0f);

    if (is_sensor_failure) {
        snprintf(message_buffer, sizeof(message_buffer),
            "%s: КРИТИЧЕСКИЙ СБОЙ! Датчик '%s' неисправен или отключен. Value: %.2f",
            level_str, event.sensor->getName().c_str(), event.current_value);
    } else {
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
