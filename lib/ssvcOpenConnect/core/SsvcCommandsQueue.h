#ifndef SSVCOPENCONNECT_SSVCCOMMANDSQUEUE_H
#define SSVCOPENCONNECT_SSVCCOMMANDSQUEUE_H

/**
 *   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive SvelteKit web interface for
 *   monitoring and controlling the distillation process.
 *   https://github.com/SSVC0059/ssvc_open_connect
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 *
 *   This software is independent and not affiliated with SSVC0059 company.
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *
 *   Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/

#include "ArduinoJson.h"
#include "SsvcConnector.h"
#include "core/SsvcSettings/SsvcSettings.h"
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
  int attempt_count = 0;
  TickType_t timeout = TIMEOUT;
};

class MutexLock
{
public:

  explicit MutexLock(SemaphoreHandle_t _mutex) : m_mutex(_mutex)
  {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
  }

  ~MutexLock() { xSemaphoreGive(m_mutex); }

private:
  SemaphoreHandle_t m_mutex;
};

class SsvcCommandsQueue
{
public:
  static SsvcCommandsQueue& getQueue()
  {
    static SsvcCommandsQueue queue;
    return queue;
  }

  void getSettings(int attempt_count = ATTEMPT_COUNT,
                   TickType_t timeout = TIMEOUT) const;

  void next(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT) const;

  void stop(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT) const;

  void start(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT) const;

  void pause(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT) const;

  void resume(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT) const;

  void version(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT) const;

  void at(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT) const;

  void set(const std::string& parameters, int attempt_count = ATTEMPT_COUNT,
           TickType_t timeout = TIMEOUT) const;

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
  static constexpr UBaseType_t COMMAND_QUEUE_LENGTH = 50;
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

  SsvcCommandsQueue();

  void pushCommandInQueue(SsvcCommandType type, const std::string& parameters,
                          int attempt_count, TickType_t timeout) const;
};

#endif // SSVCOPENCONNECT_SSVCCOMMANDSQUEUE_H
