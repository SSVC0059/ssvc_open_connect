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
#include <map>
#include <vector>

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
  SET,
  STATUS,
  GET_LOG
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

  /// @return true, если команда поставлена в очередь; false — отброшена по версии API.
  bool set(const std::string& parameters, int attempt_count = ATTEMPT_COUNT,
           TickType_t timeout = TIMEOUT) const;

  void status(const std::string& parameters, int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT) const;

  /// @return true, если команда поставлена в очередь; false — функция недоступна на устройстве.
  bool getLog(const std::string& parameters = "", int attempt_count = 1,
              TickType_t timeout = TIMEOUT) const;

  UBaseType_t availableCommands() const
  {
    return command_queue ? uxQueueMessagesWaiting(command_queue) : 0;
  }

  /** Запланировать повторный запрос getSettings при ошибке UART (SSVC выключен) */
  void scheduleUartRetryTimer() const;

  bool _cmdSetResult{false};

  /**
   * @brief Описание последней отбраковки команды по версии API.
   *
   * Пусто, если отбраковок не было. Заполняется в единой точке постановки в
   * очередь, чтобы вызывающий мог отличить «команда ушла» от «команда
   * отброшена, потому что устройство старее».
   */
  std::string lastRejectionReason() const { return _lastRejectionReason; }

  /// Возможность, которой не хватило ("get_log", "extended_heads", ...).
  std::string lastRejectedFeature() const { return _lastRejectedFeature; }

  /// Фактическая и требуемая версия API из последней отбраковки ("1.7" / "1.10").
  std::string lastRejectedDeviceApi() const { return _lastRejectedDeviceApi; }

  std::string lastRejectedRequiredApi() const { return _lastRejectedRequiredApi; }

  /**
   * @brief Параметры SET, отброшенные по версии API с момента последнего сброса.
   *
   * Заполняется в единой точке постановки в очередь, поэтому вызывающий
   * (REST-слой, Builder) узнаёт, какие поля не ушли на устройство.
   */
  const std::vector<std::string>& skippedSetParams() const { return _skippedSetParams; }

  /// Начать новый сбор пропущенных параметров (перед пакетом команд SET).
  void clearSkippedSetParams() const { _skippedSetParams.clear(); }

  ~SsvcCommandsQueue()
  {
    if (command_queue)
    {
      vQueueDelete(command_queue);
    }
  }

  static const std::map<std::string, std::function<void(const std::string&)>> COMMAND_MAP;

private:
  QueueHandle_t command_queue;
  TimerHandle_t _settingsTimer = nullptr;
  mutable TimerHandle_t _uartRetryTimer = nullptr;
  static constexpr UBaseType_t COMMAND_QUEUE_LENGTH = 50;
  static constexpr UBaseType_t COMMAND_QUEUE_ITEM_SIZE = sizeof(SsvcCommand);

  /**
   * @brief Соответствие типов команд и битов событий
   */
  std::unordered_map<SsvcCommandType, EventBits_t> commandToExpectedBit = {
      {SsvcCommandType::GET_SETTINGS, BIT10}, // BIT10 для GET_SETTINGS
      {SsvcCommandType::VERSION, BIT11},
      {SsvcCommandType::STOP, BIT9},
      {SsvcCommandType::PAUSE, BIT9},
      {SsvcCommandType::RESUME, BIT9},
      {SsvcCommandType::NEXT, BIT9},
      {SsvcCommandType::AT, BIT9},
      {SsvcCommandType::SET, BIT9},
      {SsvcCommandType::START, BIT9}
  };

  /// Тип callback-функции для обработки ответов
  using ResponseCallback = std::function<bool(SsvcCommand* cmd)>;

  std::unordered_map<SsvcCommandType, ResponseCallback>
  responseCallbacks; ///< Обработчики команд

  std::unordered_map<EventBits_t, ResponseCallback>
  bitCallbacks; ///< Коллекция обработчиков

  static void uartRetryTimerCallback(TimerHandle_t xTimer);

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

  /**
   * @brief Единая точка постановки команды в очередь — и единственное место gate.
   *
   * Проверка версии API стоит здесь, а не в set()/getLog(), чтобы новая
   * версионозависимая команда не могла её забыть: для SET отбрасываются поля,
   * отсутствующие на устройстве, для GET_LOG проверяется возможность.
   *
   * @return true, если команда поставлена в очередь.
   */
  bool pushCommandInQueue(SsvcCommandType type, const std::string& parameters,
                          int attempt_count, TickType_t timeout) const;

  /// Диагностика последней отбраковки; читается REST-слоем при отказе.
  void rememberRejection(const std::string& reason, const std::string& feature,
                         const std::string& requiredApi, const std::string& deviceApi) const;

  mutable std::string _lastRejectionReason;
  mutable std::string _lastRejectedFeature;
  mutable std::string _lastRejectedDeviceApi;
  mutable std::string _lastRejectedRequiredApi;

  /// Накопитель отброшенных параметров SET; сбрасывается clearSkippedSetParams().
  mutable std::vector<std::string> _skippedSetParams;
};

#endif // SSVCOPENCONNECT_SSVCCOMMANDSQUEUE_H