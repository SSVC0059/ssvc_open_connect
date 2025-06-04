//
// Created by demoncat on 09.04.2025.
//

#include "SsvcCommandsQueue.h"

/**
 * @brief Конструктор класса SsvcCommandsQueue.
 *
 * Создаёт очередь команд для SSVC  и запускает задачу обработки.
 */
SsvcCommandsQueue::SsvcCommandsQueue() {
  command_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, COMMAND_QUEUE_ITEM_SIZE);
  if (command_queue == nullptr) {
    ESP_LOGE("SsvcQueue", "Failed to create command queue!");
  } else {
    ESP_LOGI("SsvcQueue", "Command queue created successfully");
  }

  xTaskCreatePinnedToCore(
      SsvcCommandsQueue::commandProcessorTask, // Указатель на функцию задачи
      "CmdProcessor",                          // Имя задачи для отладки
      4096,                                    // Размер стека (байт)
      this,                                    // Параметр для передачи в задачу
      (tskIDLE_PRIORITY), // Приоритет (0-24, где 24 - наивысший)
      nullptr,            // Дескриптор задачи (не используется)
      1                   // Ядро процессора (0 или 1)
  );

  //  // Отладочная задача на отправку AT команд
  //  xTaskCreatePinnedToCore(SsvcCommandsQueue::sendAT, "CmdProcessor", 4096,
  //  this,
  //                          (tskIDLE_PRIORITY + 3), nullptr, 1);

  registerCallbackCommands();
}

/**
 * @brief Callback-функция для отложенного получения настроек.
 *
 * Используется таймером для вызова метода getSettings().
 *
 * @param xTimer Указатель на таймер, инициировавший вызов.
 */
void delayed_get_settings_callback(TimerHandle_t xTimer) {
  auto *self = static_cast<SsvcCommandsQueue *>(pvTimerGetTimerID(xTimer));
  if (self) {
    self->getSettings();
  }

  // Удаление таймера после выполнения
  xTimerDelete(xTimer, 0);
}

/**
 * @brief Регистрация callback-обработчиков для различных команд.
 *
 * Регистрирует обработчики для:
 * - BIT10: Обработка ответа на GET_SETTINGS
 * - BIT9: Обработка успешного выполнения команд
 * - BIT1: Обработка ошибок выполнения команд
 */
void SsvcCommandsQueue::registerCallbackCommands() {
  // Регистрация обработчика для GET_SETTINGS ответов
  registerBitHandler(BIT10, [this](SsvcCommand *cmd) {
    ESP_LOGI("CustomHandler", "Обработка GET_SETTINGS ответа");
    std::string _message = SsvcConnector::getConnector().getLastMessage();
    ESP_LOGI("CustomHandler", "Message: %s", _message.c_str());
    SsvcSettings::init().load(_message);
    return true;
  });

  registerBitHandler(BIT11, [this](SsvcCommand *cmd) {
    ESP_LOGI("CustomHandler", "Обработка команды VERSION");
    std::string _message = SsvcConnector::getConnector().getLastResponse();
    JsonDocument response;
    DeserializationError error = deserializeJson(response, _message);
    if (error) {
      ESP_LOGE("RectificationProcess", "Ошибка десериализации JSON");
      xSemaphoreGive(mutex);
      return false;
    }
    if (response["version"].is<String>()) {
      std::string version = response["version"].as<std::string>();
      ESP_LOGI("CustomHandler", "Версия: %s", version.c_str());
      xSemaphoreGive(mutex);
      return true;
    }
    if (response["api"].is<String>()) {
      std::string version = response["api"].as<std::string>();
      ESP_LOGI("CustomHandler", "Версия: %s", version.c_str());
      xSemaphoreGive(mutex);
      return true;
    }
    return false;
  });

  // Бит для всех команд с успешным выполнением
  registerBitHandler(BIT9, [this](SsvcCommand *cmd) {
    ESP_LOGV("BIT9 Handler", "Processing normal response");
    std::string _message = SsvcConnector::getConnector().getLastResponse();
    //      Если в полученном ответе будет SET, значит был запрос изменения
    //      настроек, а значит их нужно будет перечитать заново
    if (_message.find("SET") != std::string::npos) {
      if (_settingsTimer == nullptr) {
        // Создание одноразового таймера
        _settingsTimer =
            xTimerCreate("get_settings_timer",
                         pdMS_TO_TICKS(30000), // 30 секунд
                         pdFALSE,              // не периодический
                         this, // идентификатор — указатель на объект
                         [](TimerHandle_t xTimer) {
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
            ESP_LOGE("TIMER", "Failed to start timer");
            xTimerDelete(_settingsTimer, 0);
            _settingsTimer = nullptr;
          }
        } else {
          ESP_LOGE("TIMER", "Failed to create timer");
        }
      } else {
        // Если таймер уже существует — просто сбросить отсчёт
        if (xTimerReset(_settingsTimer, 0) != pdPASS) {
          ESP_LOGE("TIMER", "Failed to reset timer");
        }
      }
    }
    bool result = _message.find("OK") != std::string::npos;
    _cmdSetResult = result;
    return result;
  });

  // Обработка ошибок выполнения команд
  registerBitHandler(BIT1, [this](SsvcCommand *cmd) {
    ESP_LOGI("BIT1 Handler", "Ошибка выполнения команды");
    _cmdSetResult = true;
    return false;
  });
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
      ESP_LOGI("CommandProcessor", "Processing command of type: %d", cmd->type);
      bool command_success = false;

      while (cmd->attempt_count != 0 && !command_success) {
        bool send_result = false;

        cmd->attempt_count--;

        // Отправка команды в зависимости от типа
        switch (cmd->type) {
        case SsvcCommandType::GET_SETTINGS:
          send_result =
              SsvcConnector::getConnector().sendCommand("GET_SETTINGS\n");
          break;
        case SsvcCommandType::SET: {
          std::ostringstream oss;
          oss << "SET " << cmd->parameters << "\n";
          send_result =
              SsvcConnector::getConnector().sendCommand(oss.str().c_str());
          break;
        }
        case SsvcCommandType::VERSION:
          send_result = SsvcConnector::getConnector().sendCommand("VERSION\n");
          break;
        case SsvcCommandType::STOP:
          send_result = SsvcConnector::getConnector().sendCommand("STOP\n");
          break;
        case SsvcCommandType::START:
          send_result = SsvcConnector::getConnector().sendCommand("START\n");
          break;
        case SsvcCommandType::PAUSE:
          send_result = SsvcConnector::getConnector().sendCommand("PAUSE\n");
          break;
        case SsvcCommandType::RESUME:
          send_result = SsvcConnector::getConnector().sendCommand("RESUME\n");
          break;
        case SsvcCommandType::NEXT:
          send_result = SsvcConnector::getConnector().sendCommand("NEXT\n");
          break;
        case SsvcCommandType::AT:
          send_result = SsvcConnector::getConnector().sendCommand("AT\n");
          break;
        }
        ESP_LOGI("CommandProcessor", "Send result: %d", send_result);

        if (!send_result) {
          ESP_LOGE("CommandProcessor", "Failed to send command %d",
                   static_cast<int>(cmd->type));
          vTaskDelay(pdMS_TO_TICKS(2000));
          continue;
        }

        // Ожидание ответа с определенным битом
        EventBits_t expectedBit = self->commandToExpectedBit[cmd->type];
        ESP_LOGV("CommandProcessor", "Waiting for response with bit: %d",
                 expectedBit);
        EventBits_t responseMask = expectedBit | BIT1;

        EventBits_t bits = xEventGroupWaitBits(eventGroup, responseMask, pdTRUE,
                                               pdFALSE, cmd->timeout);

        if ((bits & responseMask) != 0) {
          MutexLock lock(mutex);

          // Обработка ответа через зарегистрированные callback'и
          auto it = self->bitCallbacks.find(expectedBit);
          ESP_LOGV("CommandProcessor", "Callback found: %d",
                   it != self->bitCallbacks.end());
          if (it != self->bitCallbacks.end()) {
            command_success = it->second(cmd);
            ESP_LOGV("CommandProcessor", "Command %d processed with result: %d",
                     cmd->type, command_success);
          } else {
            ESP_LOGW("CommandProcessor", "No callback handler for bit %d",
                     expectedBit);
          }
        }

        if (command_success) {
          ESP_LOGV("CommandProcessor", "Command %d executed successfully",
                   static_cast<int>(cmd->type));
          break;
        }

        if (cmd->attempt_count != 0) {
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
      }

      delete cmd;
    }
  }
}

