//
// Created by Admin on 15/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_POLLINGSUBSYSTEM_H
#define SSVC_OPEN_CONNECT_POLLINGSUBSYSTEM_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <stdint.h>
#include <string.h> // Для strdup

#include "ESP32SvelteKit.h"

#ifndef POLLING_SUBSYSTEM_TAG
#define POLLING_SUBSYSTEM_TAG "POLLING_SUB"
#endif

/**
 * @brief Абстрактный базовый класс для подсистем, требующих периодического опроса в RTOS.
 */
class PollingSubsystem {
public:
    // Удаляем константные параметры RTOS из класса, делая их настраиваемыми

    // Чисто виртуальные методы, которые должны быть реализованы потомками

    /**
     * @brief Инициализирует аппаратное обеспечение и выполняет поиск устройств.
     * @return true, если инициализация прошла успешно.
     */
    virtual bool initialize() = 0;

    /**
     * @brief Выполняет одну итерацию опроса/обновления данных.
     */
    virtual void poll() = 0;

    /**
     * @brief Запускает RTOS-задачу.
     */
    void begin() {
        if (taskHandle) {
            ESP_LOGV(POLLING_SUBSYSTEM_TAG, "Task %s already running. Ignoring begin() call.", taskName);
            return;
        }

        ESP_LOGV(POLLING_SUBSYSTEM_TAG, "Attempting to initialize subsystem %s.", taskName);
        if (!taskHandle) {
            if (initialize()) {
                // Используем xTaskCreatePinnedToCore
                BaseType_t result =xTaskCreatePinnedToCore(
                    taskEntry,
                    taskName,
                    taskStackSize,
                    this,
                    taskPriority,
                    &taskHandle,
                    coreID // <-- Передаем Core ID
                );
                if (result == pdPASS) {
                    ESP_LOGV(POLLING_SUBSYSTEM_TAG, "Task %s created on core %d with priority %u and stack %lu.",
                             taskName, coreID, taskPriority, taskStackSize);
                } else {
                    ESP_LOGE(POLLING_SUBSYSTEM_TAG, "Failed to create task %s. Error code: %ld.", taskName, result);
                }
            }
        } else {
            ESP_LOGE(POLLING_SUBSYSTEM_TAG, "Initialization failed for subsystem %s.", taskName);
        }
    }

    /**
     * @brief Остановка и удаление RTOS-задачи.
     */
    virtual ~PollingSubsystem() {
        ESP_LOGW(POLLING_SUBSYSTEM_TAG, "Destroying PollingSubsystem %s.", taskName);

        if (taskHandle) {
            vTaskDelete(taskHandle);
            taskHandle = nullptr;
            ESP_LOGW(POLLING_SUBSYSTEM_TAG, "RTOS Task %s deleted.", taskName);
        }
        if (mutex) {
            vSemaphoreDelete(mutex);
            mutex = nullptr;
            ESP_LOGD(POLLING_SUBSYSTEM_TAG, "Mutex deleted.");
        }
        if (taskName) {
            free(taskName);
            // taskName = nullptr; // Необязательно
        }
    }

protected:
    char* taskName = nullptr;
    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t mutex = nullptr;
    const uint32_t taskStackSize = 4096;
    const uint32_t coreID = 0;
    const UBaseType_t taskPriority = 0;
    const TickType_t pollIntervalTicks = pdMS_TO_TICKS(1000); // Интервал по умолчанию

    /**
     * @brief Конструктор. Инициализирует общие ресурсы.
     */
    PollingSubsystem(const char* name,
        const TickType_t interval,
        const uint32_t stackSize = 4096,
        const UBaseType_t priority = 5)
        : taskStackSize(stackSize), taskPriority(priority), pollIntervalTicks(interval) {

        taskName = strdup(name); // Копирование имени задачи
        mutex = xSemaphoreCreateMutex();
        ESP_LOGD(POLLING_SUBSYSTEM_TAG, "Subsystem %s constructed. Interval: %lu ms, Stack: %lu, Priority: %u.",
                 taskName, pdTICKS_TO_MS(pollIntervalTicks), taskStackSize, taskPriority);
    }

private:
    // Статическая точка входа для RTOS-задачи
    [[noreturn]] static void taskEntry(void* parameter) {
        const auto self = static_cast<PollingSubsystem*>(parameter);

        ESP_LOGV(POLLING_SUBSYSTEM_TAG, "Task %s loop started.", self->taskName);

        for(;;) {
            // Берем мьютекс для защиты общих ресурсов (poll() и dallasTemp)
            if (self->mutex && xSemaphoreTake(self->mutex, portMAX_DELAY) == pdTRUE) {

                ESP_LOGD(POLLING_SUBSYSTEM_TAG, "Task %s: Mutex acquired, calling poll().", self->taskName);

                self->poll();

                xSemaphoreGive(self->mutex);
                ESP_LOGD(POLLING_SUBSYSTEM_TAG, "Task %s: Mutex released.", self->taskName);

            } else {
                ESP_LOGE(POLLING_SUBSYSTEM_TAG, "Task %s: Failed to acquire mutex!", self->taskName);
            }

            // Ждем до следующего цикла
            vTaskDelay(self->pollIntervalTicks);
        }
    }
};


#endif //SSVC_OPEN_CONNECT_POLLINGSUBSYSTEM_H