#ifndef SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
#define SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H

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

#include "ESP32SvelteKit.h"
#include "EventSocket.h"
#include "SecurityManager.h"

#include "core/SsvcConnector.h"

#include "core/SsvcSettings/SsvcSettings.h"
#include "core/StatefulServices/SensorConfigService/SensorConfigService.h"
#include "core/SubsystemManager/SubsystemManager.h"

#include "API/HttpRequestHandler.h"
#include "rectification/RectificationProcess.h"

#include "components/sensors/SensorManager/SensorManager.h"
#include "components/subsystem/ThermalSubsystem.h"
#include "components/subsystem/SettingsSubsystem.h"
#include "commons/commons.h"
#include "MqttCommandHandler/MqttCommandHandler.h"
#include "StatefulServices/SensorDataService/SensorDataService.h"
#include "StatefulServices/TelemetryService/TelemetryService.h"

/**
 * @def TASK_AT_COMMAND_SEND_STACK_PERIOD
 * @brief Период отправки AT-команд в секундах
 */
#define TASK_AT_COMMAND_SEND_STACK_PERIOD 60
#define SENSOR_POLL_INTERVAL_MS 10000


class NotificationSubscriber;

class SsvcOpenConnect
{
public:
  static SsvcOpenConnect& getInstance() {
    static SsvcOpenConnect instance;
    return instance;
  }
  static void subsystemManager();
  void begin(PsychicHttpServer& server,
             ESP32SvelteKit& esp32sveltekit,
             EventSocket* socket,
             SecurityManager* securityManager);

  ESP32SvelteKit* getESP32SvelteKit() const {
    return _esp32sveltekit;
  }

  SensorConfigService* getSensorConfigService() const {
    return _sensorConfigService;
  }
  SsvcOpenConnect(const SsvcOpenConnect&) = delete;
  void operator=(const SsvcOpenConnect&) = delete;

  bool isOnline() const;

private:
  SsvcOpenConnect() = default;

  PsychicHttpServer* _server = nullptr;
  ESP32SvelteKit* _esp32sveltekit = nullptr;
  EventSocket* _socket = nullptr;
  SecurityManager* _securityManager = nullptr;
  PsychicMqttClient *_mqttClient;

  SsvcConnector& _ssvcConnector = SsvcConnector::getConnector();

  NotificationSubscriber* _notificationSubscriber = nullptr;

  SensorDataService* _sensorDataService = nullptr;
  OpenConnectSettingsService* _openConnectSettingsService = nullptr;
  AlarmThresholdService* _alarmThresholdService = nullptr;
  SensorConfigService* _sensorConfigService = nullptr;
  TelemetryService* _telemetryService = nullptr;

  MqttCommandHandler* _mqttCommandHandler = nullptr;

  AlarmMonitor* _alarmMonitor = nullptr;

  SsvcSettings& _ssvcSettings = SsvcSettings::init();
  RectificationProcess& rProcess = RectificationProcess::rectController();
  SensorManager& _sensorManager = SensorManager::getInstance();

  std::unique_ptr<HttpRequestHandler> httpRequestHandler = nullptr;

  static constexpr auto TAG = "SsvcOpenConnect";

};

#endif // SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H