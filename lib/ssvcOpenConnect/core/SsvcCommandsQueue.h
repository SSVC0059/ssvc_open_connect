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
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <map>
#include <utility>
#include <vector>

#define ATTEMPT_COUNT 3
// Таймаут ожидания ответа задан в тиках: значение по умолчанию уже
// сконвертировано, поэтому повторный pdMS_TO_TICKS над ним в
// pushCommandInQueue давал неверную длительность при tick != 1 мс.
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
  /// Номер сессии GET_LOG, под которую сделан резерв (0 — команда вне сессии).
  /// По нему воркер отбрасывает отменённые и устаревшие лог-команды.
  unsigned long generation = 0;
  /// Поколение safety: снимок _safetyEpoch на момент постановки. Неотправленная
  /// неидемпотентная команда с устаревшим поколением отбрасывается после
  /// аварийной остановки.
  unsigned long safetyEpoch = 0;
  /// Запись журнала попыток (0 — команда не журналируется: дубль GET_LOG и т.п.).
  uint32_t journalId = 0;
};

/**
 * @brief Состояние команды в журнале попыток.
 *
 * Имена состояний — часть REST-контракта (GET /rest/commands/{id}), менять их
 * нельзя без смены контракта.
 */
enum class SsvcCommandState : uint8_t
{
  Queued,   ///< принята и лежит в очереди
  Running,  ///< воркер забрал команду
  Sent,     ///< байты ушли в UART; ответа может не быть вовсе
  Done,     ///< пришёл бит успеха
  Rejected, ///< отказ: на постановке, от устройства или UART
  Timeout   ///< попытки исчерпаны без ответа
};

/**
 * @brief Копия записи журнала для REST-слоя.
 *
 * Строки command/reason — статические (литералы), поэтому снимок можно
 * сериализовать уже без мьютекса журнала.
 */
struct SsvcCommandStatus
{
  uint32_t id{0};
  const char* command{nullptr};
  SsvcCommandState state{SsvcCommandState::Queued};
  uint32_t queuedAt{0};
  uint32_t startedAt{0};
  uint32_t finishedAt{0};
  /// -1 — исход неизвестен, 0 — неуспех, 1 — пришёл бит успеха.
  int8_t ok{-1};
  const char* reason{nullptr};
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

  /// Явная инициализация: очередь → задача-воркер → Ready. Вызывается из
  /// SsvcOpenConnect::begin() после готовности зависимостей, а не из конструктора.
  bool begin();

  bool isReady() const { return _ready; }
  bool hasFailed() const { return _failed; }

  /// Аварийная остановка: приоритетный путь вне общего FIFO.
  /// Ставится в отдельную safety-очередь, которую воркер забирает первой, и не
  /// ждёт накопленных обычных команд. @return true, если STOP принят к отправке.
  /// commandIdOut — id записи журнала попыток (тот же, что вернёт REST).
  bool emergencyStop(uint32_t* commandIdOut = nullptr);

  /// @return true, если команда поставлена в очередь.
  /// commandIdOut — id записи журнала попыток, назначенный этой постановке.
  bool getSettings(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT,
                   uint32_t* commandIdOut = nullptr) const;

  bool next(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT,
            uint32_t* commandIdOut = nullptr) const;

  bool stop(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT,
            uint32_t* commandIdOut = nullptr) const;

  bool start(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT,
             uint32_t* commandIdOut = nullptr) const;

  bool pause(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT,
             uint32_t* commandIdOut = nullptr) const;

  bool resume(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT,
              uint32_t* commandIdOut = nullptr) const;

  bool version(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT,
               uint32_t* commandIdOut = nullptr) const;

  bool at(int attempt_count = ATTEMPT_COUNT, TickType_t timeout = TIMEOUT,
          uint32_t* commandIdOut = nullptr) const;

  /// @return true, если команда поставлена в очередь; false — отброшена по версии API.
  bool set(const std::string& parameters, int attempt_count = ATTEMPT_COUNT,
           TickType_t timeout = TIMEOUT,
           std::vector<std::string>* skippedOut = nullptr,
           uint32_t* commandIdOut = nullptr) const;

  bool status(const std::string& parameters, int attempt_count = ATTEMPT_COUNT,
              TickType_t timeout = TIMEOUT, uint32_t* commandIdOut = nullptr) const;

