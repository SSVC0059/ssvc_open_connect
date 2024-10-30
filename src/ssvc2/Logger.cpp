#include "Logger.h"

// Цвета для вывода логов через Serial
#define COLOR_RESET "\033[0m"
#define COLOR_DEBUG "\033[32m"    // Зеленый
#define COLOR_INFO "\033[36m"     // Голубой
#define COLOR_WARNING "\033[33m"  // Желтый
#define COLOR_ERROR "\033[31m"    // Красный
#define COLOR_CRITICAL "\033[41m"  // Красный фон

#define LOG_QUEUE_LENGTH 10
#define LOG_MESSAGE_SIZE 256

// Очередь для логов
QueueHandle_t logQueue;
static LogLevel currentLogLevel = LOG_DEBUG;  // Уровень логирования по умолчанию

// Инициализация логгера
void initLogger() {
    logQueue = xQueueCreate(LOG_QUEUE_LENGTH, LOG_MESSAGE_SIZE);
    if (logQueue == NULL) {
        Serial.println("Failed to create log queue");
        return;
    }

    // Создание задачи для вывода логов
    xTaskCreate(logTask, "Log Task", 2048, NULL, 1, NULL);
}

// Установка уровня логирования
void setLogLevel(LogLevel level) {
    currentLogLevel = level;
}

// Функция для обработки и вывода логов
void logTask(void *pvParameters) {
    char message[LOG_MESSAGE_SIZE];

    while (true) {
        // Ожидание получения сообщения из очереди
        if (xQueueReceive(logQueue, &message, portMAX_DELAY) == pdPASS) {
            Serial.print(message); // Вывод сообщения в Serial
        }
    }
}

// Функция для логирования сообщения
void logMessage(LogLevel level, const char* message) {
    // Проверка уровня логирования
    if (level < currentLogLevel) {
        return;  // Игнорировать, если уровень ниже установленного
    }

    const char* color;

    // Определение цвета в зависимости от уровня логирования
    switch (level) {
        case LOG_DEBUG:
            color = COLOR_DEBUG;
            break;
        case LOG_INFO:
            color = COLOR_INFO;
            break;
        case LOG_WARNING:
            color = COLOR_WARNING;
            break;
        case LOG_ERROR:
            color = COLOR_ERROR;
            break;
        case LOG_CRITICAL:
            color = COLOR_CRITICAL;
            break;
        default:
            color = COLOR_RESET;
            break;
    }

    // Формирование сообщения с уровнем логирования и цветом
    char formattedMessage[LOG_MESSAGE_SIZE];
    snprintf(formattedMessage, LOG_MESSAGE_SIZE, "%s%s%s\n", color, message, COLOR_RESET);

    // Отправка сообщения в очередь
    if (xQueueSend(logQueue, formattedMessage, portMAX_DELAY) != pdPASS) {
        Serial.println("Failed to send log message to queue");
    }
}
