#include "RectificationProcess.h"


// RectificationProcess.cpp
RectificationProcess::RectificationProcess()
    : _ssvcConnector(nullptr),
      _ssvcSettings(nullptr),
      _openConnectSettingsService(nullptr),
      currentStage(RectificationStage::EMPTY),
      previousStage(RectificationStage::EMPTY),
      currentProcessStatus(ProcessState::IDLE),
      pid(0),
      pidChecked(false),
      eventReceived(false) {
  memset(startTime, 0, sizeof(startTime));
  memset(endTime, 0, sizeof(endTime));
}

void RectificationProcess::begin(SsvcConnector& connector,
                               SsvcSettings& settings,
                               OpenConnectSettingsService& openConnectSettingsService) {
  _ssvcConnector = &connector;
  _ssvcSettings = &settings;
  _openConnectSettingsService = &openConnectSettingsService;

  xTaskCreatePinnedToCore(
      update,
      "RectTelemetry",
      4096,
      this,
      (tskIDLE_PRIORITY),
      nullptr,
      1
  );
}

// Сопоставление строки "type" с этапами RectificationStage
RectificationProcess::RectificationStage
RectificationProcess::stringToRectificationStage(const std::string& stageStr)
{
  if (stageStr == "waiting")
    return RectificationStage::WAITING;
  if (stageStr == "tp1_waiting")
    return RectificationStage::TP1_WAITING;
  if (stageStr == "delayed_start")
    return RectificationStage::DELAYED_START;
  if (stageStr == "heads")
    return RectificationStage::HEADS;
  if (stageStr == "late_heads")
    return RectificationStage::LATE_HEADS;
  if (stageStr == "hearts")
    return RectificationStage::HEARTS;
  if (stageStr == "tails")
    return RectificationStage::TAILS;
  if (stageStr == "settings")
    return RectificationStage::SETTINGS;
  return RectificationStage::ERROR;
}


// Добавьте функцию для преобразования RectificationStage в строку
std::string RectificationProcess::stageToString(RectificationStage stage)
{
  ESP_LOGV("SsvcOpenConnect", "stageToString: %d", static_cast<int>(stage));
  switch (stage)
  {
  case RectificationStage::EMPTY:
    return "empty";
  case RectificationStage::WAITING:
    return "waiting";
  case RectificationStage::TP1_WAITING:
    return "tp1_waiting";
  case RectificationStage::DELAYED_START:
    return "delayed_start";
  case RectificationStage::HEADS:
    return "heads";
  case RectificationStage::LATE_HEADS:
    return "late_heads";
  case RectificationStage::HEARTS:
    return "hearts";
  case RectificationStage::TAILS:
    return "tails";
  case RectificationStage::ERROR:
    return "error";
  default:
    return "unknown";
  }
}

// Добавьте функцию для преобразования RectificationStage в строку
std::string RectificationProcess::translateRectificationStage(const std::string& stageStr)
{
  if (stageStr == "waiting")
    return "Дежурный режим";
  if (stageStr == "tp1_waiting")
    return "Ожидание нагрева колонны";
  if (stageStr == "delayed_start")
    return "Отложенный старт";
  if (stageStr == "heads")
    return "Головы";
  if (stageStr == "late_heads")
    return "Подголовники";
  if (stageStr == "hearts")
    return "Тело";
  if (stageStr == "tails")
    return "Хвосты";
  if (stageStr == "settings")
    return "<UNK>";
  return stageStr;
}

std::string
RectificationProcess::stateToString(RectificationProcess::ProcessState state)
{
  switch (state)
  {
  case ProcessState::RUNNING:
    return "running";
  case ProcessState::PAUSED:
    return "paused";
  case ProcessState::FINISHED:
    return "finished";
  case ProcessState::SKIPPED:
    return "skipped";
  case ProcessState::IDLE:
    return "idle";
  default:
    return "unknown";
  }
}

std::string
RectificationProcess::rectificationEventToString(RectificationEvent event)
{
  switch (event)
  {
  case RectificationEvent::HEADS_FINISHED:
    return "heads_finished";
  case RectificationEvent::HEARTS_FINISHED:
    return "hearts_finished";
  case RectificationEvent::TAILS_FINISHED:
    return "tails_finished";
  case RectificationEvent::DS_ERROR:
    return "ds_error";
  case RectificationEvent::DS_ERROR_STOP:
    return "ds_error_stop";
  case RectificationEvent::STABILIZATION_LIMIT:
    return "stabilization_limit";
  case RectificationEvent::REMOTE_STOP:
    return "remote_stop";
  case RectificationEvent::MANUALLY_CLOSED:
    return "manually_closed";
  case RectificationEvent::MANUALLY_OPENED:
    return "manually_opened";
  default:
    return "unknown_event"; // Можно вернуть "unknown_event" или пустую строку
  // для неизвестных значений
  }
}


