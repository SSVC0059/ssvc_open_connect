//
// Created by Admin on 16/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_ALARMMONITOR_H
#define SSVC_OPEN_CONNECT_ALARMMONITOR_H

#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <string>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "core/IAlarmSubscriber/IAlarmSubscriber.h"
#include "core/StatefulServices/AlarmThresholdService/AlarmThresholdService.h"
#include "components/sensors/SensorManager/SensorManager.h"

// Сокет объявлен, а не включён: EventSocket.h тянет ESPAsyncWebServer и нужен
// только в .cpp, а заголовок подключают и там, где сокета нет.
class EventSocket;


class AlarmMonitor {
public:
    static AlarmMonitor& getInstance();

    // Метод для инициализации монитора
    void initialize(AlarmThresholdService* service);

    /**
     * @brief Куда отправлять событие `alarm` (канал /ws/events).
     *
     * nullptr — штатное состояние (тесты, сборка без веб-сокета): событие не
     * отправляется, уведомления подписчикам и REST работают как раньше.
     * Регистрация имени события — в initialize().
     */
    void setEventSocket(EventSocket* socket);

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
    AlarmMonitor(); // Приватный конструктор

    // Внутреннее состояние каждого датчика, чтобы избежать "дребезга" уведомлений
    std::map<std::string, AlarmLevel> _last_alarm_states;
    std::map<std::string, AlarmLevel> _last_hw_alarm_states;
    /** Maps the same hw key to the fault code so we can answer "what code is active here". */
    std::map<std::string, HardwareFaultCode> _hw_fault_active_codes;

    // Не const: помимо рассылки подписчикам метод отправляет событие `alarm`
    // и запоминает последнее отправленное состояние (дедупликация повторов).
    void notifySubscribers(const AlarmEvent& event);

    static std::string makeHardwareStateKey(const char* device_role, uint8_t i2c_addr);

    // --- Событие `alarm` для /ws/events -------------------------------------
    // Кадр собирается из AlarmEvent и отправляется только на переход:
    // raise/clear для hardware, смена уровня для датчиков. Таблица ниже
    // отсекает повторную отправку того же состояния (например, повторный
    // raise на каждом опросе шины или повтор после forceResetAlarm).
    void publishAlarmEvent(const AlarmEvent& event);
    bool rememberAlarmEventState(const char* key, uint8_t state);

    struct LastEmittedAlarmState {
        char key[48] = {};
        // AlarmLevel для датчика, HardwareFaultCode для hardware; 0 = «нет
        // активного состояния» (NORMAL/NONE).
        uint8_t state = 0;
    };
    // Фиксированный размер: таблица не растёт с числом опросов, ключи —
    // обычные char-массивы, без std::string и фрагментации кучи на кадр.
    static constexpr size_t kMaxLastEmittedAlarmStates = 32;
    LastEmittedAlarmState _lastEmittedAlarmStates[kMaxLastEmittedAlarmStates];
    size_t _lastEmittedAlarmCount = 0;
    size_t _nextLastEmittedAlarmSlot = 0;

    EventSocket* _eventSocket = nullptr;

    AlarmThresholdService* _thresholdService = nullptr; // Указатель на сервис с настройками
    std::vector<IAlarmSubscriber*> _subscribers;
    update_handler_id_t _updateHandlerId = 0; // ID обработчика для отписки
    mutable SemaphoreHandle_t _lock = nullptr;

    static constexpr auto TAG = "ALARM_MONITOR";

    // Значение, которое используется для отслеживания неудачных измерений
    static constexpr float FAILURE_THRESHOLD_VALUE = -999.0f;
};

#endif //SSVC_OPEN_CONNECT_ALARMMONITOR_H