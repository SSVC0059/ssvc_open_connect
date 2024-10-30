#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "freertos/queue.h"

// Определение уровней логирования
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
};

// Функции для инициализации логгера и публикации логов
void initLogger();
void logMessage(LogLevel level, const char* message);
void logTask(void *pvParameters); // Задача для вывода логов

#endif // LOGGER_H
