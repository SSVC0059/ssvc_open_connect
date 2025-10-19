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