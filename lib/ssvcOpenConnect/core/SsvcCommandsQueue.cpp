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

#include "SsvcCommandsQueue.h"
#include "core/SsvcLogProtocol/SsvcLogProtocol.h"

#include "SsvcOpenConnect.h"

#include <cstdlib>
#include <cstring>
#include <limits>

#define TAG "SsvcCommandsQueue"

// Helper function to convert UTF-8 string to Windows-1251
static std::string utf8_to_win1251(const std::string& utf8) {
    std::string win1251;
    win1251.reserve(utf8.length());
    for (size_t i = 0; i < utf8.length(); ++i) {
        const unsigned char c1 = utf8[i];
        if (c1 < 0x80) {
            win1251 += static_cast<char>(c1);
        } else if (c1 == 0xD0) {
            if (i + 1 < utf8.length()) {
                const unsigned char c2 = utf8[++i];
                if (c2 == 0x81) { // Ё
                    win1251 += static_cast<char>(0xA8);
                } else if (c2 >= 0x90 && c2 <= 0xBF) { // А-п
                    win1251 += static_cast<char>(c2 - 0x90 + 0xC0);
                }
            }
        } else if (c1 == 0xD1) {
            if (i + 1 < utf8.length()) {
                const unsigned char c2 = utf8[++i];
                if (c2 >= 0x80 && c2 <= 0x8F) { // р-я
                    win1251 += static_cast<char>(c2 - 0x80 + 0xF0);
                } else if (c2 == 0x91) { // ё
                    win1251 += static_cast<char>(0xB8);
                }
            }
        }
    }
    return win1251;
}

namespace {

/// START/NEXT/RESUME неидемпотентны: повтор после неоднозначной отправки запрещён.
bool isIdempotentRetry(const SsvcCommandType type) {
  switch (type) {
  case SsvcCommandType::START:
  case SsvcCommandType::NEXT:
  case SsvcCommandType::RESUME:
    return false;
  default:
    return true;
  }
}

/// Имя request, которым контроллер помечает ответ на эту команду ("" — неизвестно).
const char *requestNameFor(const SsvcCommandType type) {
  switch (type) {
  case SsvcCommandType::STOP:
    return "STOP";
  case SsvcCommandType::START:
    return "START";
  case SsvcCommandType::PAUSE:
    return "PAUSE";
  case SsvcCommandType::RESUME:
    return "RESUME";
  case SsvcCommandType::NEXT:
    return "NEXT";
  case SsvcCommandType::AT:
    return "AT";
  case SsvcCommandType::SET:
    return "SET";
  case SsvcCommandType::STATUS:
    return "STATUS";
  default:
    return "";
  }
}

/// Глагол команды из поля request ответа ("" — если запрос пустой).
///
/// Контроллер повторяет в request всю команду вместе с параметрами
/// (документация: `"request": "SET heads=[1,2]"`), поэтому «STATUS OpenConnect»
/// или «STATUS v:  0.2.8.0» — это ответ на STATUS, а не чужая команда. Сверять
/// и различать нужно только первое слово.
std::string requestVerb(const std::string& request) {
  const std::size_t space = request.find_first_of(" \t");
  return space == std::string::npos ? request : request.substr(0, space);
}

/// Каноническое имя команды для журнала попыток и GET /rest/commands/{id}.
const char *commandNameFor(const SsvcCommandType type) {
  switch (type) {
  case SsvcCommandType::GET_SETTINGS:
    return "get_settings";
  case SsvcCommandType::VERSION:
    return "version";
  case SsvcCommandType::STOP:
    return "stop";
  case SsvcCommandType::START:
    return "start";
  case SsvcCommandType::PAUSE:
    return "pause";
  case SsvcCommandType::RESUME:
    return "resume";
  case SsvcCommandType::NEXT:
    return "next";
  case SsvcCommandType::AT:
    return "at";
  case SsvcCommandType::SET:
    return "set";
  case SsvcCommandType::STATUS:
    return "status";
  case SsvcCommandType::GET_LOG:
    return "get_log";
  }
  return "unknown";
}

/// Команды, на которые контроллер может не ответить вовсе (§ J.2). Их судьба
/// остаётся "sent" — честное «отправлено, ответа не ждём», а не "timeout".
bool mayStayUnanswered(const SsvcCommandType type) {
  switch (type) {
  case SsvcCommandType::START:
  case SsvcCommandType::NEXT:
  case SsvcCommandType::RESUME:
  case SsvcCommandType::STOP:
  case SsvcCommandType::GET_LOG:
    return true;
  default:
    return false;
  }
}

/// Копия параметров в запись журнала: без std::string, без аллокаций и без
/// выхода за фиксированный буфер (усечение).
void copyTruncated(char *dst, const std::size_t capacity, const std::string &src) {
  if (capacity == 0) {
    return;
  }
  const std::size_t length = src.size() < capacity - 1 ? src.size() : capacity - 1;
  std::memcpy(dst, src.data(), length);
  dst[length] = '\0';
}

} // namespace

