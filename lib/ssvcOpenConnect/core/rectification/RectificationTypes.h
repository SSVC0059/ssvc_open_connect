/**
 *   SSVC Open Connect
 *
 *   Типы этапов, событий и состояний ректификации.
 *   Отдельный header без Arduino/ESP32 для тестирования на host.
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 **/

#ifndef RECTIFICATION_TYPES_H
#define RECTIFICATION_TYPES_H

namespace RectificationTypes {

enum class Stage {
  EMPTY,
  WAITING,
  TP1_WAITING,
  DELAYED_START,
  HEADS,
  LATE_HEADS,
  HEARTS,
  TAILS,
  SETTINGS,
  PARSE_ERROR  // ERROR conflicts with Windows macro
};

enum class Event {
  EMPTY,
  HEADS_FINISHED,
  HEARTS_FINISHED,
  TAILS_FINISHED,
  DS_ERROR,
  DS_ERROR_STOP,
  STABILIZATION_LIMIT,
  REMOTE_STOP,
  MANUALLY_CLOSED,
  MANUALLY_OPENED,
  PARSE_ERROR  // ERROR conflicts with Windows macro
};

enum class ProcessState {
  IDLE,
  RUNNING,
  PAUSED,
  FINISHED,
  SKIPPED,
  STOPPED,
  PROCESS_ERROR  // ERROR conflicts with Windows macro
};

}  // namespace RectificationTypes

#endif  // RECTIFICATION_TYPES_H
