//
// Created by demoncat on 05.05.25.
//

#ifndef HANDLER_REGISTRAR_H
#define HANDLER_REGISTRAR_H

#include "PsychicHttp.h"
#include "SecurityManager.h"

#include "handlers/TelemetryHandler/TelemetryHandler.h"
#include "handlers/SettingsHandler/SettingsHandler.h"
#include "handlers/CommandHandler/CommandHandler.h"
#include "handlers/SensorHandler/SensorHandler.h"
#include "handlers/MetricsHandler/MetricsHandler.h"
#include "handlers/OpenConnectHandler/OpenConnectHandler.h"
#include "handlers/SubsystemHandler/SubsystemHandler.h"
#include "handlers/TelegramBot/TelegramBotHandler.h"

class HandlerRegistrator {
public:
    HandlerRegistrator(PsychicHttpServer& server,
                    SecurityManager* securityManager,
                    TelemetryHandler& telemetryHandler,
                    SettingsHandler& settingsHandler,
                    CommandHandler& commandHandler,
                    SensorHandler& sensorHandler,
                    MetricsHandler& metricsHandler,
                    TelegramBotHandler& telegramBot,
                    SubsystemHandler& subsystemHandler,
                    OpenConnectHandler& openConnectHandler);

    void registerAllHandlers() const;

private:
    PsychicHttpServer& _server;
    SecurityManager* _securityManager;

    // Ссылки на обработчики
    TelemetryHandler& _telemetryHandler;
    SettingsHandler& _settingsHandler;
    CommandHandler& _commandHandler;
    SensorHandler& _sensorHandler;
    MetricsHandler& _metricsHandler;
    TelegramBotHandler& _telegramBot;
    SubsystemHandler& _subsystemHandler;
    OpenConnectHandler& _openConnectHandler;

    void registerTelemetryHandlers() const;
    void registerSettingsHandlers() const;
    void registerCommandHandlers() const;
    void registerSensorHandlers() const;
    void registerTelegramBot() const;
    void registerSubsystemHandler() const;
    void registerTelegramBotHandler() const;
};

#endif