/**
 * @brief Добавляет произвольную команду в очередь команд.
 *
 * @param type Тип команды (например, SET, GET_SETTINGS и т.д.)
 * @param parameters Параметры команды (строка).
 * @param attempt_count Количество попыток при неудаче.
 * @param timeout Таймаут ожидания ответа (в тиках).
 */
void SsvcCommandsQueue::pushCommandInQueue(SsvcCommandType type,
                                           std::string parameters,
                                           int attempt_count,
                                           TickType_t timeout) {
  auto *cmd = new SsvcCommand();
  cmd->type = type;
  cmd->parameters = parameters;
  cmd->attempt_count = attempt_count;
  cmd->timeout = pdMS_TO_TICKS(timeout);

  if (xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
    ESP_LOGE("SsvcCommandsQueue", "Failed to send command to queue");
    delete cmd;
  }
}

/**
 * @brief Добавляет в очередь команду получения настроек SSVC.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::getSettings(int attempt_count, TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::GET_SETTINGS, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду NEXT.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::next(int attempt_count, TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::NEXT, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду PAUSE.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::pause(int attempt_count, TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::PAUSE, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду STOP.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::stop(int attempt_count, TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::STOP, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду START.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::start(int attempt_count, TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::START, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду RESUME.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::resume(int attempt_count, TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::RESUME, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду VERSION.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::version(int attempt_count, TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::GET_SETTINGS, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь AT-команду.
 *
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::at(int attempt_count, TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::AT, "", attempt_count, timeout);
}

/**
 * @brief Добавляет в очередь команду SET с параметрами.
 *
 * @param parameters Параметры команды.
 * @param attempt_count Количество попыток.
 * @param timeout Таймаут ожидания (в тиках).
 */
void SsvcCommandsQueue::set(std::string parameters, int attempt_count,
                            TickType_t timeout) {
  pushCommandInQueue(SsvcCommandType::SET, parameters, attempt_count, timeout);
}

/**
 * @brief Фоновая задача для периодической отправки AT команд (отладка).
 *
 * @param pvParameters Указатель на экземпляр SsvcOpenConnect
 *
 * Отправляет AT команды каждые 20 секунд для проверки соединения.
 */
void SsvcCommandsQueue::sendAT(void *pvParameters) {
  auto *self = static_cast<SsvcCommandsQueue *>(pvParameters);
  vTaskDelay(pdMS_TO_TICKS(6000));

  while (true) {
    self->at();
    vTaskDelay(pdMS_TO_TICKS(20000));
  }
}
