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
#include "core/SsvcCommandsQueue.h"
#include "core/SsvcConnector.h"
#include "core/SsvcSettings/SsvcSettings.h"
#include <Arduino.h>

#include "core/StatefulServices/OpenConnectSettingsService/ssvcMqttSettings.h"
#include "commons/JsonSpiRamAllocator.h"
#include "core/rectification/RectificationTypes.h"
#include "core/rectification/RectificationParseUtils.h"

#define TEMP_GRAPH_ARRAY_SIZE 720
#define PERIOD_GRAPH_SEC 20

extern portMUX_TYPE ssvcMux;

class RectificationProcess
{
public:

  using ProcessState = RectificationTypes::ProcessState;
  using RectificationEvent = RectificationTypes::Event;
  using RectificationStage = RectificationTypes::Stage;

  struct Common
  {
    float mmhg;
    float tp1;
    float tp2;
    bool relay;
    bool signal;
    float relative_pressure;
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
    float tank_mmhg;
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

  //
  // Удаляем конструкторы копирования и присваивания
  RectificationProcess(const RectificationProcess&) = delete;
  RectificationProcess& operator=(const RectificationProcess&) = delete;

  static RectificationProcess& rectController() {
    static RectificationProcess instance;
    return instance;
  }


  void begin(SsvcConnector& connector, SsvcSettings& settings,
                 SsvcMqttSettingsService& ssvcMqttSettingsService);


  void writeTelemetryTo(JsonVariant telemetry);
  bool getStatus(JsonVariant status);

  static std::string translateRectificationStage(const std::string& stageStr);

  Metrics& getMetrics();

  RectificationTypes::ProcessState getProcessState() const { return currentProcessStatus; }
  const char* getRectificationStartTime() const { return startTime; }
  const char* getRectificationEndTime() const { return endTime; }

  /** Collected volume (ml) for a finished rectification stage; 0 if not recorded. */
  int getFlowVolumeCollectedMl(RectificationStage stage) const;

  std::string errorSet;

private:
  // Приватный конструктор
  RectificationProcess();

  float initial_pressure;
  bool initial_pressure_set;

  static void update(void* pvParameters);
  [[noreturn]] static void pressureSenderTask(void* pvParameters);

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
  SsvcMqttSettingsService* _ssvcMqttSettingsService;
  static RectificationProcess* _rectificationProcess;
  JsonSpiRamAllocator _spiRamAllocator;

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

  static constexpr auto TAG = "RectificationProcess";
};

#endif // RECTIFICATION_PROCESS_H
