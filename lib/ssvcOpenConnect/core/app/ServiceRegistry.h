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

#ifndef SSVC_OPEN_CONNECT_APP_SERVICEREGISTRY_H
#define SSVC_OPEN_CONNECT_APP_SERVICEREGISTRY_H

#include <memory>

#include "core/app/AppContext.h"

// Forward declarations — full definitions live in ServiceRegistry.cpp only.
class AlarmThresholdService;
class HardwareFaultLogSubscriber;
class NotificationSubscriber;
class PinOutSubscriber;
class SensorConfigService;
class SensorDataService;
class SsvcMqttSettingsService;
class TelemetryService;
class TelegramSettingsService;

#ifndef FT_VK_BOT
#define FT_VK_BOT 0
#endif
#if FT_VK_BOT
class VkSettingsService;
#endif

/**
 * Owns the StatefulServices and alarm subscribers and boots the whole
 * service/sensor/alarm pipeline. The composition root calls begin() once;
 * service instances live as long as this object (unique ownership).
 *
 * The default constructor and destructor are declared here and defined in
 * the .cpp so the unique_ptr members are constructed/torn down where their
 * types are complete.
 */
class ServiceRegistry
{
public:
  ServiceRegistry();
  ~ServiceRegistry();
  ServiceRegistry(const ServiceRegistry&) = delete;
  ServiceRegistry& operator=(const ServiceRegistry&) = delete;

  void begin(const AppContext& ctx);

  SensorConfigService* sensorConfig() const { return _sensorConfig.get(); }

  /** Creates PinOutSubscriber once the I2C bus is ready (called by SubsystemCatalog). */
  void ensurePinOutSubscriber();

private:
  std::unique_ptr<SsvcMqttSettingsService> _mqttSettings;
  std::unique_ptr<TelegramSettingsService> _telegramSettings;
#if FT_VK_BOT
  std::unique_ptr<VkSettingsService> _vkSettings;
#endif
  std::unique_ptr<AlarmThresholdService> _alarmThresholds;
  std::unique_ptr<SensorDataService> _sensorData;
  std::unique_ptr<SensorConfigService> _sensorConfig;
  std::unique_ptr<TelemetryService> _telemetry;
  std::unique_ptr<NotificationSubscriber> _notification;
  std::unique_ptr<HardwareFaultLogSubscriber> _hardwareFaultLog;
  std::unique_ptr<PinOutSubscriber> _pinOut;
};

#endif // SSVC_OPEN_CONNECT_APP_SERVICEREGISTRY_H
