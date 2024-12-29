#ifndef SSVCCONNECTOR_H
#define SSVCCONNECTOR_H

#include <Arduino.h>
#include <driver/uart.h>
#include <ArduinoJson.h>
#include <iostream>
#include "driver/gpio.h"

#include <utility>
#include <vector>
#include <sstream>
#include <algorithm>

// Определение минимальной поддерживаемой версии
#define SSVC_MIN_SUPPORT_VERSION "2.2.38"

// UART CONFIGS

#define SSVC_OPEN_CONNECT_UART_NUM UART_NUM_1
#ifndef SSVC_OPEN_CONNECT_UART_TX
#define SSVC_OPEN_CONNECT_UART_TX GPIO_NUM_17
#endif
#ifndef SSVC_OPEN_CONNECT_UART_RX
#define SSVC_OPEN_CONNECT_UART_RX GPIO_NUM_16
#endif

constexpr size_t SSVC_OPEN_CONNECT_BUF_SIZE = 2048;

extern SemaphoreHandle_t mutex;

class SsvcConnector {
public:

    // Получение единственного экземпляра класса (паттерн Singleton)
    static SsvcConnector& getConnector(EventGroupHandle_t eventGroup);

    // Метод для получения данных из message
    template <typename T>
    T getValue(const char* key) {
        if (_message[key] && _message[key].is<T>()) {
            return _message[key].as<T>();
        }
        return T();  // Возвращаем значение по умолчанию, если ключ не найден или тип не совпадает
    }

    JsonDocument& getMessage() {return _message;}
    String getSsvcVersion() {return _ssvcVersion; }
    JsonDocument& getSsvcSettings() {return _ssvcSettings; }
    bool uartCommunicationError;
    void checkVersionSupported();
    bool isSupportSSVCVersion;

//    Поддержка команд SSVC
    bool taskGetSettingsCommand();
    bool taskVersionCommand();
    bool taskStopCommand();
    bool taskPauseCommand();
    bool taskResumeCommand();
    bool taskNestCommand();
    bool taskATCommand();

//    static bool sendPauseCommand();
//    static bool sendResumeCommand();
//    static bool sendNextCommand();

    void begin();

    // Установка флага выполнения команды
    void setCommandInProgress(bool inProgress) {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _isCommandInProgress = inProgress;
        xSemaphoreGive(_mutex);
    }

    // Проверка состояния выполнения команды
    bool isCommandInProgress() {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        bool status = _isCommandInProgress;
        xSemaphoreGive(_mutex);
        return status;
    }

private:
    // Приватный конструктор
    explicit SsvcConnector(EventGroupHandle_t eventGroup);
    ~SsvcConnector();

    // Метод для обновления данных в message
    void updateMessage(const JsonDocument& jsonStr);
    void updateSettings(const JsonDocument& ssvcSettings);
    void updateVersion(const JsonDocument& ssvcVersion);

    //    Метод запуска задачи с отпавкой команды и ожидания ее получения
    void initSsvcController();

    [[noreturn]] static void _startTask(void *pvParameters);

    [[noreturn]] static void _telemetry(void* pvParameters);
    static bool sendCommand(const char *command);

    // Поля класса
    static SsvcConnector* _ssvcConnector;
    EventGroupHandle_t _eventGroup;
    SemaphoreHandle_t _mutex;

    // Хранение данных
    JsonDocument _message;
    JsonDocument  _ssvcSettings;
    String _ssvcVersion;
    bool _isCommandInProgress = false; // Флаг выполнения команды

    static std::vector<int> parseVersion(const std::string& version);

    struct TaskParameters {
        SsvcConnector* connector;                               // Указатель на экземпляр SsvcConnector
        std::function<void(SsvcConnector*)> commandFunction;    // Команда, которую нужно выполнить
        String expectedRequest;                                 // Ожидаемый тип команды в ответе
        std::function<void(SsvcConnector*, JsonDocument&)> responseHandler; // Обработчик ответа
        bool need_deleteTask;
    };

//    Команды SSVC
    static bool sendGetSettingsCommand();
    static bool sendVersionCommand();
    static bool sendStopCommand();
    static bool sendPauseCommand();
    static bool sendResumeCommand();
    static bool sendNextCommand();

    __attribute__((unused)) static bool sendAtCommand();

    bool createTask(
            const char* taskName,
            const char* command,
            const char* expectedResponse,
            std::function<void(SsvcConnector*)> commandFunction,
            std::function<void(SsvcConnector*, JsonDocument&)> responseHandler,
            size_t stackSize = 2048,
            UBaseType_t priority = tskIDLE_PRIORITY,
            BaseType_t core = 1)
    {
        auto params = new TaskParameters{
                this,
                std::move(commandFunction),
                expectedResponse,
                std::move(responseHandler),
                true
        };

        if (xTaskCreatePinnedToCore(
                SsvcConnector::_startTask,
                taskName,
                stackSize,
                params,
                priority,
                nullptr,
                core) != pdPASS) {
            delete params; // Удаляем параметры, если задача не была создана
            return false;
        }

        return true;
    }
};

#endif
