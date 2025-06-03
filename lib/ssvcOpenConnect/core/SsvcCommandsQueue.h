#ifndef SSVCOPENCONNECT_SSVCCOMMANDSQUEUE_H
#define SSVCOPENCONNECT_SSVCCOMMANDSQUEUE_H

#include "ArduinoJson.h"
#include "SsvcConnector.h"
#include <core/SsvcSettings/SsvcSettings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <Arduino.h>
#include <string>
#include <unordered_map>

#define ATTEMPT_COUNT 3
#define TIMEOUT pdMS_TO_TICKS(3000)
#define GET_SETTINGS_REQUEST_TIMER 30000

enum class SsvcCommandType
{
  GET_SETTINGS,
  VERSION,
  STOP,
  START,
  PAUSE,
  RESUME,
  NEXT,
  AT,
  SET
};

struct SsvcCommand
{
  SsvcCommandType type;
  std::string parameters;
  int8_t attempt_count = 0;
  TickType_t timeout = TIMEOUT;
};

/**
 * @class MutexLock
 * @brief RAII-обертка для работы с мьютексами FreeRTOS
 */
class MutexLock
{
public:
  /**
   * @brief Захватывает мьютекс при создании
   * @param mutex Указатель на мьютекс FreeRTOS
   */
  explicit MutexLock(SemaphoreHandle_t _mutex) : m_mutex(_mutex)
  {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
  }

  /**
   * @brief Освобождает мьютекс при уничтожении
   */
  ~MutexLock() { xSemaphoreGive(m_mutex); }

private:
  SemaphoreHandle_t m_mutex; ///< Указатель на мьютекс
};

class SsvcCommandsQueue
{
public:
  // Метод для получения экземпляра синглтона
  static SsvcCommandsQueue& getQueue()
  {
    static SsvcCommandsQueue queue;
    return queue;
  }

  void getSettings(int attempt_count = ATTEMPT_COUNT,
                   TickType_t timeout = TIMEOUT);

  void next(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT);

  void stop(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT);

  void start(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT);

  void pause(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT);

  void resume(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT);

  void version(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT);

  void at(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT);

  void set(std::string parameters, int attempt_count = ATTEMPT_COUNT,
           TickType_t timeout = TIMEOUT);

  // Проверка очереди
  UBaseType_t availableCommands() const
  {
    return command_queue ? uxQueueMessagesWaiting(command_queue) : 0;
  }

  bool _cmdSetResult{false};

  ~SsvcCommandsQueue()
  {
    if (command_queue)
    {
      vQueueDelete(command_queue);
    }
  }

private:
  QueueHandle_t command_queue;
  TimerHandle_t _settingsTimer = nullptr;
  static constexpr UBaseType_t COMMAND_QUEUE_LENGTH = 10;
  static constexpr UBaseType_t COMMAND_QUEUE_ITEM_SIZE = sizeof(SsvcCommand);

  /**
   * @brief Соответствие типов команд и битов событий
   */
  std::unordered_map<SsvcCommandType, EventBits_t> commandToExpectedBit = {
    {SsvcCommandType::GET_SETTINGS, BIT10}, // BIT10 для GET_SETTINGS
    {SsvcCommandType::VERSION, BIT11}, {SsvcCommandType::STOP, BIT9},
    {SsvcCommandType::PAUSE, BIT9}, {SsvcCommandType::RESUME, BIT9},
    {SsvcCommandType::NEXT, BIT9}, {SsvcCommandType::AT, BIT9},
    {SsvcCommandType::SET, BIT9},
  };

  /// Тип callback-функции для обработки ответов
  using ResponseCallback = std::function<bool(SsvcCommand* cmd)>;

  std::unordered_map<SsvcCommandType, ResponseCallback>
  responseCallbacks; ///< Обработчики команд

  std::unordered_map<EventBits_t, ResponseCallback>
  bitCallbacks; ///< Коллекция обработчиков

  /**
   * @brief Регистрация обработчика для бита события
   * @param bit Бит события
   * @param callback Функция-обработчик
   */
  void registerBitHandler(EventBits_t bit, ResponseCallback callback)
  {
    MutexLock lock(mutex);
    bitCallbacks[bit] = std::move(callback);
  }

  /**
   * @brief Задача обработки команд из очереди
   * @param pvParameters Параметры задачи (указатель на экземпляр класса)
   */
  [[noreturn]] static void commandProcessorTask(void* pvParameters);

  /**
   * @brief Задача периодической отправки AT-команд
   * @param pvParameters Параметры задачи (указатель на экземпляр класса)
   */
  [[noreturn]] static void sendAT(void* pvParameters);

  /**
   * @brief Регистрация callback-обработчиков команд
   */
  void registerCallbackCommands();

  // Приватный конструктор
  SsvcCommandsQueue();

  void pushCommandInQueue(SsvcCommandType type, std::string parameters,
                          int attempt_count, TickType_t timeout);
};

#endif // SSVCOPENCONNECT_SSVCCOMMANDSQUEUE_H
