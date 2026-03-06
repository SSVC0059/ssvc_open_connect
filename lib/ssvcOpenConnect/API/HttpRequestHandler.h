#ifndef SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
#define SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H

#include "HandlerRegistrator.h"
#include "PsychicHttp.h"
#include "SecurityManager.h"
#include "handlers/OpenConnectHandler/OpenConnectHandler.h"
#include "handlers/ProfileHandler/ProfileHandler.h"
#include "handlers/FileHandler/FileHandler.h"
#include "core/profiles/ProfileService.h"

class HttpRequestHandler {
public:
  HttpRequestHandler(PsychicHttpServer& server,
                    SecurityManager* securityManager,
                    ProfileService* profileService,
                    FS* fs);
  void begin() const;

private:
    PsychicHttpServer& _server;
    SecurityManager* _securityManager;
    ProfileService* _profileService;
    FS* _fs;

    SettingsHandler _settingsHandler;
    CommandHandler _commandHandler;
    SensorHandler _sensorHandler;
    TelegramBotHandler _telegramBotHandler;
    SubsystemHandler _subsystemHandler;
    OpenConnectHandler _openConnectHandler;
    ProfileHandler _profileHandler;
    FileHandler _fileHandler;

    HandlerRegistrator _handlerRegistrar;
};

#endif
