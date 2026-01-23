//
// Created by Admin on 22/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_SENSORCOORDINATOR_H
#define SSVC_OPEN_CONNECT_SENSORCOORDINATOR_H

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

#include <vector>
#include "components/sensors/PollingSubsystem/PollingSubsystem.h"
// PollingSubsystem теперь должен быть чистым интерфейсом (без RTOS-задачи)

class SensorCoordinator final {
public:
    // --- Singleton Access ---
    static SensorCoordinator& getInstance();
    SensorCoordinator(const SensorCoordinator&) = delete;
    void operator=(const SensorCoordinator&) = delete;

    /**
     * @brief Запускает Software Timer для периодического вызова executePollCycle.
     * @param periodMs Интервал опроса в миллисекундах.
     */
    void startPolling(uint32_t periodMs);

    /**
     * @brief Регистрирует подсистему для включения в централизованный цикл опроса.
     * @param subsystem Указатель на объект, реализующий PollingSubsystem.
     */
    void registerPollingSubsystem(PollingSubsystem* subsystem);

    /**
     * @brief Выполняет полный цикл сбора и публикации данных.
     * 1. Вызывает poll() у всех зарегистрированных подсистем (Сбор).
     * 2. Запускает SensorManager::processReadingsAndPublish() (Публикация).
     */
    void executePollCycle() const;

    // Тип функции для колбэка
    using OnFirstScanCallback = std::function<void()>;

    /**
     * @brief Регистрирует функцию, которая будет вызвана один раз после успешного первого опроса.
     */
    void onFirstScanComplete(OnFirstScanCallback cb);

private:
    SensorCoordinator() = default;

    TimerHandle_t _pollTimerHandle = nullptr;
    static void pollTimerCallback(TimerHandle_t xTimer);

    // Вектор указателей на подсистемы, которые нужно опрашивать.
    std::vector<PollingSubsystem*> _pollingSubsystems;

    static constexpr auto TAG = "SENSOR_COORDINATOR";

    bool _firstScanDone = false;
    std::vector<OnFirstScanCallback> _firstScanCallbacks;

    // Метод для вызова всех подписанных функций
    void notifyFirstScanDone();
};



#endif //SSVC_OPEN_CONNECT_SENSORCOORDINATOR_H