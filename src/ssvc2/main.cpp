#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "UartTasks.h"
#include "Logger.h"
#include <LittleFS.h>
#include "OtaHandler.h"
#include <ESPAsyncWebServer.h>
#include "routes.h"

// // Настройки Wi-Fi
// const char* ssid = "WiFi-DOM.ru-1965";
// const char* password = "88888888";

const char* ssid = "Onix";
const char* password = "88888888";

// Настройки web сервера
AsyncWebServer server(80);
AsyncEventSource events("/events");  // Объявляем SSE-обработчик

OtaHandler otaHandler; // Создаем объект класса OtaHandler

// Конфигурация UART
#define UART0 UART_NUM_0
#define UART1 UART_NUM_1
#define UART0_RX_PIN 3
#define UART0_TX_PIN 1
#define UART1_RX_PIN 16
#define UART1_TX_PIN 17

// Настройки отладки. Снять коментарий для отладки сообщений в SERIAL1
// #define DEBUG_MESSAGE

// Размеры буферов
#define BUF_SIZE 1024
#define DMA_BUF_SIZE 1024

// Задачи RTOS
TaskHandle_t uart1ToUart0TaskHandle = NULL;
TaskHandle_t uart0ToUart1TaskHandle = NULL;
TaskHandle_t handleUART1MessageTaskHandle = NULL;

// Размер очереди и длина сообщения
#define QUEUE_LENGTH 5
#define MESSAGE_SIZE BUF_SIZE

// Очередь для передачи сообщений
QueueHandle_t uartQueue;
QueueHandle_t uart0Queue;  // Очередь для передачи сообщений в ssvc

void setup() {
    // Инициализация Serial для вывода сообщений
    Serial.begin(115200);

    initLogger();
    // logMessage(LOG_INFO, "Starting setup...");
    // logMessage(LOG_DEBUG, "Debug message"); // Это сообщение не будет выведено, так как уровень установлен на INFO
    // logMessage(LOG_INFO, "Info message");   // Это сообщение будет выведено
    // logMessage(LOG_ERROR, "Error message");  // Это сообщение тоже будет выведено

     // Подключение к Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        logMessage(LOG_INFO, "Подключение к WiFi...");
    }

    logMessage(LOG_INFO, "Подключено к WiFi!");
    // logMessage(LOG_INFO, "IP: " + WiFi.localIP());
    
    // Инициализация LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Ошибка инициализации LittleFS!");
        return;
    }

    // Отправка HTML и JS файлов
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // Создание очереди
    uartQueue = xQueueCreate(QUEUE_LENGTH, MESSAGE_SIZE);
    if (uartQueue == NULL) {
        logMessage(LOG_ERROR, "Failed to create queue");
        return;
    }
    uart0Queue = xQueueCreate(10, sizeof(uint8_t*));
        if (uart0Queue == NULL) {
        logMessage(LOG_ERROR, "Failed to create queue");
        return;
    }

        // Инициализация маршрутов
    initRoutes(server); 

     // Маршрут для SSE-событий
    server.addHandler(&events);
    server.begin();

    OtaHandler otaHandler; // Создаем объект класса OtaHandler

    ssvc_connect_begin();

    // Создание задач для пересылки данных
    xTaskCreate(uart1ToUart0Task, "UART1 to UART0 Task", 4096, NULL, 1, &uart1ToUart0TaskHandle);
    xTaskCreate(uart0ToUart1Task, "UART0 to UART1 Task", 4096, NULL, 1, &uart0ToUart1TaskHandle);
    xTaskCreate(handleUART1MessageTask, "Handle UART1 Message Task", 4096, NULL, 1, &handleUART1MessageTaskHandle);
}

void loop() {

}
