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
extern EventGroupHandle_t eventGroup;

class SsvcConnector {
public:
    static SsvcConnector& getConnector();

    std::string getLastMessage();

    bool uartCommunicationError;

//    Команды SSVC
    bool sendGetSettingsCommand();
    bool sendVersionCommand();
    bool sendStopCommand();
    bool sendPauseCommand();
    bool sendResumeCommand();
    bool sendNextCommand();
    bool sendAtCommand();
    bool sendSetCommand(const std::string& settings);

private:
    // Приватный конструктор
    explicit SsvcConnector();
    void InitUartDriver();
    // Метод для обновления данных в message
    void initSsvcController();

    [[noreturn]] static void _telemetry(void* pvParameters);
    static bool sendCommand(const char *command);

    // Поля класса
    static SsvcConnector* _ssvcConnector;

    // Хранение данных
    std::string lastMessage;
    std::string lastSettings;

    struct SpiRamAllocator : ArduinoJson::Allocator {
        void* allocate(size_t size) override {
            return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        }

        void deallocate(void* pointer) override {
            heap_caps_free(pointer);
        }

        void* reallocate(void* ptr, size_t new_size) override {
            return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
        }
    };
};

#endif
