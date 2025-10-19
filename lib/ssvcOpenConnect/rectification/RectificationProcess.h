#ifndef RECTIFICATION_PROCESS_H
#define RECTIFICATION_PROCESS_H

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

#include "map"
#include "ArduinoJson.h"
#include "core/StatefulServices/OpenConnectSettings/OpenConnectSettingsManager.h"
#include "core/SsvcCommandsQueue.h"
#include "core/SsvcConnector.h"
#include "core/SsvcSettings/SsvcSettings.h"
#include <Arduino.h>

#define TEMP_GRAPH_ARRAY_SIZE 720
#define PERIOD_GRAPH_SEC 20

static auto TAG = "RectificationProcess";

extern portMUX_TYPE ssvcMux;

class RectificationProcess
{
public:


  enum class ProcessState
  {
    IDLE, // Процесс еще не запускался
    RUNNING, // Процесс запущен
    PAUSED,
    FINISHED, // Процесс завершен
    SKIPPED,
    STOPPED,
    ERROR
  };

  struct Common
  {
    unsigned int mmhg;
    float tp1;
    float tp2;
    bool relay;
    bool signal;
  };

  enum class RectificationEvent
  {
    EMPTY,
    HEADS_FINISHED, // завершен этап Головы
    HEARTS_FINISHED, // завершен этап Тело (ректификация завершена для firmware 2.3.*)
    TAILS_FINISHED, // завершен этап Хвосты (ректификация завершена для firmware 2.2.*)
    DS_ERROR, // ошибка датчика температуры
    DS_ERROR_STOP, // выключение оборудования (реле) из-за ошибки датчика
    STABILIZATION_LIMIT, // превышен лимит времени стабилизации
    REMOTE_STOP, // получена удаленная команда остановки, процесс остановлен
    MANUALLY_CLOSED, // включено ручное управление клапаном текущего этапа, клапан закрыт
    MANUALLY_OPENED, // включено ручное управление клапаном текущего этапа, клапан открыт
    ERROR
  };

  struct Metrics
  {
    std::string type;
    float tp1_target;
    float tp2_target;
    std::string countdown;
    Common common;
    std::string release;
    std::string time;
    float open;
    unsigned int period;
    unsigned int tank_mmhg;
    float tp1_sap;
    float tp2_sap;
    float hysteresis;
    unsigned int v1;
    unsigned int v2;
    unsigned int v3;
    float alc;
    bool stop;
    unsigned char stops;
    RectificationEvent event;
  };

  enum class RectificationStage
  {
    EMPTY,
    WAITING, // Дежурный режим
    TP1_WAITING, // Ожидание нагрева колонны
    DELAYED_START, // Отложенный старт
    HEADS, // Отбор голов
    LATE_HEADS, // Отбор подголовников
    HEARTS, // Отбор тела
    TAILS, // Отбор хвостов
    SETTINGS, // Открыты настройки на контроллере SSVC
    ERROR // Ошибка разбора этапов
  };

  //
  // Удаляем конструкторы копирования и присваивания
  RectificationProcess(const RectificationProcess&) = delete;
  RectificationProcess& operator=(const RectificationProcess&) = delete;

  static RectificationProcess& rectController() {
    static RectificationProcess instance;
    return instance;
  }


  void begin(SsvcConnector& connector, SsvcSettings& settings,
                 OpenConnectSettingsService& openConnectSettingsService);


  std::string getTelemetry();
  bool getStatus(JsonVariant temperBuffer);
  static std::string translateRectificationStage(const std::string& stageStr);

  Metrics& getMetrics();

  std::string errorSet;

private:
  // Приватный конструктор
  RectificationProcess();

  static void update(void* pvParameters);

  int pid;
  boolean pidChecked = false; // флаг проверки PID

  RectificationStage currentStage; // Предыдущий этап
  RectificationStage previousStage; // текущий этап
  //    Хранение результатов отбора клапанами на каждом их этапов
  std::map<RectificationStage, int> flowVolumeValves;

  // Хранение результатов прохождения этапов ректификации
  std::map<RectificationStage, ProcessState> rectificationStageStates;

  ProcessState currentProcessStatus;

  char startTime[25]{};
  char endTime[25]{};

  Metrics metric;

  SsvcConnector* _ssvcConnector;
  SsvcSettings* _ssvcSettings;
  OpenConnectSettingsService* _openConnectSettingsService;
  static RectificationProcess* _rectificationProcess;

  static RectificationStage
  stringToRectificationStage(const std::string& stageStr);
  static RectificationEvent
  stringToRectificationEvent(std::string& eventString);
  static std::string rectificationEventToString(RectificationEvent event);
  static std::string rectificationEventToDescription(RectificationEvent event);
  static std::string stageToString(RectificationStage stage);

  static std::string stateToString(ProcessState state);

  bool eventReceived = false;

  void startEventHandler(const std::string&);

  void endEventHandler(const std::string& currentEvent);

  bool isHeatingOn() const;

  bool isOverclockingOn() const;

  void recalculateFlowVolume(int v1, int v2, int v3);

  bool calculateVolumeSpeed(int valveOpen, int& volumeSpeed) const;

  static bool isNonEmptyString(const char* str);

};

#endif // RECTIFICATION_PROCESS_H