RectificationProcess::RectificationEvent
RectificationProcess::stringToRectificationEvent(std::string& eventString)
{
  if (eventString == "heads_finished")
  {
    return RectificationEvent::HEADS_FINISHED;
  }
  else if (eventString == "hearts_finished")
  {
    return RectificationEvent::HEARTS_FINISHED;
  }
  else if (eventString == "tails_finished")
  {
    return RectificationEvent::TAILS_FINISHED;
  }
  else if (eventString == "ds_error_stop")
  {
    return RectificationEvent::DS_ERROR_STOP;
  }
  else if (eventString == "stabilization_limit")
  {
    return RectificationEvent::STABILIZATION_LIMIT;
  }
  else if (eventString == "remote_stop")
  {
    return RectificationEvent::REMOTE_STOP;
  }
  else if (eventString == "manually_closed")
  {
    return RectificationEvent::MANUALLY_CLOSED;
  }
  else if (eventString == "manually_opened")
  {
    return RectificationEvent::MANUALLY_OPENED;
  }
  else if (eventString == "ds_error")
  {
    return RectificationEvent::DS_ERROR;
  }
  return RectificationEvent::ERROR;
}

std::string RectificationProcess::rectificationEventToDescription(
  RectificationEvent event)
{
  switch (event)
  {
  case RectificationEvent::HEADS_FINISHED:
    return "Завершен этап отбора голов";
  case RectificationEvent::HEARTS_FINISHED:
    return "Завершен этап отбора тела (ректификация завершена для firmware "
      "2.3.*)";
  case RectificationEvent::TAILS_FINISHED:
    return "Завершен этап отбора хвостов (ректификация завершена для firmware "
      "2.2.*)";
  case RectificationEvent::DS_ERROR:
    return "Ошибка датчика температуры";
  case RectificationEvent::DS_ERROR_STOP:
    return "Выключение оборудования (реле) из-за ошибки датчика. Срабатывает "
      "через 180 секунд, если ошибка текущего датчика не исчезнет.";
  case RectificationEvent::STABILIZATION_LIMIT:
    return "Превышен лимит времени стабилизации";
  case RectificationEvent::REMOTE_STOP:
    return "Получена удаленная команда остановки, процесс остановлен";
  case RectificationEvent::MANUALLY_CLOSED:
    return "Включено ручное управление клапаном текущего этапа, клапан закрыт";
  case RectificationEvent::MANUALLY_OPENED:
    return "Включено ручное управление клапаном текущего этапа, клапан открыт";
  default:
    return "Неизвестное событие"; // Для неизвестных значений
  }
}

