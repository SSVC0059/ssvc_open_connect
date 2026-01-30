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

#include "SsvcOpenConnect.h"

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

// Инициализация статической карты команд
const std::map<std::string, std::function<void(const std::string&)>> SsvcCommandsQueue::COMMAND_MAP = {
    {"at",           [](const std::string&){ getQueue().at(); }},
    {"next",         [](const std::string&){ getQueue().next(); }},
    {"pause",        [](const std::string&){ getQueue().pause(); }},
    {"stop",         [](const std::string&){ getQueue().stop(); }},
    {"start",        [](const std::string&){ getQueue().start(); }},
    {"resume",       [](const std::string&){ getQueue().resume(); }},
    {"version",      [](const std::string&){ getQueue().version(); }},
    {"get_settings", [](const std::string&){ getQueue().getSettings(); }},
    {"settings",     [](const std::string&){ getQueue().getSettings(); }},
    {"emergency_stop", [](const std::string&){ getQueue().stop(); }},
    {"status",       [](const std::string& params){ getQueue().status(params); }},
    {"set",          [](const std::string& params){ getQueue().set(params); }}
};

SsvcCommandsQueue::SsvcCommandsQueue() {
  command_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, COMMAND_QUEUE_ITEM_SIZE);
  if (command_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create command queue!");
  } else {
    ESP_LOGI(TAG, "Command queue created successfully");
  }

  xTaskCreatePinnedToCore(
      commandProcessorTask,
      "CmdProcessor",
      4096,
      this,                                    
      (tskIDLE_PRIORITY),
      nullptr,
      1
  );

  registerCallbackCommands();
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
  vTaskDelay(pdMS_TO_TICKS(6000));

  while (true) {
    if (xQueueReceive(self->command_queue, &cmd, portMAX_DELAY) == pdPASS) {
      ESP_LOGI(TAG, "Processing command of type: %d", cmd->type);
      bool command_success = false;

      while (cmd->attempt_count != 0) {
        cmd->attempt_count--;

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
        case SsvcCommandType::STATUS:
          std::ostringstream oss;
          oss << "STATUS " << cmd->parameters << "\n";
          command_success =
              SsvcConnector::sendCommand(oss.str().c_str());
          break;
        }
        ESP_LOGI(TAG, "Send result: %d", command_success);

        if (!command_success) {
          ESP_LOGE(TAG, "Failed to send command %d",
                   static_cast<int>(cmd->type));
          vTaskDelay(pdMS_TO_TICKS(2000));
          continue;
        }

        // Ожидание ответа с определенным битом
        EventBits_t expectedBit = self->commandToExpectedBit[cmd->type];
        ESP_LOGV(TAG, "Waiting for response with bit: %d",
                 expectedBit);
        const EventBits_t responseMask = expectedBit | BIT1;

        const EventBits_t bits = xEventGroupWaitBits(eventGroup, responseMask, pdTRUE,
                                               pdFALSE, cmd->timeout);

        if ((bits & responseMask) != 0) {
          MutexLock lock(mutex);

          // Обработка ответа через зарегистрированные callback'и
          auto it = self->bitCallbacks.find(expectedBit);
          ESP_LOGV(TAG, "Callback found: %d",
                   it != self->bitCallbacks.end());
          if (it != self->bitCallbacks.end()) {
            command_success = it->second(cmd);
            ESP_LOGV(TAG, "Command %d processed with result: %d",
                     cmd->type, command_success);
          } else {
            ESP_LOGW(TAG, "No callback handler for bit %d",
                     expectedBit);
          }
        }

        if (command_success) {
          ESP_LOGV(TAG, "Command %d executed successfully",
                   static_cast<int>(cmd->type));
          break;
        }

        if (cmd->attempt_count != 0) {
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
      }

      delete cmd;
    }
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
    const std::string _message = SsvcConnector::getConnector().getLastMessage();
    SsvcSettings::init().load(_message);
    return true;
  });

  registerBitHandler(BIT11, [](SsvcCommand *cmd) {
    std::string _message = SsvcConnector::getConnector().getLastResponse();
    JsonDocument response;
    const DeserializationError error = deserializeJson(response, _message);
    if (error) {
      xSemaphoreGive(mutex);
      return false;
    }

    bool result = false;
    if (response["version"].is<String>()) {
      const std::string ssvcVersion = response["version"].as<std::string>();
      SsvcSettings::init().setSsvcVersion(ssvcVersion);
      xSemaphoreGive(mutex);
      result = true;
    }
    if (response["api"].is<String>()) {
      const auto ssvcApiVersion = response["api"].as<float>();
      SsvcSettings::init().setSsvcApiVersion(ssvcApiVersion);
      xSemaphoreGive(mutex);
      result = true;
    }
    if (result) {
      SsvcOpenConnect::sendHello();
    }
    return result;
  });

  // Бит для всех команд с успешным выполнением
  registerBitHandler(BIT9, [this](SsvcCommand *cmd) {
    const std::string _message = SsvcConnector::getConnector().getLastResponse();
    //      Если в полученном ответе будет SET, значит был запрос изменения
    //      настроек, а значит их нужно будет перечитать заново
    if (_message.find("SET") != std::string::npos) {
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
                           self->_settingsTimer = nullptr;
                         }
                       });

        if (_settingsTimer != nullptr) {
          if (xTimerStart(_settingsTimer, 0) != pdPASS) {
            xTimerDelete(_settingsTimer, 0);
            _settingsTimer = nullptr;
          }
        } else {
        }
      } else {
        // Если таймер уже существует — просто сбросить отсчёт
        if (xTimerReset(_settingsTimer, 0) != pdPASS) {
        }
      }
    }
    const bool result = _message.find("OK") != std::string::npos;
    _cmdSetResult = result;
    return result;
  });

  // Обработка ошибок выполнения команд
  registerBitHandler(BIT1, [this](SsvcCommand *cmd) {
    _cmdSetResult = true;
    return false;
  });
}

