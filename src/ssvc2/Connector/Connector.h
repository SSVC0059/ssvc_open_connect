#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <Arduino.h>
#include <driver/uart.h>
#include "ArduinoJson.h"
#include "esp_timer.h"
#include "Logger.h"
#include "../TelemetryHandler/TelemetryHandler.h"

#define BUF_SIZE 2048

extern TelemetryHandler telemetryHandler;

class Connector {
public:
    Connector(
        uart_port_t uart_num,
        gpio_num_t tx_pin,
        gpio_num_t rx_pin);
    void start();
    void stop();

    void processData(const char* data);

        //  Контроль ответов
    bool awaitingResponse = false; // Флаг ожидания ответа
    int64_t commandSentTime = 0;   // Метка времени отправки команды
    const int64_t responseTimeout = 3000 * 1000; // Тайм-аут в микросекундах (3 секунды)
    
    //  функции поддерживаемы ssvc
    void sendCommandAT();
    void sendCommandGetSettings();
    void sendCommandStop();
    void sendCommandPause();
    void sendCommandGetVersion();

private:
    uart_port_t uart_num;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    volatile bool running;

    // Размеры буферов

    static const size_t ring_buffer_size = BUF_SIZE *5; // Размер кольцевого буфера
    char ringBuffer[ring_buffer_size];
    size_t head;
    size_t tail;
    void addToBuffer(const char* data);
    char* getDataFromBuffer();

    static void taskSendCommandAT(void *pvParameters);

    static void taskReceiveData(void *pvParameters);
    static void taskProcessData(void *pvParameters);

    void sendCommand(const char *command);
};

#endif // CONNECTOR_H