// подготавливает данные телеметрии для отображения исходя из полученных данных
// от ssvc
void RectificationProcess::update(void* pvParameters)
{
  auto* self = static_cast<RectificationProcess*>(pvParameters);
  const char* taskName = pcTaskGetName(nullptr);
  ESP_LOGE("RectificationProcess",
           "Запуск задачи подготовки данных ректификации");
  std::string currentEvent;

  while (true)
  {
    UBaseType_t stackWaterMark = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGV("SsvcOpenConnect", "Task %s: Stack high water mark: %u", taskName,
             stackWaterMark);

    // Ожидание события
    EventBits_t bits =
      xEventGroupWaitBits(eventGroup,
                          BIT0, // Ждём BIT0 (например, новое сообщение)
                          pdTRUE, // Сбрасываем флаг после обработки
                          pdFALSE, // Любое событие
                          portMAX_DELAY);

    if (bits & BIT0)
    {
      std::string message;
      if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
      {
        message = self->_ssvcConnector->getLastMessage();

        // Десериализация JSON
        JsonDocument telemetry;
        DeserializationError error = deserializeJson(telemetry, message);
        if (error)
        {
          ESP_LOGE("RectificationProcess", "Ошибка десериализации JSON");
          xSemaphoreGive(mutex);
          continue;
        }
        // Для начала очистим от старых данных
        // Получение типа этапа из JSON
        std::string type = telemetry["type"];
        ESP_LOGV("RectificationProcess", "Тип этапа: %s", type.c_str());
        //          Определение текущего этапа
        RectificationStage _currentStage = stringToRectificationStage(type);

        if (_currentStage == RectificationStage::SETTINGS)
        {
          ESP_LOGW("RectificationProcess", "Открыты настройки");
          xSemaphoreGive(mutex);
          continue;
        }
        else
        {
          self->metric = {};
        }
        // Контроль уникального идентификатора процесса
        if (telemetry["pid"].is<int>())
        {
          //                    Значение PID в телеметрии приходит только в
          //                    момент работы процесса.
          int newPid = telemetry["pid"].as<int>();
          ESP_LOGV("RectificationProcess", "PID: %d", newPid);
          //                  Попадание в условие означает что запускается новый
          //                  процесс
          if (self->pid != newPid && newPid != 0)
          {
            ESP_LOGI("RectificationProcess", "Новый PID: %d", newPid);
            // Проверка наличия сохраненного pid
            if (!self->pidChecked || self->pidChecked && self->pid != newPid)
            {
              ESP_LOGI("RectificationProcess", "Проверка сохраненного PID");
              int savedPid = 0;
              // self->_openConnectSettingsService.read(
              //   [&](OpenConnectSettings& settings)
              //   {
              //     savedPid = settings.pid;
              //   });
              if (savedPid != 0)
              {
                ESP_LOGI("RectificationProcess", "Сохраненный PID: %d",
                         savedPid);
                if (savedPid != newPid)
                {
                  ESP_LOGI("RectificationProcess",
                           "Сохраненный PID не совпадает с новым PID");
                  // self->_openConnectSettingsService.update(
                  //   [&](OpenConnectSettings& settings)
                  //   {
                  //     settings.pid = newPid; // turn on the lights
                  //     return StateUpdateResult::CHANGED; // notify
                  //     // StatefulService by
                  //     // returning CHANGED
                  //   },
                  //   "rprocess");
                }
              }
              self->pidChecked = true;
            }
            self->pid = newPid;
            self->currentProcessStatus = ProcessState::RUNNING;

            time_t now = time(nullptr);
            struct tm timeInfo{};
            localtime_r(&now, &timeInfo);
            strftime(self->startTime, 25, "%Y-%m-%d %H:%M:%S", &timeInfo);
            memset(self->endTime, 0, sizeof(endTime));

            self->flowVolumeValves = {};
            self->rectificationStageStates = {};
          }
          else
          {
            ESP_LOGV("RectificationProcess", "Процесс уже идет.");
          }
        }
        else
        {
          //                    В телеметрии PID не пришел, значит процесс не
          //                    идет
          if (_currentStage == RectificationStage::WAITING)
          {
            // Пид был установлен ранее, а теперь отсутствует и режим waiting -
            // Завершен процесс
            if (self->pid != 0)
            {
              // Pid процесса больше не приходит. Значит процесс ректификации
              // окончен
              self->pid = 0;
              self->currentProcessStatus = ProcessState::FINISHED;
              time_t now = time(nullptr);
              struct tm timeInfo{};
              localtime_r(&now, &timeInfo);
              strftime(self->endTime, 25, "%Y-%m-%d %H:%M:%S", &timeInfo);
            }
            else
            {
              self->currentProcessStatus = ProcessState::IDLE;
              memset(self->startTime, 0, sizeof(startTime));
              memset(self->endTime, 0, sizeof(endTime));
            }
          }
          else
          {
            ESP_LOGI("RectificationProcess", "Процесс не идет.");
          }
        }

        //              Отработка изменения этапа и заполнение таблицы
        //              прохождения этапов
        if (self->currentStage != _currentStage)
        {
          //        Необходимо отработать вхождение в настройки SSVC кнопками
          //        И получения ответа type = settings
          ESP_LOGV("RectificationProcess", "Этап изменен: %s",
                   stageToString(_currentStage).c_str());
          // Заполнение таблицы прохождения этапов
          self->rectificationStageStates[self->previousStage] =
            ProcessState::FINISHED;
          self->previousStage = self->currentStage;
          self->currentStage = _currentStage;
          self->rectificationStageStates[self->currentStage] =
            ProcessState::RUNNING;
        }
        else
        {
          ESP_LOGV("RectificationProcess", "Этап не изменен: %s",
                   stageToString(_currentStage).c_str());
        }

        if (_currentStage == RectificationStage::ERROR)
        {
          ESP_LOGE("RectificationProcess", "Ошибка получения типа этапа");
          xSemaphoreGive(mutex);
          continue;
        }
        ESP_LOGV("RectificationProcess", "currentStage: %s",
                 stageToString(_currentStage).c_str());
        // Обновление состояний этапов

        //            Обновление состояния
        bool hasStartTime = isNonEmptyString(self->startTime);
        bool hasEndTime = isNonEmptyString(self->endTime);

        //               Отработка поступивших собыйтий
        if (telemetry["event"].is<std::string>())
        {
          std::string event = telemetry["event"].as<std::string>();
          self->metric.event = stringToRectificationEvent(event);
          self->startEventHandler(event);
          currentEvent = event;
          self->eventReceived = true;
          ESP_LOGV("SsvcSettings", "Обновлен event: %s", event.c_str());
        }

        if (self->eventReceived && !telemetry["event"].is<std::string>())
        {
          self->eventReceived = false;
          self->endEventHandler(currentEvent);
          currentEvent = "";
        }

        ESP_LOGV("RectificationProcess", "startTime: %s", self->startTime);
        ESP_LOGV("RectificationProcess", "EndTime: %s", self->endTime);

        //            Заполняем таблицу состояния этапов

        self->rectificationStageStates[self->currentStage] =
          RectificationProcess::ProcessState::RUNNING;
        if (self->rectificationStageStates[self->previousStage] ==
          RectificationProcess::ProcessState::RUNNING)
        {
          self->rectificationStageStates[self->previousStage] =
            RectificationProcess::ProcessState::FINISHED;
        }
        if (self->rectificationStageStates[self->previousStage] ==
          RectificationProcess::ProcessState::IDLE)
        {
          self->rectificationStageStates[self->previousStage] =
            RectificationProcess::ProcessState::SKIPPED;
        }

        self->metric.type = telemetry["type"].as<std::string>();
        self->metric.common.mmhg =
          telemetry["common"]["mmhg"].as<unsigned int>();
        self->metric.common.tp1 = telemetry["common"]["tp1"].as<float>();
        self->metric.common.tp2 = telemetry["common"]["tp2"].as<float>();
        self->metric.common.relay = telemetry["common"]["relay"].as<int>() != 0;
        self->metric.common.signal =
          telemetry["common"]["signal"].as<int>() != 0;
        // Выводим значения полей common в лог
        ESP_LOGV("Telemetry",
                 "Metric Common - mmhg: %u, tp1: %.2f, tp2: %.2f, relay: %d, "
                 "signal: %d",
                 self->metric.common.mmhg, self->metric.common.tp1,
                 self->metric.common.tp2, self->metric.common.relay,
                 self->metric.common.signal);

        if (telemetry["tp1_target"].is<float>())
        {
          auto tp1_target = telemetry["tp1_target"].as<float>();
          ESP_LOGV("SsvcSettings", "tp1_target: %f", tp1_target);
          self->metric.tp1_target = tp1_target;
          ESP_LOGV("SsvcSettings", "Обновлен tp1_target: %f",
                   self->metric.tp1_target);
        }

        if (telemetry["tp2_target"].is<float>())
        {
          self->metric.tp2_target = telemetry["tp2_target"].as<float>();
          ESP_LOGV("SsvcSettings", "Обновлен tp2_target: %f",
                   self->metric.tp2_target);
        }

        if (telemetry["countdown"].is<std::string>())
        {
          self->metric.countdown = telemetry["countdown"].as<std::string>();
          ESP_LOGV("SsvcSettings", "Обновлен countdown: %s",
                   self->metric.countdown.c_str());
        }

        if (telemetry["release"].is<std::string>())
        {
          self->metric.release = telemetry["release"].as<std::string>();
          ESP_LOGV("SsvcSettings", "Обновлен release: %s",
                   self->metric.release.c_str());
        }

        if (telemetry["time"].is<std::string>())
        {
          self->metric.time = telemetry["time"].as<std::string>();
          ESP_LOGV("SsvcSettings", "Обновлен time: %s",
                   self->metric.time.c_str());
        }

        if (telemetry["open"].is<float>())
        {
          self->metric.open = telemetry["open"].as<float>();
          ESP_LOGV("SsvcSettings", "Обновлен open: %f", self->metric.open);
        }

        if (telemetry["period"].is<int>())
        {
          self->metric.period = telemetry["period"].as<int>();
          ESP_LOGV("SsvcSettings", "Обновлен period: %d", self->metric.period);
        }

        if (telemetry["tank_mmhg"].is<int>())
        {
          self->metric.tank_mmhg = telemetry["tank_mmhg"].as<int>();
          ESP_LOGV("SsvcSettings", "Обновлен tank_mmhg: %d",
                   self->metric.tank_mmhg);
        }

        if (telemetry["tp1_sap"].is<float>())
        {
          self->metric.tp1_sap = telemetry["tp1_sap"].as<float>();
          ESP_LOGV("SsvcSettings", "Обновлен tp1_sap: %f",
                   self->metric.tp1_sap);
        }

        if (telemetry["tp2_sap"].is<float>())
        {
          self->metric.tp2_sap = telemetry["tp2_sap"].as<float>();
          ESP_LOGV("SsvcSettings", "Обновлен tp2_sap: %f",
                   self->metric.tp2_sap);
        }

        if (telemetry["hysteresis"].is<float>())
        {
          self->metric.hysteresis = telemetry["hysteresis"].as<float>();
          ESP_LOGV("SsvcSettings", "Обновлен hysteresis: %f",
                   self->metric.hysteresis);
        }

        if (telemetry["v1"].is<int>())
        {
          self->metric.v1 = telemetry["v1"].as<int>();
          ESP_LOGV("SsvcSettings", "Обновлен v1: %d", self->metric.v1);
        }

        if (telemetry["v2"].is<int>())
        {
          self->metric.v2 = telemetry["v2"].as<int>();
          ESP_LOGV("SsvcSettings", "Обновлен v2: %d", self->metric.v2);
        }

        if (telemetry["v3"].is<int>())
        {
          self->metric.v3 = telemetry["v3"].as<int>();
          ESP_LOGV("SsvcSettings", "Обновлен v3: %d", self->metric.v3);
        }

        if (telemetry["alc"].is<float>())
        {
          self->metric.alc = telemetry["alc"].as<float>();
          ESP_LOGV("SsvcSettings", "Обновлен alc: %f", self->metric.alc);
        }

        if (telemetry["stop"].is<int>())
        {
          self->metric.stop = telemetry["stop"].as<int>() == 1;
          ESP_LOGV("SsvcSettings", "Обновлен stop: %u", self->metric.stop);
        }

        if (telemetry["stops"].is<unsigned char>())
        {
          self->metric.stops = telemetry["stops"].as<unsigned char>();
          ESP_LOGV("SsvcSettings", "Обновлен stops: %u", self->metric.stops);
        }

        // Пересчет количества отобранного продукта
        self->recalculateFlowVolume(telemetry["v1"], telemetry["v2"],
                                    telemetry["v3"]);

        ESP_LOGV("RectificationProcess", "LastMessage %s", message.c_str());
        xSemaphoreGive(mutex); // Копирование данных
      }
      else
      {
        ESP_LOGE("RectificationProcess",
                 "Не удалось захватить мьютекс для _message");
        return;
      }
    }
  }
}

