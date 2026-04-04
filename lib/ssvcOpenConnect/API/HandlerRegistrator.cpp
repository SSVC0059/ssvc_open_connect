#include "HandlerRegistrator.h"
#include <AsyncJson.h>

#define TAG "HandlerRegistrar"

HandlerRegistrator::HandlerRegistrator(AsyncWebServer& server,
                                 SecurityManager* securityManager,
                                 SettingsHandler& settingsHandler,
                                 CommandHandler& commandHandler,
                                 SensorHandler& sensorHandler,
                                 TelegramBotHandler& telegramBot,
                                 SubsystemHandler& subsystemHandler,
                                 OpenConnectHandler& openConnectHandler,
                                 ProfileHandler& profileHandler,
                                 FileHandler& fileHandler)
    : _server(server),
      _securityManager(securityManager),
      _settingsHandler(settingsHandler),
      _commandHandler(commandHandler),
      _sensorHandler(sensorHandler),
      _telegramBot(telegramBot),
      _subsystemHandler(subsystemHandler),
      _openConnectHandler(openConnectHandler),
      _profileHandler(profileHandler),
      _fileHandler(fileHandler)
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
    registerProfileHandler();
    registerFileHandler();

    ESP_LOGI(TAG, "All HTTP handlers registered successfully");
}

void HandlerRegistrator::registerSettingsHandlers() const
{
    _server.on("/rest/settings", HTTP_PUT,
              _securityManager->wrapCallback(
                  [](AsyncWebServerRequest* request, JsonVariant& json) {
                      SettingsHandler::updateSettings(request, json);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/settings", HTTP_GET,
      _securityManager->wrapRequest(
          [](AsyncWebServerRequest* request) {
              SettingsHandler::getSettings(request);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerCommandHandlers() const
{
    _server.on("/rest/commands", HTTP_POST,
              _securityManager->wrapCallback(
                  [](AsyncWebServerRequest* request, JsonVariant& json) {
                      CommandHandler::handleCommand(request, json);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerSensorHandlers() const
{
    _server.on("/rest/sensors/zone", HTTP_PUT,
              _securityManager->wrapRequest(
                  [](AsyncWebServerRequest* request) {
                      SensorHandler::updateSensorZone(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}


void HandlerRegistrator::registerTelegramBot() const
{

    _server.on("/rest/telegram/config", HTTP_PUT,
      _securityManager->wrapCallback(
          [](AsyncWebServerRequest* request, JsonVariant& json) {
              TelegramBotHandler::updateSettings(request, json);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));


    _server.on("/rest/telegram/config", HTTP_GET,
    _securityManager->wrapRequest(
      [](AsyncWebServerRequest* request) {
          TelegramBotHandler::getSettings(request);
      },
      AuthenticationPredicates::IS_AUTHENTICATED));

}

void HandlerRegistrator::registerSubsystemHandler() const
{
    _server.on("/rest/subsystem", HTTP_GET,
          _securityManager->wrapRequest(
              [](AsyncWebServerRequest* request) {
                  SubsystemHandler::getStatus(request);
              },
              AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/subsystem", HTTP_PUT,
      _securityManager->wrapCallback(
          [](AsyncWebServerRequest* request, JsonVariant& json) {
             SubsystemHandler::state(request, json);
          },
          AuthenticationPredicates::IS_AUTHENTICATED
      )
    );
}

void HandlerRegistrator::registerTelegramBotHandler() const
{
    _server.on("/rest/oc/info", HTTP_GET,
              _securityManager->wrapRequest(
                  [](AsyncWebServerRequest* request) {
                      OpenConnectHandler::getInfo(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

}

void HandlerRegistrator::registerProfileHandler() const
{
    // GET /rest/profiles - Get list of all profiles (metadata)
    _server.on("/rest/profiles", HTTP_GET,
                _securityManager->wrapRequest(
                        [](AsyncWebServerRequest* request) {
                            ProfileHandler::handleGetProfiles(request);
                        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // GET /rest/profiles/active - Get the ID of the active profile
    _server.on("/rest/profiles/active", HTTP_GET,
                _securityManager->wrapRequest([](AsyncWebServerRequest* request) {
                    ProfileHandler::handleGetActiveProfile(request);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // GET /rest/profiles/content - Get full content of a specific profile (ID in query param)
    _server.on("/rest/profiles/content", HTTP_GET,
                _securityManager->wrapRequest([](AsyncWebServerRequest* request) {
                    ProfileHandler::handleGetProfileContent(request);
                }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles - Create a profile from current settings
    _server.on("/rest/profiles", HTTP_POST,
                _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
                    ProfileHandler::handleCreateProfile(request, json);
                }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/copy - Copy a profile (source ID and new name in body)
    _server.on("/rest/profiles/copy", HTTP_POST,
                _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
                    ProfileHandler::handleCopyProfile(request, json);
                }, AuthenticationPredicates::IS_AUTHENTICATED));

    // PUT /rest/profiles/meta - Update profile metadata (e.g., name) (ID and new name in body)
    _server.on("/rest/profiles/meta", HTTP_PUT,
        _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
            ProfileHandler::handleUpdateProfileMeta(request, json);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/set-active - Set a profile as active and apply it (ID in body)
    _server.on("/rest/profiles/set-active", HTTP_POST,
        _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
            ProfileHandler::handleSetActiveAndApplyProfile(request, json);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/save - Save current settings to a profile (ID in body)
    _server.on("/rest/profiles/save", HTTP_POST,
        _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
            ProfileHandler::handleSaveSettingsToProfile(request, json);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // DELETE /rest/profiles/delete - Delete a profile (ID in body)
    _server.on("/rest/profiles/delete", HTTP_DELETE,
            _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
                ProfileHandler::handleDeleteProfile(request, json);
            }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/content - Update profile content
    _server.on("/rest/profiles/content", HTTP_POST,
            _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
                ProfileHandler::handleUpdateProfileContent(request, json);
            }, AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerFileHandler() const
{
    _fileHandler.registerHandlers(_server, _securityManager);
}
