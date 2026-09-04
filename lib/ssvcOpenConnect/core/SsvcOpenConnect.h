#ifndef SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
#define SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H

#include <memory>

#include "core/app/AppContext.h"
#include "core/app/ServiceRegistry.h"
#include "core/app/SubsystemCatalog.h"

// Forward declarations — full definitions live in SsvcOpenConnect.cpp only.
class EventSocket;
class HttpRequestHandler;
class MqttCommandHandler;
class SensorConfigService;
class StatusLed;

/**
 * Composition root / facade of the firmware.
 *
 * begin() only sequences the boot phases and delegates the heavy wiring to
 * ServiceRegistry (services + alarms) and SubsystemCatalog (subsystems).
 * The public API (getInstance, begin, getSensorConfigService) is preserved.
 */
class SsvcOpenConnect
{
public:
  static SsvcOpenConnect& getInstance();

  void begin(AsyncWebServer& server, ESP32SvelteKit& esp32sveltekit,
             EventSocket* socket, SecurityManager* securityManager);

  SensorConfigService* getSensorConfigService() const { return _services.sensorConfig(); }

  SsvcOpenConnect(const SsvcOpenConnect&) = delete;
  void operator=(const SsvcOpenConnect&) = delete;

private:
  SsvcOpenConnect() = default;
  ~SsvcOpenConnect();

  void initPlatform(AsyncWebServer& server, ESP32SvelteKit& esp32sveltekit,
                    EventSocket* socket, SecurityManager* securityManager);
  void initHttp();
  void initMessaging();

  AppContext _ctx;
  ServiceRegistry _services;
  SubsystemCatalog _subsystems;

  std::unique_ptr<HttpRequestHandler> _http;
  std::unique_ptr<StatusLed> _statusLed;
  std::unique_ptr<MqttCommandHandler> _mqttCommands;

  static constexpr auto TAG = "SsvcOpenConnect";
};

#endif // SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