  /// @return true, если команда поставлена в очередь; false — функция недоступна на устройстве.
  /// generation — номер сессии, зарезервированной через Transfer::reserve*;
  /// команда с устаревшим generation отбрасывается воркером до отправки.
  bool getLog(const std::string& parameters = "", unsigned long generation = 0,
              int attempt_count = 1, TickType_t timeout = TIMEOUT,
              uint32_t* commandIdOut = nullptr) const;

  /**
   * @brief Судьба команды по id из POST /rest/commands.
   *
   * @param commandId id, выданный этой постановке (0 — служебное «нет записи»).
   * @param out снимок записи; при @return false не изменяется.
   * @return true, если запись есть и не истекла по TTL.
   */
  bool getCommandStatus(uint32_t commandId, SsvcCommandStatus& out) const;

  /// Имя состояния для REST ("queued", "running", "sent", "done", "rejected", "timeout").
  static const char* commandStateName(SsvcCommandState state);

  UBaseType_t availableCommands() const
  {
    return command_queue ? uxQueueMessagesWaiting(command_queue) : 0;
  }

  /** Запланировать повторный запрос getSettings при ошибке UART (SSVC выключен) */
  void scheduleUartRetryTimer() const;

  /**
   * @brief Описание последней отбраковки команды по версии API.
   *
   * Пусто, если отбраковок не было. Заполняется в единой точке постановки в
   * очередь, чтобы вызывающий мог отличить «команда ушла» от «команда
   * отброшена, потому что устройство старее».
   */
  std::string lastRejectionReason() const
  {
    const std::lock_guard<std::mutex> lock(_rejectionMutex);
    return _lastRejectionReason;
  }

  /// Возможность, которой не хватило ("get_log", "extended_heads", "predec", ...).
  std::string lastRejectedFeature() const
  {
    const std::lock_guard<std::mutex> lock(_rejectionMutex);
    return _lastRejectedFeature;
  }

  /// Фактическая и требуемая версия API из последней отбраковки ("1.7" / "1.10").
  std::string lastRejectedDeviceApi() const
  {
    const std::lock_guard<std::mutex> lock(_rejectionMutex);
    return _lastRejectedDeviceApi;
  }

  std::string lastRejectedRequiredApi() const
  {
    const std::lock_guard<std::mutex> lock(_rejectionMutex);
    return _lastRejectedRequiredApi;
  }

  ~SsvcCommandsQueue()
  {
    if (command_queue)
    {
      vQueueDelete(command_queue);
    }
  }

  /**
   * @brief Обёртка команды в COMMAND_MAP.
   *
   * Вызов без id (MQTT, внутренние триггеры) не журналирует ответ вызывающему;
   * вызов с id отдаёт id записи журнала, выданный именно этой постановке.
   * Две перегрузки operator() сохраняют прежний способ вызова `it->second(params)`.
   */
  struct CommandEntry
  {
    std::function<bool(const std::string&, uint32_t*)> dispatch;

    /// Шаблонный конструктор, а не агрегат: std::map инициализируется парами
    /// {"имя", лямбда}. Обычный конструктор от std::function потребовал бы двух
    /// пользовательских преобразований (лямбда -> std::function -> CommandEntry),
    /// что std::pair не допускает; здесь преобразование одно.
    template <typename Handler>
    CommandEntry(Handler&& handler)
        : dispatch(std::forward<Handler>(handler))
    {
    }

    bool operator()(const std::string& parameters) const
    {
      return dispatch(parameters, nullptr);
    }

    bool operator()(const std::string& parameters, uint32_t* commandIdOut) const
    {
      return dispatch(parameters, commandIdOut);
    }
  };

