#include "HandlerRegistrator.h"
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

#define TAG "HandlerRegistrar"

HandlerRegistrator::HandlerRegistrator(PsychicHttpServer& server,
                                 SecurityManager* securityManager,
                                 SettingsHandler& settingsHandler,
                                 CommandHandler& commandHandler,
                                 SensorHandler& sensorHandler,
                                 TelegramBotHandler& telegramBot,
                                 SubsystemHandler& subsystemHandler,
                                 OpenConnectHandler& openConnectHandler)
    : _server(server),
      _securityManager(securityManager),
      _settingsHandler(settingsHandler),
      _commandHandler(commandHandler),
      _sensorHandler(sensorHandler),
      _telegramBot(telegramBot),
      _subsystemHandler(subsystemHandler),
      _openConnectHandler(openConnectHandler)
{}

void HandlerRegistrator::registerAllHandlers() const
{
    ESP_LOGI(TAG, "Registering all HTTP handlers");

    registerSettingsHandlers();
    registerCommandHandlers();
    registerSensorHandlers();
    registerTelegramBot();
    registerSubsystemHandler();
    registerTelegramBotHandler();

    ESP_LOGI(TAG, "All HTTP handlers registered successfully");
}

void HandlerRegistrator::registerSettingsHandlers() const
{
    _server.on("/rest/settings", HTTP_GET,
              _securityManager->wrapRequest(
                  [](PsychicRequest* request) {
                      return SettingsHandler::getSettings(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/settings", HTTP_PUT,
              _securityManager->wrapRequest(
                  [](PsychicRequest* request) {
                      return SettingsHandler::updateSettings(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerCommandHandlers() const
{
    _server.on("/rest/commands", HTTP_POST,
              _securityManager->wrapRequest(
                  [](PsychicRequest* request) {
                      return CommandHandler::handleCommand(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerSensorHandlers() const
{
    // _server.on("/rest/sensors/zone", HTTP_GET,
    //           _securityManager->wrapRequest(
    //               [](PsychicRequest* request) {
    //                   return SensorHandler::getSensorAddresses(request);
    //               },
    //               AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/sensors/zone", HTTP_PUT,
              _securityManager->wrapRequest(
                  [](PsychicRequest* request) {
                      return SensorHandler::updateSensorZone(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}


void HandlerRegistrator::registerTelegramBot() const
{

    _server.on("/rest/telegram/config", HTTP_PUT,
      _securityManager->wrapRequest(
          [](PsychicRequest* request) {
              return TelegramBotHandler::updateSettings(request);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));


    _server.on("/rest/telegram/config", HTTP_GET,
    _securityManager->wrapRequest(
      [](PsychicRequest* request) {
          return TelegramBotHandler::getSettings(request);
      },
      AuthenticationPredicates::IS_AUTHENTICATED));

}

void HandlerRegistrator::registerSubsystemHandler() const
{
    _server.on("/rest/subsystem", HTTP_GET,
          _securityManager->wrapRequest(
              [](PsychicRequest* request) {
                  return SubsystemHandler::getStatus(request);
              },
              AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/subsystem", HTTP_PUT,
      _securityManager->wrapRequest(
          [](PsychicRequest* request) {
             return SubsystemHandler::state(request);
          },
          AuthenticationPredicates::IS_AUTHENTICATED
      )
    );
}

void HandlerRegistrator::registerTelegramBotHandler() const
{
    _server.on("/rest/oc/info", HTTP_GET,
              _securityManager->wrapRequest(
                  [](PsychicRequest* request) {
                      return OpenConnectHandler::getInfo(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

}