/**
 * @brief Добавляет произвольную команду в очередь команд.
 *
 * @param type Тип команды (например, SET, GET_SETTINGS и т.д.)
 * @param parameters Параметры команды (строка).
 * @param attempt_count Количество попыток при неудаче.
 * @param timeout Тайм-аут ожидания ответа (в тиках).
 */
void SsvcCommandsQueue::pushCommandInQueue(const SsvcCommandType type,
                                           const std::string& parameters,
                                           const int attempt_count,
                                           const TickType_t timeout) const
{
  ESP_LOGD(TAG, "Attempting to enqueue command type: %d", static_cast<int>(type));
  auto *cmd = new SsvcCommand();
  cmd->type = type;
  cmd->parameters = parameters;
  cmd->attempt_count = attempt_count;
  cmd->timeout = pdMS_TO_TICKS(timeout);

  if (xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(1000)) != pdPASS) {
    ESP_LOGE(TAG, "Failed to send command to queue! Queue might be full.");
    delete cmd;
  } else {
    ESP_LOGI(TAG, "Command type %d enqueued successfully.", static_cast<int>(type));
  }
}

/**
 * @brief Добавляет в очередь команду получения настроек SSVC.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::getSettings(const int attempt_count, const TickType_t timeout) const
{
  pushCommandInQueue(SsvcCommandType::GET_SETTINGS, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду NEXT.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::next(const int attempt_count, const TickType_t timeout) const
{
  pushCommandInQueue(SsvcCommandType::NEXT, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду PAUSE.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::pause(const int attempt_count, const TickType_t timeout) const
{
  pushCommandInQueue(SsvcCommandType::PAUSE, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду STOP.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::stop(const int attempt_count, const TickType_t timeout) const
{
  pushCommandInQueue(SsvcCommandType::STOP, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду START.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::start(const int attempt_count, const TickType_t timeout) const
{
  pushCommandInQueue(SsvcCommandType::START, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду RESUME.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::resume(const int attempt_count,const TickType_t timeout) const
{
  pushCommandInQueue(SsvcCommandType::RESUME, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду VERSION.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::version(const int attempt_count, const TickType_t timeout) const
{
  pushCommandInQueue(SsvcCommandType::VERSION, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь AT-команду.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::at(const int attempt_count, const TickType_t timeout) const
{
  pushCommandInQueue(SsvcCommandType::AT, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду SET с параметрами.
 *
 * @param parameters Параметры команды.
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::set(const std::string& parameters, const int attempt_count,
                            const TickType_t timeout) const
{
  ESP_LOGI(TAG, "Set command called with parameters: %s", parameters.c_str());
  pushCommandInQueue(SsvcCommandType::SET, parameters, attempt_count, timeout);
}

/**
 * @brief Публикует информацию в количестве 15 символов на дисплей ssvc.
 *
 * @param parameters Информационное сообщение.
 * @param attempt_count Количество попыток.
 * @param timeout Тайм-аут ожидания (в тиках).
 */
void SsvcCommandsQueue::status(const std::string& parameters, const int attempt_count,
                               const TickType_t timeout) const {
  pushCommandInQueue(SsvcCommandType::STATUS, utf8_to_win1251(parameters), attempt_count, timeout);
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