// Инициализация статической карты команд
const std::map<std::string, SsvcCommandsQueue::CommandEntry> SsvcCommandsQueue::COMMAND_MAP = {
    {"at",           [](const std::string&, uint32_t* id){ return getQueue().at(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"next",         [](const std::string&, uint32_t* id){ return getQueue().next(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"pause",        [](const std::string&, uint32_t* id){ return getQueue().pause(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"stop",         [](const std::string&, uint32_t* id){ return getQueue().stop(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"start",        [](const std::string&, uint32_t* id){ return getQueue().start(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"resume",       [](const std::string&, uint32_t* id){ return getQueue().resume(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"version",      [](const std::string&, uint32_t* id){ return getQueue().version(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"get_settings", [](const std::string&, uint32_t* id){ return getQueue().getSettings(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"settings",     [](const std::string&, uint32_t* id){ return getQueue().getSettings(ATTEMPT_COUNT, TIMEOUT, id); }},
    {"emergency_stop", [](const std::string&, uint32_t* id){ return getQueue().emergencyStop(id); }},
    {"status",       [](const std::string& params, uint32_t* id){ return getQueue().status(params, ATTEMPT_COUNT, TIMEOUT, id); }},
    {"set",          [](const std::string& params, uint32_t* id){ return getQueue().set(params, ATTEMPT_COUNT, TIMEOUT, nullptr, id); }},
    {"get_log",      [](const std::string& params, uint32_t* id) {
      auto& transfer = SsvcLogProtocol::getTransfer();
      SsvcLogProtocol::ReserveResult reserved = SsvcLogProtocol::ReserveResult::Unavailable;

      if (params.empty()) {
        reserved = transfer.reserveList(millis());
      } else {
        char* end = nullptr;
        const long processNumber = std::strtol(params.c_str(), &end, 10);
        if (end == params.c_str() || *end != '\0' || processNumber <= 0 ||
            processNumber > std::numeric_limits<int>::max()) {
          return false;
        }
        reserved = transfer.reserveFile(static_cast<int>(processNumber), millis());
      }

      // Резерв обязателен и здесь: без него воркер вызывает beginList()/beginFile()
      // с generation 0 и затирает уже идущую передачу другого клиента.
      if (reserved == SsvcLogProtocol::ReserveResult::SameRequest) {
        // Тот же запрос уже выполняется: новой UART-команды нет, поэтому и
        // записи в журнале нет — REST вернёт commandId 0 (см. docs).
        return true;
      }
      if (reserved != SsvcLogProtocol::ReserveResult::Accepted) {
        return false;  // Busy/Unavailable — сессию не занимаем
      }
      if (!getQueue().getLog(params, transfer.generation(), 1, TIMEOUT, id)) {
        transfer.releaseReservation();
        return false;
      }
      return true;
    }}
};

SsvcCommandsQueue::SsvcCommandsQueue() {
  // Конструктор только создаёт очереди и регистрирует обработчики: задача-воркер
  // поднимается в begin() после готовности зависимостей.
  command_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, COMMAND_QUEUE_ITEM_SIZE);
  if (command_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create command queue!");
  } else {
    ESP_LOGI(TAG, "Command queue created successfully");
  }

  safety_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, COMMAND_QUEUE_ITEM_SIZE);
  if (safety_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create safety queue!");
  }

  registerCallbackCommands();
}

bool SsvcCommandsQueue::begin() {
  if (_ready) {
    return true; // Инициализация идемпотентна
  }
  if (command_queue == nullptr || safety_queue == nullptr) {
    // Без очередей воркеру нечего обслуживать: честный отказ вместо работы с
    // нулевым дескриптором.
    _failed = true;
    ESP_LOGE(TAG, "begin: очередь не создана, состояние Failed");
    return false;
  }

  TaskHandle_t processorTask = nullptr;
  const BaseType_t created = xTaskCreatePinnedToCore(
      commandProcessorTask,
      "CmdProcessor",
      4096,
      this,
      (tskIDLE_PRIORITY + 1),
      &processorTask,
      1
  );
  if (created != pdPASS || processorTask == nullptr) {
    _failed = true;
    ESP_LOGE(TAG, "begin: не удалось создать CmdProcessor, состояние Failed");
    return false;
  }

  _ready = true;
  ESP_LOGI(TAG, "begin: обработчик команд готов (Ready)");
  return true;
}

bool SsvcCommandsQueue::emergencyStop(uint32_t *commandIdOut) {
  // Готовность проверяется до всего остального: если воркер не поднят (begin() не
  // вызывался или упал), ставить STOP некуда, и «принято» без отправки — ложное
  // подтверждение защитного действия. Вызывающий получит 503, а не 200.
  if (!_ready || safety_queue == nullptr) {
    ESP_LOGE(TAG, "emergencyStop: очередь команд не готова (ready=%d), STOP не поставлен",
             _ready ? 1 : 0);
    rememberRejection("not_ready", "emergency_stop", "", "");
    return false;
  }

  // Аварийная команда идёт приоритетным путём: отдельная очередь (воркер берёт
  // её первой) и новое поколение safety, чтобы неотправленные START/NEXT/RESUME
  // не «догнали» остановку. Поколение применяется только после успешной
  // постановки: иначе при отказе STOP не ушёл бы, а догоняющие команды уже были
  // бы инвалидированы.
  const unsigned long safetyEpoch = _safetyEpoch + 1;

  auto *cmd = new SsvcCommand();
  cmd->type = SsvcCommandType::STOP;
  cmd->attempt_count = 1;
  cmd->timeout = TIMEOUT;
  cmd->safetyEpoch = safetyEpoch;

  // id и запись журнала выдаются до постановки: вызывающий получает id именно этой
  // команды, а не «последний принятый». Мьютекс журнала держим только на время
  // выдачи id: xQueueSend при полной очереди ждёт до 1 с, и держать блокировку всё
  // это время нельзя — воркер не смог бы отметить running/sent, а GET
  // /rest/commands/{id} не ответил бы. Для аварийного STOP это ещё важнее: его
  // задержка не должна зависеть от чужой блокировки.
  uint32_t commandId = 0;
  {
    const std::lock_guard<std::mutex> lock(_journalMutex);
    commandId = beginRecordLocked("emergency_stop", std::string(), millis()).id;
  }
  cmd->journalId = commandId;

  const bool queued = safety_queue != nullptr &&
                      xQueueSend(safety_queue, &cmd, pdMS_TO_TICKS(1000)) == pdPASS;
  if (!queued) {
    journalFinish(commandId, SsvcCommandState::Rejected, 0, "safety_rejected");
    ESP_LOGE(TAG, "emergencyStop: не удалось поставить STOP в safety-очередь");
    delete cmd;
    rememberRejection("safety_rejected", "emergency_stop", "", "");
    return false;
  }

  _safetyEpoch = safetyEpoch;
  if (commandIdOut != nullptr) {
    *commandIdOut = commandId;
  }
  ESP_LOGW(TAG, "emergencyStop: STOP поставлен вне общего FIFO (id=%u)",
           static_cast<unsigned>(commandId));
  return true;
}

/**
 * @brief Callback-функция для отложенного получения настроек.
 *
 * Используется таймером для вызова метода getSettings().
 *
 * @param xTimer Указатель на таймер, инициировавший вызов.
 */
void delayed_get_settings_callback(const TimerHandle_t xTimer) {
  const auto *self = static_cast<SsvcCommandsQueue *>(pvTimerGetTimerID(xTimer));
  if (self) {
    self->getSettings();
  }

  // Удаление таймера после выполнения
  xTimerDelete(xTimer, 0);
}

void SsvcCommandsQueue::uartRetryTimerCallback(TimerHandle_t xTimer) {
  auto *self = static_cast<SsvcCommandsQueue *>(pvTimerGetTimerID(xTimer));
  if (self) {
    ESP_LOGI(TAG, "UART retry: requesting settings and version");
    self->getSettings();
    self->version();
    xTimerDelete(xTimer, 0);
    const std::lock_guard<std::mutex> lock(self->_timerMutex);
    self->_uartRetryTimer = nullptr;
  }
}

void SsvcCommandsQueue::scheduleUartRetryTimer() const {
  const std::lock_guard<std::mutex> lock(_timerMutex);
  if (_uartRetryTimer != nullptr) {
    return;  // Таймер уже запланирован
  }
  _uartRetryTimer = xTimerCreate(
      "uart_retry_timer",
      pdMS_TO_TICKS(GET_SETTINGS_REQUEST_TIMER),
      pdFALSE,
      const_cast<SsvcCommandsQueue *>(this),
      SsvcCommandsQueue::uartRetryTimerCallback);
  if (_uartRetryTimer != nullptr &&
      xTimerStart(_uartRetryTimer, 0) == pdPASS) {
    ESP_LOGI(TAG, "Scheduled UART retry in %d seconds",
             GET_SETTINGS_REQUEST_TIMER / 1000);
  } else {
    // Молча проглоченный отказ оставлял связь без повторного запроса настроек.
    ESP_LOGW(TAG, "Не удалось запланировать UART retry");
    if (_uartRetryTimer != nullptr) {
      xTimerDelete(_uartRetryTimer, 0);
      _uartRetryTimer = nullptr;
    }
  }
}

/**
 * @brief Основная задача обработки команд из очереди и отправки их SSVC
 *
 * @param pvParameters Указатель на экземпляр SsvcOpenConnect
 *
 * Получает команды из очереди и выполняет их с учетом:
 * - Типа команды
 * - Количества попыток
 * - Таймаута ожидания ответа
 * - Обработки ответов через зарегистрированные callback'и
 */
void SsvcCommandsQueue::commandProcessorTask(void *pvParameters) {
  auto *self = static_cast<SsvcCommandsQueue *>(pvParameters);
  SsvcCommand *cmd = nullptr;
  // Пауза при откладывании обычной команды: за это время воркер успевает
  // опросить safety-очередь и извлечь зарезервированный GET_LOG.
  constexpr uint32_t kDeferRetryMs = 100;
  // Квант ожидания ответа: аварийный STOP не должен ждать полного таймаута
  // текущей команды, поэтому ожидание разбито на такие интервалы.
  constexpr TickType_t kSafetyPollTicks = pdMS_TO_TICKS(100);

  while (true) {
    // Приоритет аварийных команд: safety-очередь забирается без ожидания, до
    // общего FIFO, поэтому STOP не зависит от накопленных обычных команд.
    if (xQueueReceive(self->safety_queue, &cmd, 0) != pdPASS) {
      if (xQueueReceive(self->command_queue, &cmd, pdMS_TO_TICKS(500)) != pdPASS) {
        // Пока очередь пуста, проверяется deadline незавершённой лог-сессии:
        // потерянный LIST/START не должен оставить её занятой без HTTP-polling.
        SsvcLogProtocol::getTransfer().checkTimeout(millis());
        continue;
      }
    }

    ESP_LOGD(TAG, "Processing command of type: %d", cmd->type);
    bool command_success = false;
    /// Исход последней попытки: нужен, чтобы отличить «нет ответа» от отказа
    /// устройства, когда попытки уже исчерпаны.
    enum class AttemptOutcome : uint8_t {
      None,
      NoResponse,
      DeviceRejected,
      CallbackFailed
    };
    AttemptOutcome outcome = AttemptOutcome::None;
    /// Была ли хотя бы одна успешная запись в UART (для итогового состояния).
    bool sent = false;
    /// Ожидание прервано аварийной командой (STOP в safety-очереди).
    bool preemptedBySafety = false;
    /// Запись журнала уже переведена в терминальное состояние в этой итерации.
    bool journalFinalized = false;

    // Команда отменённой или устаревшей лог-сессии до отправки не доходит.
    if (cmd->generation != 0 &&
        cmd->generation != SsvcLogProtocol::getTransfer().generation()) {
      ESP_LOGW(TAG, "Command %d из отменённой сессии пропущена",
               static_cast<int>(cmd->type));
      self->journalFinish(cmd->journalId, SsvcCommandState::Rejected, 0,
                          "cancelled_before_send");
      delete cmd;
      continue;
    }

    // Неотправленная неидемпотентная команда, устаревшая после аварийной
    // остановки, не «догоняет» STOP: опасен именно догоняющий START/NEXT/RESUME.
    if (!isIdempotentRetry(cmd->type) && cmd->safetyEpoch != self->_safetyEpoch) {
      ESP_LOGW(TAG, "Command %d устарела после emergency_stop и отброшена",
               static_cast<int>(cmd->type));
      self->journalFinish(cmd->journalId, SsvcCommandState::Rejected, 0,
                          "stale_after_emergency_stop");
      delete cmd;
      continue;
    }

    // Обычные команды не вклиниваются в активную лог-сессию. Команда возвращается
    // в хвост очереди, а не удерживается в цикле: воркер продолжает опрашивать
    // safety-очередь и способен извлечь зарезервированный GET_LOG. Удержание
    // блокировало и приоритет STOP, и саму лог-сессию до её таймаута.
    if (cmd->type != SsvcCommandType::GET_LOG && cmd->type != SsvcCommandType::STOP &&
        SsvcLogProtocol::getTransfer().isBlocked()) {
      SsvcLogProtocol::getTransfer().checkTimeout(millis());
      vTaskDelay(pdMS_TO_TICKS(kDeferRetryMs));
      // Слот освободился при извлечении команды, но его мог занять другой
      // постановщик — повтор до успешного возврата.
      while (xQueueSendToBack(self->command_queue, &cmd, 0) != pdPASS) {
        vTaskDelay(pdMS_TO_TICKS(kDeferRetryMs));
      }
      continue;
    }

    // Воркер забрал команду: до первой записи в UART она в журнале "running".
    self->journalMarkRunning(cmd->journalId);

    while (cmd->attempt_count != 0) {
      cmd->attempt_count--;

      // Поздний ответ уже отброшенной по таймауту команды не должен
      // засчитываться этой: биты ожидания и отказа сбрасываются до записи в
      // UART, иначе чужой BIT9 читается как успех, а BIT1 — как отказ
      // устройства. GET_LOG ответа через eventGroup не ждёт.
      if (cmd->type != SsvcCommandType::GET_LOG) {
        const auto expectedBitIt = self->commandToExpectedBit.find(cmd->type);
        if (expectedBitIt != self->commandToExpectedBit.end()) {
          xEventGroupClearBits(eventGroup, expectedBitIt->second | BIT1);
        }
      }

      // Отправка команды в зависимости от типа
      switch (cmd->type) {
        case SsvcCommandType::GET_SETTINGS:
          command_success =
              SsvcConnector::sendCommand("GET_SETTINGS\n");
          break;
        case SsvcCommandType::SET: {
          std::ostringstream oss;
          oss << "SET " << cmd->parameters << "\n";
          command_success =
              SsvcConnector::sendCommand(oss.str().c_str());
          break;
        }
        case SsvcCommandType::VERSION:
          command_success = SsvcConnector::sendCommand("VERSION\n");
          break;
        case SsvcCommandType::STOP:
          // STOP не ждёт файловую сессию (см. проверку выше). Если она активна,
          // передача переводится в дренирование: кадры старого потока не должны
          // завершить её «успешно».
          if (SsvcLogProtocol::getTransfer().isFileSession()) {
            SsvcLogProtocol::getTransfer().cancel();
          }
          command_success = SsvcConnector::sendCommand("STOP\n");
          break;
        case SsvcCommandType::START:
          command_success = SsvcConnector::sendCommand("START\n");
          break;
        case SsvcCommandType::PAUSE:
          command_success = SsvcConnector::sendCommand("PAUSE\n");
          break;
        case SsvcCommandType::RESUME:
          command_success = SsvcConnector::sendCommand("RESUME\n");
          break;
        case SsvcCommandType::NEXT:
          command_success = SsvcConnector::sendCommand("NEXT\n");
          break;
        case SsvcCommandType::AT:
          command_success = SsvcConnector::sendCommand("AT\n");
          break;
        case SsvcCommandType::STATUS: {
          std::ostringstream oss;
          oss << "STATUS " << cmd->parameters << "\n";
          command_success =
              SsvcConnector::sendCommand(oss.str().c_str());
          break;
        }
        case SsvcCommandType::GET_LOG: {
          auto &transfer = SsvcLogProtocol::getTransfer();
          // generation != 0 означает, что резерв сессии уже сделан в REST.
          // Переход QUEUED_* -> WAITING_* выполняется до записи в UART, чтобы
          // состояние не разошлось с фактом отправки.
          const bool reserved = cmd->generation != 0;
          std::string request;

          if (cmd->parameters.empty()) {
            if (!reserved && !transfer.beginList(millis())) {
              command_success = false;
              break;
            }
            if (!SsvcLogProtocol::formatListRequest(request)) {
              command_success = false;
              break;
            }
          } else {
            char* end = nullptr;
            const long processNumber = std::strtol(cmd->parameters.c_str(), &end, 10);
            if (end == cmd->parameters.c_str() || *end != '\0' ||
                processNumber <= 0 || processNumber > std::numeric_limits<int>::max() ||
                !SsvcLogProtocol::formatFileRequest(static_cast<int>(processNumber), request) ||
                (!reserved && !transfer.beginFile(static_cast<int>(processNumber), millis()))) {
              command_success = false;
              break;
            }
          }

          if (reserved) {
            transfer.markSent();
          }
          command_success = SsvcConnector::sendCommand(request.c_str());
          if (!command_success) {
            // Запись не прошла — резерв откатывается, чтобы клиент не ждал
            // ответа, которого не будет.
            transfer.abortReservation("uart_write_failed");
          }
          break;
        }
        }
        ESP_LOGD(TAG, "Send result: %d", command_success);

        if (!command_success) {
          ESP_LOGE(TAG, "Failed to send command %d",
                   static_cast<int>(cmd->type));
          vTaskDelay(pdMS_TO_TICKS(2000));
          continue;
        }

        // Байты ушли в UART: с этого момента запись честно "sent", даже если
        // ответа не будет вовсе.
        self->journalMarkSent(cmd->journalId);
        sent = true;

        if (cmd->type == SsvcCommandType::GET_LOG) {
          // GET_LOG не ждёт бита: сессию завершает SsvcLogProtocol. Ответа,
          // который можно привязать к команде, нет — остаётся "sent".
          self->journalFinish(cmd->journalId, SsvcCommandState::Sent, -1, nullptr);
          journalFinalized = true;
          break;
        }

        // Ожидание ответа с определенным битом
        const EventBits_t expectedBit = self->commandToExpectedBit[cmd->type];
        ESP_LOGV(TAG, "Waiting for response with bit: %d",
                 expectedBit);
        const EventBits_t responseMask = expectedBit | BIT1;

        // Успешная запись в UART ещё не означает выполнение команды: результат
        // сбрасывается перед ожиданием, а BIT1 (отказ устройства) не
        // маршрутизируется в callback ожидаемого бита.
        command_success = false;
        EventBits_t bits = 0;
        const TickType_t responseDeadline = xTaskGetTickCount() + cmd->timeout;
        while (bits == 0) {
          const TickType_t left = responseDeadline - xTaskGetTickCount();
          if (left <= 0) {
            break;
          }
          const TickType_t slice = left < kSafetyPollTicks ? left : kSafetyPollTicks;
          bits = xEventGroupWaitBits(eventGroup, responseMask, pdTRUE, pdFALSE, slice);
          if (bits != 0) {
            break;
          }
          // STOP ждёт в safety-очереди, а не в этом ожидании: «мёртвая» команда
          // не должна держать воркер полный таймаут (до 13–16 с) до отправки
          // аварийной остановки. Ответ на прерванную попытку уже не нужен.
          if (cmd->type != SsvcCommandType::STOP &&
              uxQueueMessagesWaiting(self->safety_queue) > 0) {
            preemptedBySafety = true;
            break;
          }
        }

        if ((bits & expectedBit) != 0) {
          // Callback'и читают lastMessage/lastResponse сами: getLast* берут
          // блокировку внутри. Внешний захват того же нерекурсивного mutex
          // приводил к отказу внутреннего захвата и «пустому» ответу.
          auto it = self->bitCallbacks.find(expectedBit);
          ESP_LOGV(TAG, "Callback found: %d",
                   it != self->bitCallbacks.end());
          if (it != self->bitCallbacks.end()) {
            command_success = it->second(cmd);
            outcome = command_success ? AttemptOutcome::None
                                      : AttemptOutcome::CallbackFailed;
            ESP_LOGV(TAG, "Command %d processed with result: %d",
                     cmd->type, command_success);
          } else {
            // Бит пришёл, но обработать его некому: ответ unusable.
            outcome = AttemptOutcome::CallbackFailed;
            ESP_LOGW(TAG, "No callback handler for bit %d",
                     expectedBit);
          }
        } else if ((bits & BIT1) != 0) {
          outcome = AttemptOutcome::DeviceRejected;
          ESP_LOGW(TAG, "Command %d rejected by device (BIT1)",
                   static_cast<int>(cmd->type));
        } else {
          outcome = AttemptOutcome::NoResponse;
          ESP_LOGW(TAG, "Command %d: no response, outcome unknown",
                   static_cast<int>(cmd->type));
        }

        if (preemptedBySafety) {
          // Повтор бессмысленен: STOP уже в очереди и должен уйти первым.
          // Исход прерванной команды неизвестен, поэтому она не повторяется.
          ESP_LOGW(TAG, "Command %d прервана аварийной командой",
                   static_cast<int>(cmd->type));
          break;
        }

        if (command_success) {
          ESP_LOGV(TAG, "Command %d executed successfully",
                   static_cast<int>(cmd->type));
          break;
        }

        // Неидемпотентную команду нельзя повторять после отправки: контроллер
        // мог её выполнить, а ответ потеряться.
        if (!isIdempotentRetry(cmd->type)) {
          ESP_LOGW(TAG, "Command %d: retry skipped after uncertain outcome",
                   static_cast<int>(cmd->type));
          break;
        }

        if (cmd->attempt_count != 0) {
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
      }

      if (!journalFinalized) {
        // Итог записывается ровно один раз. "sent" без ответа — это не таймаут:
        // для START/NEXT/RESUME/STOP/GET_LOG ответа может не быть вовсе, и такая
        // запись честно означает «отправлено, ответа не ждём».
        if (preemptedBySafety) {
          self->journalFinish(cmd->journalId, SsvcCommandState::Sent, -1,
                              "preempted_by_safety");
        } else if (command_success) {
          self->journalFinish(cmd->journalId, SsvcCommandState::Done, 1, nullptr);
        } else if (!sent) {
          self->journalFinish(cmd->journalId, SsvcCommandState::Rejected, 0,
                              "uart_write_failed");
        } else if (outcome == AttemptOutcome::DeviceRejected) {
          // BIT1: устройство ответило отказом (result != OK).
          self->journalFinish(cmd->journalId, SsvcCommandState::Rejected, 0,
                              "device_rejected");
        } else if (outcome == AttemptOutcome::CallbackFailed) {
          // Бит успеха пришёл, но ответ не разобран/не принят обработчиком.
          self->journalFinish(cmd->journalId, SsvcCommandState::Rejected, 0,
                              "invalid_response");
        } else if (mayStayUnanswered(cmd->type)) {
          self->journalFinish(cmd->journalId, SsvcCommandState::Sent, -1, nullptr);
        } else {
          self->journalFinish(cmd->journalId, SsvcCommandState::Timeout, 0,
                              "no_response");
        }
      }

      delete cmd;
      // Логирование использования стека задачи
      const UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(nullptr);
      ESP_LOGD(TAG, "CmdProcessor Task: Stack High Water Mark: %u bytes", stackHighWaterMark);
  }
}

/**
 * @brief Регистрация callback-обработчиков для различных команд.
 *
 * Регистрирует обработчики для:
 * - BIT10: Обработка ответа на GET_SETTINGS
 * - BIT11: Обработка ответа на VERSION
 * - BIT9: Обработка успешного выполнения команд
 * - BIT1: Обработка ошибок выполнения команд
 */
void SsvcCommandsQueue::registerCallbackCommands() {
  // Регистрация обработчика для GET_SETTINGS ответов
  registerBitHandler(BIT10, [](SsvcCommand *cmd) {
    const std::string message = SsvcConnector::getConnector().getLastMessage();
    if (message.empty()) {
      // Снимок недоступен (mutex занят) — не подменяем настройки пустым кадром.
      ESP_LOGW(TAG, "BIT10: пустой снимок кадра GET_SETTINGS");
      return false;
    }
    SsvcSettings::init().load(message);
    return true;
  });

  registerBitHandler(BIT11, [](SsvcCommand *cmd) {
    std::string _message = SsvcConnector::getConnector().getLastResponse();
    JsonDocument response;
    const DeserializationError error = deserializeJson(response, _message);
    if (error) {
      return false;
    }

    bool result = false;
    if (response["version"].is<String>()) {
      const std::string ssvcVersion = response["version"].as<std::string>();
      SsvcSettings::init().setSsvcVersion(ssvcVersion);
      result = true;
    }
    if (response["api"].is<String>()) {
      // Версия приходит строкой: "1.10" нельзя читать как float — получится 1.1,
      // то есть формально меньше "1.7". Разбор идёт в целый код major*100+minor.
      SsvcSettings::init().setSsvcApiVersion(response["api"].as<std::string>());
      result = true;
    }
    if (result) {
      SsvcOpenConnect::sendHello();
    }
    return result;
  });

  // Бит для всех команд с успешным выполнением
  registerBitHandler(BIT9, [this](SsvcCommand *cmd) {
    std::string message;
    std::string responseRequest;
    SsvcConnector::getConnector().getResponseSnapshot(message, responseRequest);
    if (message.empty()) {
      ESP_LOGW(TAG, "BIT9: пустой снимок ответа");
      return false;
    }

    JsonDocument response;
    if (deserializeJson(response, message)) {
      ESP_LOGW(TAG, "BIT9: ответ не разобран как JSON");
      return false;
    }

    // Ответ должен относиться к отправленной команде: если контроллер указал
    // request, его глагол обязан совпасть — иначе это ответ другой команды.
    // Параметры в request не сравниваются: контроллер повторяет команду целиком
    // («STATUS OpenConnect», «SET heads=[1,2]»). payload и request берутся одним
    // снимком, поэтому проверка не гоняется с телеметрией.
    const std::string expectedRequest = requestNameFor(cmd->type);
    const std::string responseVerb = requestVerb(responseRequest);
    if (!responseVerb.empty() && !expectedRequest.empty() &&
        responseVerb != expectedRequest) {
      ESP_LOGW(TAG, "BIT9: ответ %s не соответствует команде %s",
               responseRequest.c_str(), expectedRequest.c_str());
      return false;
    }

    //      Если в полученном ответе будет SET, значит был запрос изменения
    //      настроек, а значит их нужно будет перечитать заново
    const std::string responseRequestField = response["request"] | "";
    if (requestVerb(responseRequestField) == "SET") {
      const std::lock_guard<std::mutex> lock(_timerMutex);
      if (_settingsTimer == nullptr) {
        // Создание одноразового таймера
        _settingsTimer =
          xTimerCreate("get_settings_timer",
                       pdMS_TO_TICKS(30000),
                       pdFALSE,
                       this,
                       [](const TimerHandle_t xTimer) {
                         auto *self = static_cast<SsvcCommandsQueue *>(
                           pvTimerGetTimerID(xTimer));
                         if (self) {
                           self->getSettings();
                           // После выполнения очищаем и удаляем таймер
                           xTimerDelete(xTimer, 0);
                           const std::lock_guard<std::mutex> guard(self->_timerMutex);
                           self->_settingsTimer = nullptr;
                         }
                       });

        if (_settingsTimer != nullptr) {
          if (xTimerStart(_settingsTimer, 0) != pdPASS) {
            ESP_LOGW(TAG, "get_settings_timer: не удалось запустить");
            xTimerDelete(_settingsTimer, 0);
            _settingsTimer = nullptr;
          }
        } else {
          ESP_LOGW(TAG, "get_settings_timer: не удалось создать");
        }
      } else {
        // Если таймер уже существует — просто сбросить отсчёт
        if (xTimerReset(_settingsTimer, 0) != pdPASS) {
          ESP_LOGW(TAG, "get_settings_timer: не удалось перезапустить");
        }
      }
    }
    return std::string(response["result"] | "") == "OK";
  });

  // Отказ устройства (BIT1) не считается успехом: обработчик возвращает false,
  // чтобы ожидающий код увидел неуспех. Флаг результата здесь не нужен —
  // результат каждой команды возвращается из обработчика ожидаемого бита.
  registerBitHandler(BIT1, [](SsvcCommand *cmd) {
    (void)cmd;
    return false;
  });
}

/**
 * @brief Единая точка постановки команды в очередь и единственное место gate.
 *
 * Проверка версии API стоит здесь, а не в set()/getLog(): новая
 * версионозависимая команда не сможет её обойти. Для SET из пакета
 * исключаются поля, которых нет на устройстве, для GET_LOG проверяется
 * наличие возможности.
 *
 * @param type Тип команды (например, SET, GET_SETTINGS и т.д.)
 * @param parameters Параметры команды (строка).
 * @param attempt_count Количество попыток при неудаче.
 * @param timeout Тайм-аут ожидания ответа (в тиках).
 * @param generation Номер лог-сессии для GET_LOG (0 — команда вне сессии).
 * @param skippedOut Приёмник отброшенных по версии API полей SET. Владелец —
 *        вызывающая операция; nullptr означает, что отчёт не нужен. Общий
 *        накопитель здесь недопустим: HTTP, MQTT и профили идут из разных задач.
 * @return true, если команда поставлена в очередь.
 */
bool SsvcCommandsQueue::pushCommandInQueue(const SsvcCommandType type,
                                           const std::string& parameters,
                                           const int attempt_count,
                                           const TickType_t timeout,
                                           const unsigned long generation,
                                           std::vector<std::string>* skippedOut,
                                           uint32_t* commandIdOut) const
{
  const SsvcSettings& settings = SsvcSettings::init();
  const int deviceCode = settings.getSsvcApiVersionCode();
  const std::string deviceApi = settings.getSsvcApiVersion();

  std::string effectiveParameters = parameters;

  // Параметры уходят в UART-кадр как есть, поэтому перевод строки превратил бы
  // одну команду в две ("SET x\nSTART" -> SET + START) и обошёл предстартовые
  // проверки (XKC-25 start-guard). Такой ввод отвергается в единственной точке
  // постановки, а не в отдельных транспортах.
  if (parameters.find_first_of("\r\n") != std::string::npos) {
    ESP_LOGW(TAG, "Команда отклонена: параметры содержат перевод строки");
    journalRejected(commandNameFor(type), parameters, "invalid_parameters");
    rememberRejection("invalid_parameters", "", "", deviceApi);
    return false;
  }

  if (type == SsvcCommandType::SET) {
    std::vector<std::string> skipped;
    std::string filtered;
    if (!SsvcApiCapabilities::filterSetParameters(parameters, deviceCode, skipped, filtered)) {
      // Ни один параметр команды не поддерживается версией устройства.
      int feature = SsvcApiCapabilities::FEATURE_BASE;
      SsvcUartApiSpec::ApiVersion required = SsvcApiCapabilities::minSupportedApiVersion();
      if (!skipped.empty()) {
        const SsvcApiCapabilities::SetParamRequirement requirement =
            SsvcApiCapabilities::requiredForSetParam(skipped.front());
        feature = requirement.feature;
        required = requirement.minVersion;
      }
      ESP_LOGW(TAG, "SET отклонён: ни один параметр не поддерживается устройством (API %s)",
               deviceApi.c_str());
      if (skippedOut != nullptr) {
        skippedOut->insert(skippedOut->end(), skipped.begin(), skipped.end());
      }
      journalRejected(commandNameFor(type), parameters, "no_supported_params");
      rememberRejection("no_supported_params", SsvcApiCapabilities::featureName(feature),
                        SsvcUartApiSpec::formatApiVersion(required), deviceApi);
      return false;
    }
    // Отчёт кладётся в приёмник вызывающей операции: REST-слой показывает, какие
    // поля не ушли на устройство, не смешивая их с параллельными запросами.
    if (skippedOut != nullptr) {
      skippedOut->insert(skippedOut->end(), skipped.begin(), skipped.end());
    }

    if (!skipped.empty()) {
      std::string skippedList;
      for (const std::string& item : skipped) {
        if (!skippedList.empty()) {
          skippedList += ", ";
        }
        skippedList += item;
      }
      ESP_LOGW(TAG, "SET: поля недоступны на API %s и пропущены: %s", deviceApi.c_str(),
               skippedList.c_str());
    }
    effectiveParameters = filtered;
  } else if (type == SsvcCommandType::GET_LOG) {
    if (!SsvcApiCapabilities::featureAvailable(deviceCode, SsvcApiCapabilities::FEATURE_GET_LOG)) {
      const std::string requiredApi = SsvcUartApiSpec::formatApiVersion(
          SsvcApiCapabilities::featureMinVersion(SsvcApiCapabilities::FEATURE_GET_LOG));
      ESP_LOGW(TAG, "GET_LOG недоступен: устройство на API %s, требуется %s", deviceApi.c_str(),
               requiredApi.c_str());
      journalRejected(commandNameFor(type), parameters, "feature_unsupported");
      rememberRejection("feature_unsupported", "get_log", requiredApi, deviceApi);
      return false;
    }
  }

  // Тракт должен быть явно готов: до begin() команды не ставятся, а отказ виден
  // вызывающему (REST отвечает ошибкой, а не «accepted»).
  if (!_ready || command_queue == nullptr) {
    journalRejected(commandNameFor(type), parameters, "not_ready");
    rememberRejection("not_ready", "", "", "");
    ESP_LOGW(TAG, "Команда %d отклонена: очередь не готова", static_cast<int>(type));
    return false;
  }

  ESP_LOGD(TAG, "Attempting to enqueue command type: %d", static_cast<int>(type));
  auto *cmd = new SsvcCommand();
  cmd->type = type;
  cmd->parameters = effectiveParameters;
  cmd->attempt_count = attempt_count;
  // timeout приходит уже в тиках (SsvcCommand/TIMEOUT): повторный pdMS_TO_TICKS
  // удлинял ожидание при tick != 1 мс.
  cmd->timeout = timeout;
  cmd->generation = generation;
  cmd->safetyEpoch = _safetyEpoch;

  // id назначается до постановки в очередь и принадлежит именно этой команде:
  // id возвращается только ей, а не «последнему принятому», который перезаписали
  // бы MQTT или бот. Мьютекс журнала держим только на время выдачи id:
  // xQueueSend при полной очереди ждёт до 1 с, и блокировка на это время
  // задержала бы и воркер (он не отметил бы running/sent), и GET
  // /rest/commands/{id}.
  uint32_t commandId = 0;
  {
    const std::lock_guard<std::mutex> lock(_journalMutex);
    commandId = beginRecordLocked(commandNameFor(type), effectiveParameters, millis()).id;
  }
  cmd->journalId = commandId;

  const bool queued = xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(1000)) == pdPASS;
  if (!queued) {
    journalFinish(commandId, SsvcCommandState::Rejected, 0, "queue_full");
    ESP_LOGE(TAG, "Failed to send command to queue! Queue might be full.");
    delete cmd;
    // Причина видна REST-слою: 503 с queue_full вместо «accepted».
    rememberRejection("queue_full", "", "", "");
    return false;
  }

  if (commandIdOut != nullptr) {
    *commandIdOut = commandId;
  }
  ESP_LOGD(TAG, "Command type %d enqueued successfully (id=%u).", static_cast<int>(type),
           static_cast<unsigned>(commandId));
  return true;
}

/**
 * @brief Запоминает причину последней отбраковки для REST-слоя.
 *
 * Пишут HTTP- и MQTT-задачи, читает REST-слой, поэтому поля защищены
 * мьютексом: без него одновременная запись и чтение std::string — UB.
 */
void SsvcCommandsQueue::rememberRejection(const std::string& reason,
                                          const std::string& feature,
                                          const std::string& requiredApi,
                                          const std::string& deviceApi) const
{
  const std::lock_guard<std::mutex> lock(_rejectionMutex);
  _lastRejectionReason = reason;
  _lastRejectedFeature = feature;
  _lastRejectedRequiredApi = requiredApi;
  _lastRejectedDeviceApi = deviceApi;
}

/**
 * @brief Имя состояния для REST-контракта GET /rest/commands/{id}.
 */
const char* SsvcCommandsQueue::commandStateName(const SsvcCommandState state)
{
  switch (state) {
  case SsvcCommandState::Queued:
    return "queued";
  case SsvcCommandState::Running:
    return "running";
  case SsvcCommandState::Sent:
    return "sent";
  case SsvcCommandState::Done:
    return "done";
  case SsvcCommandState::Rejected:
    return "rejected";
  case SsvcCommandState::Timeout:
    return "timeout";
  }
  return "unknown";
}

/// Вызывается только под _journalMutex. 0 зарезервирован под «записи нет».
uint32_t SsvcCommandsQueue::nextJournalIdLocked() const
{
  ++_journalLastId;
  if (_journalLastId == 0) {
    _journalLastId = 1;
  }
  return _journalLastId;
}

/// Вызывается только под _journalMutex. Слот берётся по кругу: самая старая
/// запись перезаписывается, поэтому GET по её id после переполнения даёт 404, а
/// не чужую команду.
SsvcCommandsQueue::CommandRecord& SsvcCommandsQueue::beginRecordLocked(
    const char* command, const std::string& parameters, const uint32_t now) const
{
  CommandRecord& record = _journal[_journalNext];
  _journalNext = static_cast<uint8_t>((_journalNext + 1) % JOURNAL_SIZE);
  record.id = nextJournalIdLocked();
  record.command = command;
  record.state = SsvcCommandState::Queued;
  record.ok = -1;
  record.reason = nullptr;
  record.queuedAt = now;
  record.startedAt = 0;
  record.finishedAt = 0;
  copyTruncated(record.parameters, sizeof(record.parameters), parameters);
  return record;
}

/// Вызывается только под _journalMutex.
SsvcCommandsQueue::CommandRecord* SsvcCommandsQueue::findRecordLocked(
    const uint32_t id) const
{
  if (id == 0) {
    return nullptr;
  }
  for (auto& record : _journal) {
    if (record.id == id) {
      return &record;
    }
  }
  return nullptr;
}

/**
 * @brief Отказ на постановке: терминальная запись с причиной.
 *
 * id такой записи клиенту не выдаётся (тела 400/409/501/503 не менялись),
 * запись остаётся в журнале для диагностики.
 */
void SsvcCommandsQueue::journalRejected(const char* command,
                                        const std::string& parameters,
                                        const char* reason) const
{
  const uint32_t now = millis();
  const std::lock_guard<std::mutex> lock(_journalMutex);
  CommandRecord& record = beginRecordLocked(command, parameters, now);
  record.state = SsvcCommandState::Rejected;
  record.ok = 0;
  record.reason = reason;
  record.finishedAt = now;
}

/// Воркер забрал команду (переход queued -> running).
void SsvcCommandsQueue::journalMarkRunning(const uint32_t id) const
{
  if (id == 0) {
    return;
  }
  const std::lock_guard<std::mutex> lock(_journalMutex);
  CommandRecord* record = findRecordLocked(id);
  if (record == nullptr || record->state != SsvcCommandState::Queued) {
    return;  // запись уже перезаписана кольцом
  }
  record->state = SsvcCommandState::Running;
  record->startedAt = millis();
}

/// Байты ушли в UART (running -> sent); finishedAt ставит journalFinish.
void SsvcCommandsQueue::journalMarkSent(const uint32_t id) const
{
  if (id == 0) {
    return;
  }
  const std::lock_guard<std::mutex> lock(_journalMutex);
  CommandRecord* record = findRecordLocked(id);
  if (record == nullptr) {
    return;
  }
  record->state = SsvcCommandState::Sent;
  if (record->startedAt == 0) {
    record->startedAt = millis();
  }
}

/// Терминальное состояние записи: done/rejected/timeout или sent-как-итог.
void SsvcCommandsQueue::journalFinish(const uint32_t id, const SsvcCommandState state,
                                      const int8_t ok, const char* reason) const
{
  if (id == 0) {
    return;
  }
  const uint32_t now = millis();
  const std::lock_guard<std::mutex> lock(_journalMutex);
  CommandRecord* record = findRecordLocked(id);
  if (record == nullptr) {
    return;
  }
  record->state = state;
  record->ok = ok;
  record->reason = reason;
  record->finishedAt = now;
}

/**
 * @brief Судьба команды по id: копия записи под мьютексом журнала.
 *
 * Копирование под блокировкой (а не сериализация) нужно, чтобы воркер не ждал
 * HTTP-задачу: JSON собирается уже по снимку.
 */
bool SsvcCommandsQueue::getCommandStatus(const uint32_t commandId,
                                         SsvcCommandStatus& out) const
{
  if (commandId == 0) {
    return false;
  }
  const uint32_t now = millis();
  const std::lock_guard<std::mutex> lock(_journalMutex);
  const CommandRecord* record = findRecordLocked(commandId);
  if (record == nullptr) {
    return false;
  }
  // TTL: кольцо перезаписывается по кругу, поэтому старая запись должна
  // «исчезнуть» и по времени, а не только по факту перезаписи.
  if (now - record->queuedAt > JOURNAL_TTL_MS) {
    return false;
  }
  out.id = record->id;
  out.command = record->command;
  out.state = record->state;
  out.queuedAt = record->queuedAt;
  out.startedAt = record->startedAt;
  out.finishedAt = record->finishedAt;
  out.ok = record->ok;
  out.reason = record->reason;
  return true;
}

/**
 * @brief Добавляет в очередь команду получения настроек SSVC.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
bool SsvcCommandsQueue::getSettings(const int attempt_count, const TickType_t timeout,
                                    uint32_t* commandIdOut) const
{
  return pushCommandInQueue(SsvcCommandType::GET_SETTINGS, "", attempt_count, timeout, 0,
                            nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь команду NEXT.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 * @return true, если команда поставлена в очередь.
 */
bool SsvcCommandsQueue::next(const int attempt_count, const TickType_t timeout,
                              uint32_t* commandIdOut) const
{
  return pushCommandInQueue(SsvcCommandType::NEXT, "", attempt_count, timeout, 0,
                            nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь команду PAUSE.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 * @return true, если команда поставлена в очередь.
 */
bool SsvcCommandsQueue::pause(const int attempt_count, const TickType_t timeout,
                               uint32_t* commandIdOut) const
{
  return pushCommandInQueue(SsvcCommandType::PAUSE, "", attempt_count, timeout, 0,
                            nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь команду STOP.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 * @return true, если команда поставлена в очередь.
 */
bool SsvcCommandsQueue::stop(const int attempt_count, const TickType_t timeout,
                              uint32_t* commandIdOut) const
{
  return pushCommandInQueue(SsvcCommandType::STOP, "", attempt_count, timeout, 0,
                            nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь команду START.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
bool SsvcCommandsQueue::start(const int attempt_count, const TickType_t timeout,
                               uint32_t* commandIdOut) const
{
  return pushCommandInQueue(SsvcCommandType::START, "", attempt_count, timeout, 0,
                            nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь команду RESUME.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 * @return true, если команда поставлена в очередь.
 */
bool SsvcCommandsQueue::resume(const int attempt_count, const TickType_t timeout,
                                uint32_t* commandIdOut) const
{
  return pushCommandInQueue(SsvcCommandType::RESUME, "", attempt_count, timeout, 0,
                            nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь команду VERSION.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 * @return true, если команда поставлена в очередь.
 */
bool SsvcCommandsQueue::version(const int attempt_count, const TickType_t timeout,
                                 uint32_t* commandIdOut) const
{
  return pushCommandInQueue(SsvcCommandType::VERSION, "", attempt_count, timeout, 0,
                            nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь AT-команду.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
bool SsvcCommandsQueue::at(const int attempt_count, const TickType_t timeout,
                            uint32_t* commandIdOut) const
{
  return pushCommandInQueue(SsvcCommandType::AT, "", attempt_count, timeout, 0,
                            nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь команду SET с параметрами.
 *
 * Фильтрация полей по версии API выполняется в pushCommandInQueue — единой
 * точке gate, чтобы проверку нельзя было обойти из нового вызова.
 *
 * @param parameters Параметры команды.
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 * @param skippedOut Приёмник отброшенных по версии API полей; может быть nullptr.
 * @return true, если команда поставлена в очередь.
 */
bool SsvcCommandsQueue::set(const std::string& parameters, const int attempt_count,
                            const TickType_t timeout,
                            std::vector<std::string>* skippedOut,
                            uint32_t* commandIdOut) const
{
  ESP_LOGD(TAG, "Set command called with parameters: %s", parameters.c_str());
  return pushCommandInQueue(SsvcCommandType::SET, parameters, attempt_count, timeout, 0,
                            skippedOut, commandIdOut);
}

/**
 * @brief Публикует информацию в количестве 15 символов на дисплей ssvc.
 *
 * @param parameters Информационное сообщение.
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
bool SsvcCommandsQueue::status(const std::string& parameters, const int attempt_count,
                               const TickType_t timeout, uint32_t* commandIdOut) const {
  return pushCommandInQueue(SsvcCommandType::STATUS, utf8_to_win1251(parameters), attempt_count,
                            timeout, 0, nullptr, commandIdOut);
}

/**
 * @brief Добавляет в очередь команду GET_LOG.
 *
 * Проверка возможности выполняется в pushCommandInQueue — единой точке gate.
 *
 * @param parameters Параметры команды.
 * @param generation Номер зарезервированной лог-сессии (0 — без резерва).
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 * @return true, если команда поставлена в очередь.
 */
bool SsvcCommandsQueue::getLog(const std::string& parameters, const unsigned long generation,
                               const int attempt_count, const TickType_t timeout,
                               uint32_t* commandIdOut) const {
  return pushCommandInQueue(SsvcCommandType::GET_LOG, parameters, attempt_count, timeout,
                            generation, nullptr, commandIdOut);
}

/**
 * @brief Фоновая задача для периодической отправки AT команд (отладка).
 *
 * @param pvParameters Указатель на экземпляр SsvcOpenConnect
 *
 * Отправляет AT команды каждые 20 секунд для проверки соединения.
 */
void SsvcCommandsQueue::sendAT(void *pvParameters) {
  const auto *self = static_cast<SsvcCommandsQueue *>(pvParameters);
  vTaskDelay(pdMS_TO_TICKS(6000));

  while (true) {
    self->at();
    vTaskDelay(pdMS_TO_TICKS(20000));
  }
}
