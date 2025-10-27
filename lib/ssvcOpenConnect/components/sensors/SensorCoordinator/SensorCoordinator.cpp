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

#include "SensorCoordinator.h"
#include "esp_log.h"
#include "components/sensors/SensorManager/SensorManager.h"
#include "core/AlarmMonitor/AlarmMonitor.h"
#include "core/StatefulServices/SensorDataService/SensorDataService.h"


#define SENSOR_POLL_TIMER_NAME "SensorPollTimer"

SensorCoordinator& SensorCoordinator::getInstance() {
    static SensorCoordinator instance;
    return instance;
}

void SensorCoordinator::pollTimerCallback(const TimerHandle_t xTimer) {
    const auto self = static_cast<SensorCoordinator*>(pvTimerGetTimerID(xTimer));
    if (self) {
        // Вызываем основной метод оркестратора
        self->executePollCycle();
    }
}

void SensorCoordinator::startPolling(const uint32_t periodMs) {
    if (_pollTimerHandle) {
        ESP_LOGV(TAG, "Polling timer already created. Restarting.");
        xTimerStop(_pollTimerHandle, 0);
        xTimerChangePeriod(_pollTimerHandle, pdMS_TO_TICKS(periodMs), 0);
        xTimerStart(_pollTimerHandle, 0);
        return;
    }

    const TickType_t periodTicks = pdMS_TO_TICKS(periodMs);

    // 1. Создание таймера
    _pollTimerHandle = xTimerCreate(
        SENSOR_POLL_TIMER_NAME,
        periodTicks,
        pdTRUE,
        this,
        pollTimerCallback
    );

    if (_pollTimerHandle == nullptr) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS Sensor Poll Timer!");
        return;
    }
    if (xTimerStart(_pollTimerHandle, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start FreeRTOS Sensor Poll Timer!");
    } else {
        ESP_LOGV(TAG, "FreeRTOS Sensor Poll Timer started, interval: %lu ms", periodMs);
    }
}

void SensorCoordinator::registerPollingSubsystem(PollingSubsystem* subsystem) {
    if (subsystem) {
        subsystem->initialize();
        _pollingSubsystems.push_back(subsystem);
    }
}

void SensorCoordinator::executePollCycle() const
{
    if (_pollingSubsystems.empty()) {
        ESP_LOGV(TAG, "No polling subsystems registered. Skipping poll cycle.");
        return;
    }
    ESP_LOGV(TAG, "Starting sensor poll cycle for %zu subsystems.", _pollingSubsystems.size());
    // 1. СБОР ДАННЫХ (Вызов poll() у каждой подсистемы) ---
    for (PollingSubsystem* subsystem : _pollingSubsystems) {
        subsystem->poll();
    }
    //  2. ПУБЛИКАЦИЯ ДАННЫХ (Централизованно через SensorManager)
    // SensorManager собирает данные из AbstractSensor'ов и обновляет SensorDataService.
    SensorManager::getInstance().processReadingsAndPublish();

    ESP_LOGV(TAG, "Sensor poll cycle finished. Data published.");
}
#include "SensorCoordinator.h"