void RectificationProcess::startEventHandler(const std::string& event)
{
  if (event == "manually_closed" || event == "manually_opened")
  {
    currentProcessStatus = ProcessState::PAUSED;
  }
  else if (event == "remote_stop")
  {
    currentProcessStatus = ProcessState::STOPPED;
  }
  else if (event == "heads_finished")
  {
    rectificationStageStates[RectificationStage::HEADS] =
      ProcessState::FINISHED;
  }
  else if (event == "late_heads_finished")
  {
    rectificationStageStates[RectificationStage::LATE_HEADS] =
      ProcessState::FINISHED;
  }
  else if (event == "hearts_finished")
  {
    rectificationStageStates[RectificationStage::HEARTS] =
      ProcessState::FINISHED;
  }
  else if (event == "tails_finished")
  {
    rectificationStageStates[RectificationStage::TAILS] =
      ProcessState::FINISHED;
  }
  else if (event == "ds_error" || event == "ds_error_stop")
  {
    currentProcessStatus = ProcessState::ERROR;
  }
}

void RectificationProcess::endEventHandler(std::string currentEvent)
{
  //    TODO:Доработать блок, после сбора данных событий
  if (currentEvent == "manually_closed" || currentEvent == "manually_opened")
  {
    currentProcessStatus = ProcessState::RUNNING;
  }
  else if (currentEvent == "ds_error" || currentEvent == "ds_error_stop")
  {
    currentProcessStatus = ProcessState::RUNNING;
  }
}

