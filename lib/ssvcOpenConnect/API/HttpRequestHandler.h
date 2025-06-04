#ifndef SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
#define SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H

#include "HandlerRegistrator.h"
#include "PsychicHttp.h"
#include "SecurityManager.h"

class HttpRequestHandler {
public:
  HttpRequestHandler(PsychicHttpServer& server,
                    SecurityManager* securityManager,
                    RectificationProcess& rProcess);
  void begin() const;

private:
  PsychicHttpServer& _server;
  SecurityManager* _securityManager;

  // Обработчики
  TelemetryHandler _telemetryHandler;
  SettingsHandler _settingsHandler;
  CommandHandler _commandHandler;
  SensorHandler _sensorHandler;
  MetricsHandler _metricsHandler;
  TelegramBotHandler _telegramBotHandler;
  SubsystemHandler _subsystemHandler;

  HandlerRegistrator _handlerRegistrar;
};

#endif