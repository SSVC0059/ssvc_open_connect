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

#include "core/app/ServiceRegistry.h"

#include <Arduino.h>

#include "ESP32SvelteKit.h"

#include "core/StatefulServices/OpenConnectSettingsService/ssvcMqttSettings.h"
#include "core/StatefulServices/TelegramSettingsService/TelegramSettingsService.h"
#if FT_VK_BOT
#include "core/StatefulServices/VkSettingsService/VkSettingsService.h"
#endif
#include "core/StatefulServices/AlarmThresholdService/AlarmThresholdService.h"
#include "core/StatefulServices/SensorDataService/SensorDataService.h"
#include "core/StatefulServices/SensorConfigService/SensorConfigService.h"
#include "core/StatefulServices/TelemetryService/TelemetryService.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareSettingsService.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"

#include "core/profiles/ProfileService.h"
#include "core/SsvcConnector.h"
#include "core/SsvcSettings/SsvcSettings.h"
#include "core/RelayRuleEngine/RelayRuleEngine.h"
#include "core/UserRelayProfileBridge/UserRelayProfileBridge.h"

#if !PINOUT_USE_GPIO
#include "core/RelayPortCoordinator/RelayPortCoordinator.h"
#endif

#include "core/AlarmMonitor/AlarmMonitor.h"
#include "core/AlarmSubscribers/Notification/NotificationSubscriber.h"
#include "core/AlarmSubscribers/HardwareFault/HardwareFaultLogSubscriber.h"
#include "core/AlarmSubscribers/PinOut/PinOutSubscriber.h"

#include "components/sensors/SensorCoordinator/SensorCoordinator.h"
#include "components/sensors/OneWireThermalSubsystem/OneWireThermalSubsystem.h"
#include "core/rectification/RectificationProcess.h"

#define TAG "ServiceRegistry"

// Interval of the central sensor polling loop (ms) — was SENSOR_POLL_INTERVAL_MS macro.
static constexpr uint32_t SENSOR_POLL_INTERVAL_MS = 10000;

ServiceRegistry::ServiceRegistry() = default;
ServiceRegistry::~ServiceRegistry() = default;

void ServiceRegistry::begin(const AppContext& ctx)
{
  ESP_LOGI(TAG, "begin: creating StatefulServices");

  _mqttSettings = std::unique_ptr<SsvcMqttSettingsService>(
      new SsvcMqttSettingsService(ctx.server, ctx.sveltekit));
  _telegramSettings = std::unique_ptr<TelegramSettingsService>(
      new TelegramSettingsService(ctx.server, ctx.sveltekit));
  TelegramSettingsService::setInstance(_telegramSettings.get());
#if FT_VK_BOT
  _vkSettings = std::unique_ptr<VkSettingsService>(
      new VkSettingsService(ctx.server, ctx.sveltekit));
  VkSettingsService::setInstance(_vkSettings.get());
#endif
  _alarmThresholds = std::unique_ptr<AlarmThresholdService>(
      new AlarmThresholdService(ctx.server, ctx.sveltekit));
  _sensorData = std::unique_ptr<SensorDataService>(
      new SensorDataService(ctx.server, ctx.sveltekit));
  SensorDataService::setInstance(_sensorData.get());
  _sensorConfig = std::unique_ptr<SensorConfigService>(
      new SensorConfigService(ctx.server, ctx.sveltekit));

  // Наблюдателей подписываем ДО вызова ProfileService::begin()
  ProfileService* profileService = ProfileService::getInstance();
  SsvcSettings& ssvcSettings = SsvcSettings::init();
  profileService->subscribe(&ssvcSettings);
  profileService->subscribe(_telegramSettings.get());
#if FT_VK_BOT
  profileService->subscribe(_vkSettings.get());
#endif
  profileService->subscribe(&UserRelayProfileBridge::instance());
  profileService->subscribe(_alarmThresholds.get());

  ESP_LOGD(TAG, "begin: ProfileService::begin");
  profileService->begin(ctx.sveltekit->getFS());

  ESP_LOGD(TAG, "begin: OpenConnectHardwareSettingsService");
  OpenConnectHardwareSettingsService::instance().begin(ctx.sveltekit->getFS());

#if !PINOUT_USE_GPIO
  OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
    RelayPortCoordinator::getInstance().configure(cfg.relayPcf8574Addresses);
  });
#endif

  ESP_LOGD(TAG, "begin: Telegram/Alarm/SensorData/SensorConfig begin");
  _telegramSettings->begin();
#if FT_VK_BOT
  _vkSettings->begin();
#endif
  _alarmThresholds->begin();
  _sensorData->begin();
  _sensorConfig->begin();

  ESP_LOGD(TAG, "begin: AlarmMonitor::initialize");
  AlarmMonitor::getInstance().initialize(_alarmThresholds.get());

  RelayRuleEngine::getInstance().begin();
  if (!profileService->applyActiveProfileObservers()) {
    ESP_LOGW(TAG, "applyActiveProfileObservers: skipped or failed (no active profile)");
  }

#if !PINOUT_USE_GPIO
  ctx.sveltekit->getFeatureService()->addFeature(String("openConnectUserRelays"), true);
#endif

  ESP_LOGD(TAG, "begin: SensorCoordinator + OneWire (pin=%u)",
           static_cast<unsigned>(OneWireThermalSubsystem::ONEWIRE_PIN));
  SensorCoordinator::getInstance().registerPollingSubsystem(
      &OneWireThermalSubsystem::getInstance()
  );

  ESP_LOGD(TAG, "begin: SensorCoordinator::startPolling");
  SensorCoordinator::getInstance().startPolling(SENSOR_POLL_INTERVAL_MS);

  _sensorConfig->addUpdateHandler([&](const String& originId) {
    _sensorData->triggerZoneDataRecalculation();
    AlarmMonitor::getInstance().checkAllSensors();
  });

  SensorCoordinator::getInstance().onFirstScanComplete([]() {
    ESP_LOGI(TAG, "First sensor scan complete. Running initial alarm check.");
    AlarmMonitor::getInstance().checkAllSensors();
  });

  ESP_LOGD(TAG, "begin: Notification + hardware-fault log subscribers");
  _notification = std::unique_ptr<NotificationSubscriber>(
      new NotificationSubscriber(ctx.sveltekit));
  _hardwareFaultLog = std::unique_ptr<HardwareFaultLogSubscriber>(
      new HardwareFaultLogSubscriber());

  ESP_LOGD(TAG, "begin: RectificationProcess::begin");
  RectificationProcess::rectController().begin(
      SsvcConnector::getConnector(), ssvcSettings, *_mqttSettings);

  ESP_LOGD(TAG, "begin: TelemetryService");
  _telemetry = std::unique_ptr<TelemetryService>(
      new TelemetryService(ctx.server, ctx.sveltekit, RectificationProcess::rectController()));
  _telemetry->begin();
}

void ServiceRegistry::ensurePinOutSubscriber()
{
  if (_pinOut == nullptr) {
    ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] PinOutSubscriber (I2C bus ready)");
    _pinOut = std::unique_ptr<PinOutSubscriber>(new PinOutSubscriber());
  }
}
