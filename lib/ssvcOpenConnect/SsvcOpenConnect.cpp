//
// Created by demoncat on 23.12.2024.
//

#include "SsvcOpenConnect.h"


SsvcOpenConnect::SsvcOpenConnect(PsychicHttpServer& server,
                                 EventSocket *socket,
                                 SecurityManager* securityManager) : _server(server),
                                                                     _socket(socket),
                                                                     _securityManager(securityManager),
                                                                     _ssvcConnector(SsvcConnector::getConnector()),
                                                                     _ssvcSettings(SsvcSettings::init(_ssvcConnector)),
                                                                     rProcess(
                                                                             RectificationProcess::createRectification
                                                                                     (
                                                                                         _ssvcConnector,
                                                                                         _ssvcSettings
                                                                                     )),
                                                                     _ssvcTelemetryService(
                                                                             SsvcTelemetryService(
                                                                                     _socket,
                                                                                     _securityManager,
                                                                                     rProcess)
                                                                     ),
                                                                     httpRequestHandler(
                                                                             HttpRequestHandler(server,
                                                                                                _securityManager,
                                                                                                rProcess,
                                                                                                _ssvcConnector,
                                                                                                _ssvcSettings)
                                                                     )

{
    waitingCommandResponse = false;
}

void SsvcOpenConnect::begin() {
    ESP_LOGI("SsvcOpenConnect", "Start SsvcOpenConnect begin()");
    _ssvcTelemetryService.begin();
    httpRequestHandler.begin();

//    Задача отправки команды AT
    ESP_LOGI("SsvcOpenConnect", "Start taskATCommandSend");
    xTaskCreatePinnedToCore(
            SsvcOpenConnect::taskATCommandSend,            // Function that should be called
            "AT_task",     // Name of the task (for debugging)
            2048,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (tskIDLE_PRIORITY),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );

    // Task for sending getSettings command
    ESP_LOGI("SsvcOpenConnect", "Start taskGetSettingsSend");
    xTaskCreatePinnedToCore(
            SsvcOpenConnect::taskGetSettingsSend,
            "GetSettings_task",
            4096,
            this,
            (tskIDLE_PRIORITY),
            nullptr,
            1
    );
}

void SsvcOpenConnect::taskGetSettingsSend(void *pvParameters) {
    auto* self = static_cast<SsvcOpenConnect*>(pvParameters);
    const char *taskName = pcTaskGetName(nullptr);
    ESP_LOGI("GetSettingsTask", "Task %s started", taskName);
    vTaskDelay(pdMS_TO_TICKS(1000*2));

    while (true) {
        UBaseType_t stackWaterMark = uxTaskGetStackHighWaterMark(nullptr);
        ESP_LOGI("SsvcOpenConnect", "Tak %s: Stack high water mark: %u",taskName, stackWaterMark);

        ESP_LOGI("GetSettingsTask", "Sending getSettings command");
        if (! self->waitingCommandResponse) {
            self->_ssvcConnector.sendGetSettingsCommand();
            self->waitingCommandResponse = true;
        }else {
            ESP_LOGV(
                    "SsvcOpenConnect",
                    "GetSettingsTask: Команда уже отправлена. Ожидаем ответа"
            );
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        EventBits_t bits = xEventGroupWaitBits(
                eventGroup,
                BIT10,
                pdTRUE,       // Сбрасываем флаг после обработки
                pdFALSE,      // Любое событие
                pdMS_TO_TICKS(3000)
        );

        if (bits & BIT10) {
            self->waitingCommandResponse = false;
            std::string _message;
            if (xSemaphoreTake(mutex, portMAX_DELAY)) {
                _message = self->_ssvcConnector.getLastMessage();
                self->_ssvcSettings.load(_message);
                xSemaphoreGive(mutex);// Копирование данных
            } else {
                ESP_LOGE("RectificationProcess", "Не удалось захватить мьютекс для _message");
                return;
            }
            ESP_LOGV("SsvcOpenConnect", "Deleting taskGetSettingsSend");
            vTaskDelete(nullptr);

        } else {
            // Бит не был получен в течение указанного времени
            self->waitingCommandResponse = false;
        }


    }
}

[[noreturn]] void SsvcOpenConnect::taskATCommandSend(void* pvParameters) {
    auto* self = static_cast<SsvcOpenConnect*>(pvParameters);
    const char *taskName = pcTaskGetName(nullptr);
    ESP_LOGI("taskATCommandSend", "Task %s started", taskName);
    vTaskDelay(pdMS_TO_TICKS(1000*2));
    while (true) {
        UBaseType_t stackWaterMark = uxTaskGetStackHighWaterMark(nullptr);
//        ESP_LOGI("SsvcOpenConnect", "AT command water mark: %u", stackWaterMark);
        if (! self->waitingCommandResponse) {
            self->_ssvcConnector.sendAtCommand();
            self->waitingCommandResponse = true;
        }else {
            ESP_LOGV(
                    "SsvcOpenConnect",
                    "taskATCommandSend: Команда уже отправлена. Ожидаем ответа"
            );
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        EventBits_t bits = xEventGroupWaitBits(
                eventGroup,
                BIT15,
                pdTRUE,       // Сбрасываем флаг после обработки
                pdFALSE,      // Любое событие
                pdMS_TO_TICKS(3000)
        );
        if ((bits & BIT15) != 0) {
            // Нужный бит был получен. Команда отработана.
            self->waitingCommandResponse = false;
//            ESP_LOGI("SsvcOpenConnect", "Получен ответ на команду AT");
            vTaskDelay(pdMS_TO_TICKS(1000*8));
        } else {
            // Бит не был получен в течение указанного времени
//            TODO AT Может не ответить. Это нужно как то обработать
            self->waitingCommandResponse = false;
        }
    }
}
