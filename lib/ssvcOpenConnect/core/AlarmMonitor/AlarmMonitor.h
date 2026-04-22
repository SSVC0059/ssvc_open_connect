//
// Created by Admin on 16/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_ALARMMONITOR_H
#define SSVC_OPEN_CONNECT_ALARMMONITOR_H

#include <vector>
#include <memory>
#include <algorithm>
#include <string>
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

    /** I2C / hardware faults (deduplicated by role+address). */
    void raiseHardwareFault(HardwareFaultCode code, uint8_t i2c_addr, const char* device_role);
    void clearHardwareFault(const char* device_role, uint8_t i2c_addr);
    std::vector<std::string> getKnownHardwareRoles() const;

    /** Entry describing a currently ACTIVE (non-NORMAL) hardware fault. */
    struct HardwareFaultEntry {
        std::string role;
        uint8_t i2c_addr = 0;
        HardwareFaultCode code = HardwareFaultCode::NONE;
    };

    /** Returns all hardware faults that are currently active (level != NORMAL). */
    std::vector<HardwareFaultEntry> getActiveHardwareFaults() const;

private:
    AlarmMonitor() {} // Приватный конструктор

    // Внутреннее состояние каждого датчика, чтобы избежать "дребезга" уведомлений
    std::map<std::string, AlarmLevel> _last_alarm_states;
    std::map<std::string, AlarmLevel> _last_hw_alarm_states;
    /** Maps the same hw key to the fault code so we can answer "what code is active here". */
    std::map<std::string, HardwareFaultCode> _hw_fault_active_codes;

    void notifySubscribers(const AlarmEvent& event) const;

    static std::string makeHardwareStateKey(const char* device_role, uint8_t i2c_addr);

    AlarmThresholdService* _thresholdService = nullptr; // Указатель на сервис с настройками
    std::vector<IAlarmSubscriber*> _subscribers;
    update_handler_id_t _updateHandlerId = 0; // ID обработчика для отписки

    static constexpr auto TAG = "ALARM_MONITOR";

    // Значение, которое используется для отслеживания неудачных измерений
    static constexpr float FAILURE_THRESHOLD_VALUE = -999.0f;
};

#endif //SSVC_OPEN_CONNECT_ALARMMONITOR_H