  /// Команда возвращает результат постановки в очередь: REST отвечает ошибкой
  /// при отказе, а не «accepted» независимо от фактической отправки.
  static const std::map<std::string, CommandEntry> COMMAND_MAP;

private:
  QueueHandle_t command_queue;
  /// Отдельная очередь аварийных команд: воркер забирает её прежде общего FIFO,
  /// поэтому STOP не зависит от накопленных обычных команд.
  QueueHandle_t safety_queue{};
  bool _ready{false};
  bool _failed{false};
  /// Поколение safety: инкремент при emergencyStop инвалидирует неотправленные
  /// START/NEXT/RESUME, устаревшие после аварийной остановки.
  unsigned long _safetyEpoch{0};
  /// Таймеры создаются и сбрасываются из разных задач (воркер, RX, callback
  /// таймера), поэтому check-then-set защищён общим мьютексом.
  mutable std::mutex _timerMutex;
  TimerHandle_t _settingsTimer = nullptr;
  mutable TimerHandle_t _uartRetryTimer = nullptr;
  static constexpr UBaseType_t COMMAND_QUEUE_LENGTH = 50;
  /**
   * @brief Размер элемента очереди — указателя на команду, а не объекта.
   *
   * FreeRTOS копирует элемент побайтово и не вызывает конструктор копирования
   * std::string, поэтому очередь владеет указателями, а объект живёт в куче
   * до delete в commandProcessorTask. Размер обязан совпадать с адресом,
   * который передаётся в xQueueSend/xQueueReceive.
   */
  static constexpr UBaseType_t COMMAND_QUEUE_ITEM_SIZE = sizeof(SsvcCommand *);

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
      {SsvcCommandType::STATUS, BIT9},
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
                          int attempt_count, TickType_t timeout,
                          unsigned long generation = 0,
                          std::vector<std::string>* skippedOut = nullptr,
                          uint32_t* commandIdOut = nullptr) const;

  /// Диагностика последней отбраковки; читается REST-слоем при отказе.
  void rememberRejection(const std::string& reason, const std::string& feature,
                         const std::string& requiredApi, const std::string& deviceApi) const;

  // ==================== Журнал попыток (commandId) ==================== //
  //
  // Кольцо фиксированного размера, статический массив: ни одной аллокации на
  // команду. Запись живёт до перезаписи слотом кольца или до истечения TTL —
  // GET /rest/commands/{id} после этого отвечает 404, а не чужой командой.

  static constexpr uint8_t JOURNAL_SIZE = 16;
  /// Сколько запись считается актуальной (10 минут).
  static constexpr uint32_t JOURNAL_TTL_MS = 10UL * 60UL * 1000UL;
  static constexpr uint8_t JOURNAL_PARAMS_MAX = 64;

  struct CommandRecord
  {
    uint32_t id{0};
    uint32_t queuedAt{0};
    uint32_t startedAt{0};
    uint32_t finishedAt{0};
    SsvcCommandState state{SsvcCommandState::Queued};
    int8_t ok{-1};
    /// Имя команды и причина — статические строки (литералы), не std::string.
    const char* command{nullptr};
    const char* reason{nullptr};
    /// Усечённые параметры (SET/STATUS/GET_LOG) — для диагностики; в REST не отдаются.
    char parameters[JOURNAL_PARAMS_MAX]{};
  };

  /// Отдельный мьютекс: журнал не смешивается с _rejectionMutex и не держится
  /// во время UART-операций. mutable — пишется из const-методов постановки.
  mutable std::mutex _journalMutex;
  /// Само кольцо. Размер ~1.5 КБ .bss, куча не используется.
  mutable CommandRecord _journal[JOURNAL_SIZE]{};
  mutable uint8_t _journalNext{0};
  /// Монотонный счётчик id: 0 зарезервирован под «записи нет».
  mutable uint32_t _journalLastId{0};

  /// Вызывается только под _journalMutex.
  uint32_t nextJournalIdLocked() const;
  /// Занимает слот кольца под новую запись (под _journalMutex).
  CommandRecord& beginRecordLocked(const char* command, const std::string& parameters,
                                   uint32_t now) const;
  /// Поиск по id (под _journalMutex); nullptr — запись перезаписана.
  CommandRecord* findRecordLocked(uint32_t id) const;

  /// Отказ на постановке: запись сразу терминальная (id клиенту не выдаётся —
  /// тела 4xx/5xx не менялись, запись остаётся для диагностики).
  void journalRejected(const char* command, const std::string& parameters,
                       const char* reason) const;
  /// Воркер забрал команду.
  void journalMarkRunning(uint32_t id) const;
  /// Байты ушли в UART (не терминальное состояние).
  void journalMarkSent(uint32_t id) const;
  /// Терминальное состояние: done/rejected/timeout/sent-как-итог.
  void journalFinish(uint32_t id, SsvcCommandState state, int8_t ok,
                     const char* reason) const;

  mutable std::mutex _rejectionMutex;
  mutable std::string _lastRejectionReason;
  mutable std::string _lastRejectedFeature;
  mutable std::string _lastRejectedDeviceApi;
  mutable std::string _lastRejectedRequiredApi;
};

#endif // SSVCOPENCONNECT_SSVCCOMMANDSQUEUE_H