bool RectificationProcess::getStatus(JsonVariant status)
{
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
  {
    status["stage"] = stageToString(currentStage);
    status["status"] = stateToString(currentProcessStatus);
    status["startTime"] = startTime;
    status["endTime"] = endTime;

    JsonObject stages = status["stages"].to<JsonObject>();
    for (auto& rectificationStageState : rectificationStageStates)
    {
      auto& stage =
        rectificationStageState.first; // Ключ (тип RectificationStage)
      auto& state =
        rectificationStageState.second; // Значение (тип RectificationState)
      if (stage != RectificationStage::EMPTY)
      {
        stages[stageToString(stage)] = stateToString(state);
      }
    }

    xSemaphoreGive(mutex);
    char buffer[512]; // Размер подберите под ваши нужды
    size_t len = serializeJson(status, buffer, sizeof(buffer));

    if (len >= sizeof(buffer))
    {
      ESP_LOGV("RectificationProcess", "JSON truncated! Needed %d bytes", len);
    }

    // Выводим результат
    ESP_LOGV("RectificationProcess", "JSON: %.*s", len, buffer);
    return true;
  }
  return false;
}

RectificationProcess::Metrics& RectificationProcess::getMetrics()
{
  return  metric;
}


std::string RectificationProcess::getTelemetry()
{
  if (xSemaphoreTake(mutex, portMAX_DELAY))
  {
    JsonDocument _message;

    _message["type"] = metric.type;

    if (pid != 0)
    {
      _message["pid"] = pid;
    }

    if (strlen(startTime) > 0)
    {
      _message["start_time"] = startTime;
    }
    if (strlen(endTime) > 0)
    {
      _message["end_time"] = endTime;
    }
    if (metric.tank_mmhg != 0)
    {
      _message["tank_mmhg"] = metric.tank_mmhg;
    }

    if (metric.tp1_target != 0)
    {
      _message["tp1_target"] = metric.tp1_target;
    }
    if (metric.tp2_target != 0)
    {
      _message["tp2_target"] = metric.tp2_target;
    }

    if (!metric.countdown.empty())
    {
      _message["countdown"] = metric.countdown;
    }

    if (!metric.release.empty())
    {
      _message["release"] = metric.release;
    }

    if (!metric.time.empty())
    {
      _message["time"] = metric.time;
    }

    if (metric.open > 0)
    {
      _message["open"] = metric.open;
    }
    if (metric.period > 0)
    {
      _message["period"] = metric.period;
    }

    if (metric.period > 0)
    {
      int valveOpen = static_cast<int>(
        std::round((100.0 * metric.open / metric.period) * 100.0));
      _message["valveOpen"] = valveOpen;
      int volumeSpeed = 0;
      if (calculateVolumeSpeed(valveOpen, volumeSpeed))
      {
        _message["volumeSpeed"] = volumeSpeed;
      }
    }
    if (metric.v1 > 0)
    {
      _message["v1"] = metric.v1;
    }

    if (metric.v2 > 0)
    {
      _message["v2"] = metric.v2;
    }

    if (metric.v3 > 0)
    {
      _message["v3"] = metric.v3;
    }

    _message["stop"] = metric.stop;
    _message["stops"] = metric.stops;

    JsonObject common = _message["common"].to<JsonObject>();
    if (metric.common.mmhg)
    {
      common["mmhg"] = metric.common.mmhg;
    }
    if (metric.tp1_sap != 0)
    {
      common["tp1"] = metric.tp1_sap;
    }
    else
    {
      common["tp1"] = metric.common.tp1;
    }
    if (metric.tp1_sap != 0)
    {
      common["tp2"] = metric.tp2_sap;
    }
    else
    {
      common["tp2"] = metric.common.tp2;
    }
    common["relay"] = metric.common.relay;
    common["signal"] = metric.common.signal;
    common["heatingOn"] = isHeatingOn();
    common["overclockingOn"] = isOverclockingOn();
    common["hysteresis"] = metric.hysteresis;

    //    TODO Вопрос по отправке каждый раз данных по уже прошедшим отборам
    //    сомнительный
    // Пока оставлю так как есть. Затем возможно переделаю. На получение через
    // POST.
    JsonObject volume = _message["volume"].to<JsonObject>();
    if (flowVolumeValves[RectificationStage::HEADS] != 0)
    {
      volume["heads"] = flowVolumeValves[RectificationStage::HEADS];
    }
    if (flowVolumeValves[RectificationStage::LATE_HEADS] != 0)
    {
      volume["late_heads"] = flowVolumeValves[RectificationStage::LATE_HEADS];
    }
    if (flowVolumeValves[RectificationStage::HEARTS] != 0)
    {
      volume["hearts"] = flowVolumeValves[RectificationStage::HEARTS];
    }
    if (flowVolumeValves[RectificationStage::TAILS] != 0)
    {
      volume["tails"] = flowVolumeValves[RectificationStage::TAILS];
    }

    // Сообщение о событии и прочее уведомление
    if (metric.event != RectificationEvent::EMPTY)
    {
      _message["event"] = rectificationEventToString(metric.event);
      //          TODO В информацию пока заводим только обработку event. В
      //          последствии это нужно будет расширить
      _message["info"] = rectificationEventToDescription(metric.event);
    }

    if (metric.alc != 0)
    {
      _message["alc"] = metric.alc;
    }
    std::string json;
    serializeJson(_message, json);
    ESP_LOGV("RECTIFICATION", "Отладка json: %s", json.c_str());
    xSemaphoreGive(mutex);
    return json;
  }
  else
  {
    ESP_LOGE("RectificationProcess", "Failed to take mutex");
    return "{}";
  }
}

