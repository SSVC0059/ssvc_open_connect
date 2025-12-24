#ifndef HANDLER_REGISTRAR_H
#define HANDLER_REGISTRAR_H

#include "PsychicHttp.h"
#include "SecurityManager.h"

#include "handlers/SettingsHandler/SettingsHandler.h"
#include "handlers/CommandHandler/CommandHandler.h"
#include "handlers/SensorHandler/SensorHandler.h"
#include "handlers/OpenConnectHandler/OpenConnectHandler.h"
#include "handlers/SubsystemHandler/SubsystemHandler.h"
#include "handlers/TelegramBot/TelegramBotHandler.h"
#include "handlers/ProfileHandler/ProfileHandler.h"
#include "handlers/FileHandler/FileHandler.h"

class HandlerRegistrator {
public:
    HandlerRegistrator(PsychicHttpServer& server,
                    SecurityManager* securityManager,
                    SettingsHandler& settingsHandler,
                    CommandHandler& commandHandler,
                    SensorHandler& sensorHandler,
                    TelegramBotHandler& telegramBot,
                    SubsystemHandler& subsystemHandler,
                    OpenConnectHandler& openConnectHandler,
                    ProfileHandler& profileHandler,
                    FileHandler& fileHandler);

    void registerAllHandlers() const;

private:
    PsychicHttpServer& _server;
    SecurityManager* _securityManager;

    SettingsHandler& _settingsHandler;
    CommandHandler& _commandHandler;
    SensorHandler& _sensorHandler;
    TelegramBotHandler& _telegramBot;
    SubsystemHandler& _subsystemHandler;
    OpenConnectHandler& _openConnectHandler;
    ProfileHandler& _profileHandler;
    FileHandler& _fileHandler;

    void registerSettingsHandlers() const;
    void registerCommandHandlers() const;
    void registerSensorHandlers() const;
    void registerTelegramBot() const;
    void registerSubsystemHandler() const;
    void registerTelegramBotHandler() const;
    void registerProfileHandler() const;
    void registerFileHandler() const;
};

#endif
