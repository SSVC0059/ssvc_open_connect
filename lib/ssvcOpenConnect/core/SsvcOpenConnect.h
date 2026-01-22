#ifndef SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
#define SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H

#include <memory>
#include "ESP32SvelteKit.h"
#include "EventSocket.h"
#include "SecurityManager.h"
#include "core/SsvcConnector.h"
#include "core/SsvcSettings/SsvcSettings.h"
#include "core/StatefulServices/SensorConfigService/SensorConfigService.h"
#include "core/SubsystemManager/SubsystemManager.h"
#include "core/profiles/ProfileService.h"
#include "API/HttpRequestHandler.h"
#include "rectification/RectificationProcess.h"
#include "components/sensors/SensorManager/SensorManager.h"
#include "components/subsystem/ThermalSubsystem.h"
#include "components/subsystem/SettingsSubsystem.h"
#include "commons/commons.h"
#include "MqttCommandHandler/MqttCommandHandler.h"
#include "StatefulServices/SensorDataService/SensorDataService.h"
#include "StatefulServices/TelemetryService/TelemetryService.h"
#include "StatefulServices/TelegramSettingsService/TelegramSettingsService.h"
#include "components/Led/StatusLed.h"

#define TASK_AT_COMMAND_SEND_STACK_PERIOD 60
#define SENSOR_POLL_INTERVAL_MS 10000

class NotificationSubscriber;

class SsvcOpenConnect
{
public:
  static SsvcOpenConnect& getInstance();

  void subsystemManager();
  void begin(PsychicHttpServer& server, ESP32SvelteKit& esp32sveltekit, EventSocket* socket, SecurityManager* securityManager);

  ESP32SvelteKit* getESP32SvelteKit() const { return _esp32sveltekit; }
  SensorConfigService* getSensorConfigService() const { return _sensorConfigService; }
  TelegramSettingsService* getTelegramSettingsService() const { return _telegramSettingsService; }

  SsvcOpenConnect(const SsvcOpenConnect&) = delete;
  void operator=(const SsvcOpenConnect&) = delete;

  bool isOnline() const;
  static void sendHello();

private:
  SsvcOpenConnect() = default;

  PsychicHttpServer* _server = nullptr;
  ESP32SvelteKit* _esp32sveltekit = nullptr;
  EventSocket* _socket = nullptr;
  SecurityManager* _securityManager = nullptr;
  PsychicMqttClient *_mqttClient  = nullptr;

  SsvcConnector& _ssvcConnector = SsvcConnector::getConnector();
  NotificationSubscriber* _notificationSubscriber = nullptr;

  SensorDataService* _sensorDataService = nullptr;
  SsvcMqttSettingsService* _ssvcMqttSettingsService = nullptr;
  AlarmThresholdService* _alarmThresholdService = nullptr;
  SensorConfigService* _sensorConfigService = nullptr;
  TelemetryService* _telemetryService = nullptr;
  ProfileService* _profileService = nullptr;
  TelegramSettingsService* _telegramSettingsService = nullptr;

  MqttCommandHandler* _mqttCommandHandler = nullptr;
  AlarmMonitor* _alarmMonitor = nullptr;

  SsvcSettings& _ssvcSettings = SsvcSettings::init();
  RectificationProcess& rProcess = RectificationProcess::rectController();
  SensorManager& _sensorManager = SensorManager::getInstance();

  std::unique_ptr<HttpRequestHandler> httpRequestHandler = nullptr;
  std::unique_ptr<StatusLed> _statusLed;

  static constexpr auto TAG = "SsvcOpenConnect";
};

#endif // SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