bool RectificationProcess::isHeatingOn()
{
  bool relay_inverted = _ssvcSettings->getRelayInverted();
  return relay_inverted ? !metric.common.relay : metric.common.relay;
}

bool RectificationProcess::isOverclockingOn()
{
  bool signal_inverted = _ssvcSettings->getSignalInverted();
  return signal_inverted ? metric.common.signal : !metric.common.signal;
}

void RectificationProcess::recalculateFlowVolume(int v1, int v2, int v3)
{
  std::array<int, 3> bw = _ssvcSettings->getValveBw();
  ESP_LOGV("RectificationProcess", "Valve Bw: [%d, %d, %d]", bw[0], bw[1],
           bw[2]);
  int value = 0;
  if (currentStage == RectificationStage::HEADS)
  {
    value = (bw[0] * v1) / 3600; // Целочисленное деление
  }
  else if (currentStage == RectificationStage::HEARTS)
  {
    value = (bw[1] * v2) / 3600; // Целочисленное деление
  }
  else if (currentStage == RectificationStage::TAILS ||
    currentStage == RectificationStage::LATE_HEADS)
  {
    value = (bw[2] * v3) / 3600; // Целочисленное деление
  }
  else
  {
    return;
  }
  ESP_LOGV("RectificationProcess", "Отобранный объем: %d мл", value);
  flowVolumeValves[currentStage] = value;

  for (auto& _flowVolumeValves : flowVolumeValves)
  {
    auto& stage = _flowVolumeValves.first; // Ключ (тип RectificationStage)
    auto& _value =
      _flowVolumeValves.second; // Значение (тип RectificationState)
    ESP_LOGV("Volume", "Processing stage: %s, current value: %d",
             stageToString(stage).c_str(), _value);
  }
}

bool RectificationProcess::calculateVolumeSpeed(int valveOpen,
                                                int& volumeSpeed)
{
  std::array<int, 3> bw = _ssvcSettings->getValveBw();
  if (currentStage == RectificationStage::HEADS && bw[0] != 0)
  {
    volumeSpeed = (bw[0] * valveOpen) / 10000;
    return true;
  }
  else if (currentStage == RectificationStage::HEARTS && bw[1] != 0)
  {
    volumeSpeed = (bw[1] * valveOpen) / 10000;
    return true;
  }
  else if ((currentStage == RectificationStage::LATE_HEADS ||
      currentStage == RectificationStage::TAILS) &&
    bw[2] != 0)
  {
    volumeSpeed = (bw[2] * valveOpen) / 10000;
    return true;
  }
  else
  {
    return false;
  }
}

bool RectificationProcess::isNonEmptyString(const char* str)
{
  return str && str[0] != '\0';
}

