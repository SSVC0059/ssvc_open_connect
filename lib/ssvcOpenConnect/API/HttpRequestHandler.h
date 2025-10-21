#ifndef SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
#define SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H

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

#include "HandlerRegistrator.h"
#include "PsychicHttp.h"
#include "SecurityManager.h"
#include "handlers/OpenConnectHandler/OpenConnectHandler.h"

class HttpRequestHandler {
public:
  HttpRequestHandler(PsychicHttpServer& server,
                    SecurityManager* securityManager);
  void begin() const;

private:
    PsychicHttpServer& _server;
    SecurityManager* _securityManager;

    // Обработчики
    SettingsHandler _settingsHandler;
    CommandHandler _commandHandler;
    SensorHandler _sensorHandler;
    TelegramBotHandler _telegramBotHandler;
    SubsystemHandler _subsystemHandler;
    OpenConnectHandler _openConnectHandler;

    HandlerRegistrator _handlerRegistrar;
};

#endif