#include "HttpRequestHandler.h"

HttpRequestHandler::HttpRequestHandler(PsychicHttpServer& server,
                                     SecurityManager* securityManager,
                                     ProfileService* profileService,
                                     FS* fs)
        : _server(server),
        _securityManager(securityManager),
        _profileService(profileService),
        _fs(fs),
        _settingsHandler(),
        _commandHandler(),
        _sensorHandler(),
        _telegramBotHandler(),
        _subsystemHandler(),
        _openConnectHandler(),
        _profileHandler(profileService),
        _fileHandler(fs),
        _handlerRegistrar(server,
                        securityManager,
                        _settingsHandler,
                        _commandHandler,
                        _sensorHandler,
                        _telegramBotHandler,
                        _subsystemHandler,
                        _openConnectHandler,
                        _profileHandler,
                        _fileHandler)
{

}

void HttpRequestHandler::begin() const
{
    Serial.println("Initializing HTTP request handlers");
    _handlerRegistrar.registerAllHandlers();
}
