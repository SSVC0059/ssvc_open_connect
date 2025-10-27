#ifndef HANDLER_REGISTRAR_H
#define HANDLER_REGISTRAR_H

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

#include "PsychicHttp.h"
#include "SecurityManager.h"

#include "handlers/SettingsHandler/SettingsHandler.h"
#include "handlers/CommandHandler/CommandHandler.h"
#include "handlers/SensorHandler/SensorHandler.h"
#include "handlers/OpenConnectHandler/OpenConnectHandler.h"
#include "handlers/SubsystemHandler/SubsystemHandler.h"
#include "handlers/TelegramBot/TelegramBotHandler.h"

class HandlerRegistrator {
public:
    HandlerRegistrator(PsychicHttpServer& server,
                    SecurityManager* securityManager,
                    SettingsHandler& settingsHandler,
                    CommandHandler& commandHandler,
                    SensorHandler& sensorHandler,
                    TelegramBotHandler& telegramBot,
                    SubsystemHandler& subsystemHandler,
                    OpenConnectHandler& openConnectHandler);

    void registerAllHandlers() const;

private:
    PsychicHttpServer& _server;
    SecurityManager* _securityManager;

    // Ссылки на обработчики
    SettingsHandler& _settingsHandler;
    CommandHandler& _commandHandler;
    SensorHandler& _sensorHandler;
    TelegramBotHandler& _telegramBot;
    SubsystemHandler& _subsystemHandler;
    OpenConnectHandler& _openConnectHandler;

    void registerSettingsHandlers() const;
    void registerCommandHandlers() const;
    void registerSensorHandlers() const;
    void registerTelegramBot() const;
    void registerSubsystemHandler() const;
    void registerTelegramBotHandler() const;
};

#endif