#include <Arduino.h>
#include "header.h"
#include "./Connector/Connector.h"
#include "Logger.h"


Connector* connector;

Logger logger; // Создаем объект Logger, задача для логирования будет запущена

void setup() {
    Serial.begin(115200);
    Logger::setLogLevel(LOG_LEVEL_DEBUG); // Устанавливаем уровень логирования


    connector = new Connector(
        UART_NUM_1,
        GPIO_NUM_17,
        GPIO_NUM_16
    ); 
    connector->start();


    // Logger::debug("Отладочное сообщение");
    // Logger::info("Информационное сообщение");
    // Logger::warning("Предупреждение");
    // Logger::error("Ошибка");
    connector->sendCommandGetSettings();
}

void loop() {
    // Основной цикл остается пустым, поскольку задачи работают асинхронно
}

