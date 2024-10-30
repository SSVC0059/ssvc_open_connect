#include <Arduino.h>
#include <ArduinoJson.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "UartTasks.h"
#include "Logger.h"
#include <ESPAsyncWebServer.h>

// Конфигурация UART
#define UART0 UART_NUM_0
#define UART1 UART_NUM_1
#define UART0_RX_PIN 3
#define UART0_TX_PIN 1
#define UART1_RX_PIN 16
#define UART1_TX_PIN 17

// Размеры буферов
#define BUF_SIZE 1024

// Размер очереди и длина сообщения
#define QUEUE_LENGTH 5
#define MESSAGE_SIZE BUF_SIZE

// Очередь для передачи сообщений
extern QueueHandle_t uartQueue;
extern QueueHandle_t uart0Queue;
extern AsyncEventSource events;

// Функция для обработки типов входящих данных из UART1
void handleUART1MessageTask(void *pvParameters) {
    uint8_t data[MESSAGE_SIZE];

    while (true) {
        // Ожидание получения сообщения из очереди
        if (xQueueReceive(uartQueue, &data, portMAX_DELAY) == pdPASS) {
            // Создаем статический JSON-документ для десериализации
            JsonDocument doc;

            // Попытка десериализации данных JSON
            DeserializationError error = deserializeJson(doc, data);
            if (error) {
                logMessage(LOG_ERROR, "JSON parsing failed: ");
                logMessage(LOG_ERROR, error.c_str());
                continue;
            }

            // Проверка наличия и значения поля "type"
            const char* type = doc["type"];
            // if (type && strcmp(type, "response") != 0) {
                // Печать содержимого JSON
                logMessage(LOG_DEBUG, "Received message:");
                char message[256];
                snprintf(message, sizeof(message), "Type: %s", type);
                logMessage(LOG_DEBUG, message);
                String json;
                serializeJson(doc, json);
                events.send(json.c_str(), "telemetry", millis());
            // }
        }
    }
}

// Функция для настройки UART
void ssvc_connect_begin() {
    // Настройка UART0
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART0, &uart_config);
    uart_set_pin(UART0, UART0_TX_PIN, UART0_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART0, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);

    // Настройка UART1
    uart_param_config(UART1, &uart_config);
    uart_set_pin(UART1, UART1_TX_PIN, UART1_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART1, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
}

// Функция задачи для передачи данных из UART1 в UART0 и очереди
void uart1ToUart0Task(void *pvParameters) {
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    while (true) {
        // Чтение данных из UART1
        int len = uart_read_bytes(UART1, data, BUF_SIZE, 20 / portTICK_RATE_MS);
        if (len > 0) {
            // Отправка данных в очередь
            if (xQueueSend(uartQueue, data, portMAX_DELAY) != pdPASS) {
                Serial.println("Failed to send to queue");
            }

            // Запись данных в UART0
            #ifdef DEBUG_MESSAGE
                uart_write_bytes(UART0, (const char*) data, len);
            #endif
        }
    }
    free(data);
}

// Функция задачи для передачи данных из UART0 в UART1
// void uart0ToUart1Task(void *pvParameters) {
//     uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
//     while (true) {
//         // Чтение данных из UART0
//         int len = uart_read_bytes(UART0, data, BUF_SIZE, 20 / portTICK_RATE_MS);
//         if (len > 0) {
//             // Запись данных в UART1
//             uart_write_bytes(UART1, (const char*) data, len);
//         }
//     }
//     free(data);
// }

void uart0ToUart1Task(void *pvParameters) {
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    while (true) {
        // Ожидание получения сообщения из очереди
        if (xQueueReceive(uart0Queue, &data, portMAX_DELAY) == pdPASS) {
            // Отправка данных в UART1
            uart_write_bytes(UART1, (const char*) data, strlen((const char*)data)); // Отправка данных из очереди
        }
    }
}

void sendCommandToSsvc(const char* message) {
    uint8_t* data = (uint8_t*) malloc(strlen(message) + 1); // +1 для завершающего нуля
    strcpy((char*)data, message); // Копируем сообщение

    // Отправка сообщения в очередь
    if (xQueueSend(uart0Queue, &data, portMAX_DELAY) != pdPASS) {
        free(data); // Освобождение памяти, если отправка не удалась
        logMessage(LOG_ERROR, "Failed to send message to UART1 queue");
    }
}