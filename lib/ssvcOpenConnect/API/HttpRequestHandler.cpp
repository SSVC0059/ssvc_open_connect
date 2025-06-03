#include "HttpRequestHandler.h"

HttpRequestHandler::HttpRequestHandler(PsychicHttpServer& server,
                                     SecurityManager* securityManager,
                                     RectificationProcess& rProcess)
    : _server(server), 
      _securityManager(securityManager),
      _telemetryHandler(rProcess),
      _settingsHandler(),
      _commandHandler(),
      _sensorHandler(),
      _metricsHandler(rProcess),
      _telegramBotHandler(),
      _subsystemHandler(),
      _handlerRegistrar(server,
                        securityManager,
                        _telemetryHandler,
                        _settingsHandler,
                        _commandHandler,
                        _sensorHandler,
                        _metricsHandler,
                        _telegramBotHandler,
                        _subsystemHandler)
{

}

void HttpRequestHandler::begin() const
{
    Serial.println("Initializing HTTP request handlers");
    _handlerRegistrar.registerAllHandlers();
}
