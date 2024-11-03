#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <queue>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Определение уровней логирования
enum LogLevel {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
};

class Logger {
public:
    Logger();
    ~Logger();

    static void setLogLevel(LogLevel level) {
        logLevel = level;
    }

    static void debug(const String& message);
    static void info(const String& message);
    static void warning(const String& message);
    static void error(const String& message);

private:
    static LogLevel logLevel; // Уровень логирования
    static QueueHandle_t logQueue; // Очередь логов

    static void logTask(void* parameter); // Задача для обработки логов
};

#endif // LOGGER_H
