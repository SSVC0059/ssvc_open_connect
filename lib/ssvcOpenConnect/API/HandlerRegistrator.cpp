//
// Created by demoncat on 05.05.25.
//

#include "HandlerRegistrator.h"
#include "esp_log.h"

#define TAG "HandlerRegistrar"

HandlerRegistrator::HandlerRegistrator(PsychicHttpServer& server,
                                 SecurityManager* securityManager,
                                 TelemetryHandler& telemetryHandler,
                                 SettingsHandler& settingsHandler,
                                 CommandHandler& commandHandler,
                                 SensorHandler& sensorHandler,
                                 MetricsHandler& metricsHandler,
                                 TelegramBotHandler& telegramBot,
                                 SubsystemHandler& subsystemHandler)
    : _server(server),
      _securityManager(securityManager),
      _telemetryHandler(telemetryHandler),
      _settingsHandler(settingsHandler),
      _commandHandler(commandHandler),
      _sensorHandler(sensorHandler),
      _metricsHandler(metricsHandler),
      _telegramBot(telegramBot),
      _subsystemHandler(subsystemHandler)
{}

void HandlerRegistrator::registerAllHandlers() const
{
    ESP_LOGI(TAG, "Registering all HTTP handlers");
    
    registerTelemetryHandlers();
    registerSettingsHandlers();
    registerCommandHandlers();
    registerSensorHandlers();
    // registerMetricsHandlers();
    registerTelegramBot();
    registerSubsystemHandler();

    ESP_LOGI(TAG, "All HTTP handlers registered successfully");
}

void HandlerRegistrator::registerTelemetryHandlers() const
{
    _server.on("/rest/telemetry", HTTP_GET,
              _securityManager->wrapRequest(
                  [this](PsychicRequest* request) {
                      return _telemetryHandler.handleRequest(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerSettingsHandlers() const
{
    _server.on("/rest/settings", HTTP_GET,
              _securityManager->wrapRequest(
                  [this](PsychicRequest* request) {
                      return SettingsHandler::getSettings(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/settings", HTTP_PUT,
              _securityManager->wrapRequest(
                  [this](PsychicRequest* request) {
                      return SettingsHandler::updateSettings(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerCommandHandlers() const
{
    _server.on("/rest/commands", HTTP_POST,
              _securityManager->wrapRequest(
                  [this](PsychicRequest* request) {
                      return CommandHandler::handleCommand(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerSensorHandlers() const
{
    _server.on("/rest/sensors/zone", HTTP_GET,
              _securityManager->wrapRequest(
                  [this](PsychicRequest* request) {
                      return SensorHandler::getSensorAddresses(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/sensors/zone", HTTP_PUT,
              _securityManager->wrapRequest(
                  [this](PsychicRequest* request) {
                      return SensorHandler::updateSensorZone(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/sensors/temperatures", HTTP_GET,
              _securityManager->wrapRequest(
                  [this](PsychicRequest* request) {
                      return SensorHandler::getTemperatures(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}


void HandlerRegistrator::registerTelegramBot() const
{

    _server.on("/rest/telegram/config", HTTP_PUT,
      _securityManager->wrapRequest(
          [this](PsychicRequest* request) {
              return TelegramBotHandler::updateSettings(request);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));


    _server.on("/rest/telegram/config", HTTP_GET,
    _securityManager->wrapRequest(
      [this](PsychicRequest* request) {
          return TelegramBotHandler::getSettings(request);
      },
      AuthenticationPredicates::IS_AUTHENTICATED));

}

void HandlerRegistrator::registerSubsystemHandler() const
{
    _server.on("/rest/subsystem", HTTP_GET,
          _securityManager->wrapRequest(
              [this](PsychicRequest* request) {
                  return SubsystemHandler::getStatus(request);
              },
              AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/subsystem/state", HTTP_PUT,
      _securityManager->wrapRequest(
          [this](PsychicRequest* request) {
             return SubsystemHandler::state(request);
          },
          AuthenticationPredicates::IS_AUTHENTICATED
      )
    );
}