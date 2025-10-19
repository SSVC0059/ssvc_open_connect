//
// Created by Admin on 16/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_ALARMMONITOR_H
#define SSVC_OPEN_CONNECT_ALARMMONITOR_H

#include <vector>
#include <memory>
#include <algorithm>
#include <Arduino.h>
#include "core/IAlarmSubscriber/IAlarmSubscriber.h"
#include "core/StatefulServices/AlarmThresholdService/AlarmThresholdService.h"
#include "components/sensors/SensorManager/SensorManager.h"


class AlarmMonitor {
public:
    static AlarmMonitor& getInstance();

    // Метод для инициализации монитора
    void initialize(AlarmThresholdService* service);

    // Подписка/отписка
    void subscribe(IAlarmSubscriber* subscriber);
    void unsubscribe(IAlarmSubscriber* subscriber);

    // Главный метод, который будет вызываться после опроса датчиков
    void checkAllSensors();

    void onThresholdsUpdated(const String& originId);

private:
    AlarmMonitor() {} // Приватный конструктор

    // Внутреннее состояние каждого датчика, чтобы избежать "дребезга" уведомлений
    std::map<std::string, AlarmLevel> _last_alarm_states;

    void notifySubscribers(const AlarmEvent& event) const;

    AlarmThresholdService* _thresholdService = nullptr; // Указатель на сервис с настройками
    std::vector<IAlarmSubscriber*> _subscribers;
    update_handler_id_t _updateHandlerId = 0; // ID обработчика для отписки

    static constexpr auto TAG = "ALARM_MONITOR";

    // Значение, которое используется для отслеживания неудачных измерений
    static constexpr float FAILURE_THRESHOLD_VALUE = -999.0f;
};

#endif //SSVC_OPEN_CONNECT_ALARMMONITOR_H