#include "HandlerRegistrator.h"

#define TAG "HandlerRegistrar"

HandlerRegistrator::HandlerRegistrator(PsychicHttpServer& server,
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
              _securityManager->wrapRequest(
                  [](PsychicRequest* request) {
                      return SettingsHandler::updateSettings(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/settings", HTTP_GET,
      _securityManager->wrapRequest(
          [](PsychicRequest* request) {
              return SettingsHandler::getSettings(request);
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

void HandlerRegistrator::registerProfileHandler() const
{
    // GET /rest/profiles - Get list of all profiles (metadata)
    _server.on("/rest/profiles", HTTP_GET,
                _securityManager->wrapRequest(
                        [](PsychicRequest* request) -> esp_err_t {
                            return ProfileHandler::handleGetProfiles(request);
                        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // GET /rest/profiles/active - Get the ID of the active profile
    _server.on("/rest/profiles/active", HTTP_GET,
                _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
                    return ProfileHandler::handleGetActiveProfile(request);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // GET /rest/profiles/content - Get full content of a specific profile (ID in query param)
    _server.on("/rest/profiles/content", HTTP_GET,
                _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
                    return ProfileHandler::handleGetProfileContent(request);
                }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles - Create a profile from current settings
    _server.on("/rest/profiles", HTTP_POST,
                _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
                    return ProfileHandler::handleCreateProfile(request);
                }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/copy - Copy a profile (source ID and new name in body)
    _server.on("/rest/profiles/copy", HTTP_POST,
                _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
                    return ProfileHandler::handleCopyProfile(request);
                }, AuthenticationPredicates::IS_AUTHENTICATED));

    // PUT /rest/profiles/meta - Update profile metadata (e.g., name) (ID and new name in body)
    _server.on("/rest/profiles/meta", HTTP_PUT,
        _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
            return ProfileHandler::handleUpdateProfileMeta(request);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/set-active - Set a profile as active and apply it (ID in body)
    _server.on("/rest/profiles/set-active", HTTP_POST,
        _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
            return ProfileHandler::handleSetActiveAndApplyProfile(request);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/save - Save current settings to a profile (ID in body)
    _server.on("/rest/profiles/save", HTTP_POST,
        _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
            return ProfileHandler::handleSaveSettingsToProfile(request);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // DELETE /rest/profiles/delete - Delete a profile (ID in body)
    _server.on("/rest/profiles/delete", HTTP_DELETE,
            _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
                return ProfileHandler::handleDeleteProfile(request);
            }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/content - Update profile content
    _server.on("/rest/profiles/content", HTTP_POST,
            _securityManager->wrapRequest([](PsychicRequest* request) -> esp_err_t {
                return ProfileHandler::handleUpdateProfileContent(request);
            }, AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerFileHandler() const
{
    _fileHandler.registerHandlers(_server, _securityManager);
}
