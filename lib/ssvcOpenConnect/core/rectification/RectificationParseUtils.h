/**
 *   SSVC Open Connect
 *
 *   Чистые функции парсинга строк телеметрии ректификации.
 *   Без Arduino/ESP32 — для native-тестов.
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 **/

#ifndef RECTIFICATION_PARSE_UTILS_H
#define RECTIFICATION_PARSE_UTILS_H

#include "RectificationTypes.h"
#include <string>

namespace RectificationParseUtils {

inline RectificationTypes::Stage stringToStage(const std::string& stageStr) {
  if (stageStr == "waiting") return RectificationTypes::Stage::WAITING;
  if (stageStr == "tp1_waiting") return RectificationTypes::Stage::TP1_WAITING;
  if (stageStr == "delayed_start") return RectificationTypes::Stage::DELAYED_START;
  if (stageStr == "heads") return RectificationTypes::Stage::HEADS;
  if (stageStr == "late_heads") return RectificationTypes::Stage::LATE_HEADS;
  if (stageStr == "hearts") return RectificationTypes::Stage::HEARTS;
  if (stageStr == "tails") return RectificationTypes::Stage::TAILS;
  if (stageStr == "settings") return RectificationTypes::Stage::SETTINGS;
  return RectificationTypes::Stage::PARSE_ERROR;
}

inline std::string stageToString(RectificationTypes::Stage stage) {
  switch (stage) {
    case RectificationTypes::Stage::EMPTY: return "empty";
    case RectificationTypes::Stage::WAITING: return "waiting";
    case RectificationTypes::Stage::TP1_WAITING: return "tp1_waiting";
    case RectificationTypes::Stage::DELAYED_START: return "delayed_start";
    case RectificationTypes::Stage::HEADS: return "heads";
    case RectificationTypes::Stage::LATE_HEADS: return "late_heads";
    case RectificationTypes::Stage::HEARTS: return "hearts";
    case RectificationTypes::Stage::TAILS: return "tails";
    case RectificationTypes::Stage::SETTINGS: return "settings";
    case RectificationTypes::Stage::PARSE_ERROR: return "error";
    default: return "unknown";
  }
}

inline std::string translateStage(const std::string& stageStr) {
  if (stageStr == "waiting") return "Дежурный режим";
  if (stageStr == "tp1_waiting") return "Ожидание нагрева колонны";
  if (stageStr == "delayed_start") return "Отложенный старт";
  if (stageStr == "heads") return "Головы";
  if (stageStr == "late_heads") return "Подголовники";
  if (stageStr == "hearts") return "Тело";
  if (stageStr == "tails") return "Хвосты";
  if (stageStr == "settings") return "<UNK>";
  return stageStr;
}

inline RectificationTypes::Event stringToEvent(const std::string& eventString) {
  if (eventString == "heads_finished") return RectificationTypes::Event::HEADS_FINISHED;
  if (eventString == "hearts_finished") return RectificationTypes::Event::HEARTS_FINISHED;
  if (eventString == "tails_finished") return RectificationTypes::Event::TAILS_FINISHED;
  if (eventString == "ds_error_stop") return RectificationTypes::Event::DS_ERROR_STOP;
  if (eventString == "stabilization_limit") return RectificationTypes::Event::STABILIZATION_LIMIT;
  if (eventString == "remote_stop") return RectificationTypes::Event::REMOTE_STOP;
  if (eventString == "manually_closed") return RectificationTypes::Event::MANUALLY_CLOSED;
  if (eventString == "manually_opened") return RectificationTypes::Event::MANUALLY_OPENED;
  if (eventString == "ds_error") return RectificationTypes::Event::DS_ERROR;
  return RectificationTypes::Event::PARSE_ERROR;
}

inline std::string eventToString(RectificationTypes::Event event) {
  switch (event) {
    case RectificationTypes::Event::HEADS_FINISHED: return "heads_finished";
    case RectificationTypes::Event::HEARTS_FINISHED: return "hearts_finished";
    case RectificationTypes::Event::TAILS_FINISHED: return "tails_finished";
    case RectificationTypes::Event::DS_ERROR: return "ds_error";
    case RectificationTypes::Event::DS_ERROR_STOP: return "ds_error_stop";
    case RectificationTypes::Event::STABILIZATION_LIMIT: return "stabilization_limit";
    case RectificationTypes::Event::REMOTE_STOP: return "remote_stop";
    case RectificationTypes::Event::MANUALLY_CLOSED: return "manually_closed";
    case RectificationTypes::Event::MANUALLY_OPENED: return "manually_opened";
    default: return "unknown_event";
  }
}

inline std::string processStateToString(RectificationTypes::ProcessState state) {
  switch (state) {
    case RectificationTypes::ProcessState::RUNNING: return "running";
    case RectificationTypes::ProcessState::PAUSED: return "paused";
    case RectificationTypes::ProcessState::FINISHED: return "finished";
    case RectificationTypes::ProcessState::SKIPPED: return "skipped";
    case RectificationTypes::ProcessState::IDLE: return "idle";
    case RectificationTypes::ProcessState::PROCESS_ERROR: return "error";
    default: return "unknown";
  }
}

inline std::string eventToDescription(RectificationTypes::Event event) {
  switch (event) {
    case RectificationTypes::Event::HEADS_FINISHED: return "Завершен этап отбора голов";
    case RectificationTypes::Event::HEARTS_FINISHED: return "Завершен этап отбора тела";
    case RectificationTypes::Event::TAILS_FINISHED: return "Завершен этап отбора хвостов";
    case RectificationTypes::Event::DS_ERROR: return "Ошибка датчика температуры";
    case RectificationTypes::Event::DS_ERROR_STOP: return "Выключение оборудования (реле) из-за ошибки датчика. Срабатывает через 180 секунд, если ошибка текущего датчика не исчезнет.";
    case RectificationTypes::Event::STABILIZATION_LIMIT: return "Превышен лимит времени стабилизации";
    case RectificationTypes::Event::REMOTE_STOP: return "Получена удаленная команда остановки, процесс остановлен";
    case RectificationTypes::Event::MANUALLY_CLOSED: return "Включено ручное управление клапаном текущего этапа, клапан закрыт";
    case RectificationTypes::Event::MANUALLY_OPENED: return "Включено ручное управление клапаном текущего этапа, клапан открыт";
    default: return "Неизвестное событие";
  }
}

}  // namespace RectificationParseUtils

#endif  // RECTIFICATION_PARSE_UTILS_H
