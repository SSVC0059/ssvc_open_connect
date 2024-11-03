#include "Logger.h"

LogLevel Logger::logLevel = LOG_LEVEL_INFO; // Изначальный уровень логирования
QueueHandle_t Logger::logQueue = xQueueCreate(10, sizeof(String)); // Создаем очередь на 10 элементов

Logger::Logger() {
    // Создаем задачу для обработки логов
    xTaskCreatePinnedToCore(logTask, "LogTask", 2048, NULL, 0, NULL, 0);
}

Logger::~Logger() {
    vQueueDelete(logQueue); // Освобождаем ресурсы очереди
}

void Logger::debug(const String& message) {
    if (logLevel <= LOG_LEVEL_DEBUG) {
        String logMessage = String("[DEBUG] ") + message;
        xQueueSend(logQueue, &logMessage, portMAX_DELAY); // Добавляем сообщение в очередь
    }
}

void Logger::info(const String& message) {
    if (logLevel <= LOG_LEVEL_INFO) {
        String logMessage = String("[INFO] ") + message;
        xQueueSend(logQueue, &logMessage, portMAX_DELAY); // Добавляем сообщение в очередь
    }
}

void Logger::warning(const String& message) {
    if (logLevel <= LOG_LEVEL_WARNING) {
        String logMessage = String("[WARNING] ") + message;
        xQueueSend(logQueue, &logMessage, portMAX_DELAY); // Добавляем сообщение в очередь
    }
}

void Logger::error(const String& message) {
    if (logLevel <= LOG_LEVEL_ERROR) {
        String logMessage = String("[ERROR] ") + message;
        xQueueSend(logQueue, &logMessage, portMAX_DELAY); // Добавляем сообщение в очередь
    }
}

void Logger::logTask(void* parameter) {
    String logMessage;
    while (true) {
        // Извлекаем сообщение из очереди
        if (xQueueReceive(logQueue, &logMessage, portMAX_DELAY)) {
            Serial.println(logMessage); // Выводим сообщение в Serial
        }
    }
}
