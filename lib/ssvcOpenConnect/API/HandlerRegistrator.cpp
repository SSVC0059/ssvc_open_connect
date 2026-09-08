#include "HandlerRegistrator.h"
#include <AsyncJson.h>
#include <Features.h>
#include "handlers/VkBot/VkBotHandler.h"
#include "core/SsvcCommandsQueue.h"
#include "core/SsvcLogProtocol/SsvcLogProtocol.h"

#include <cstdlib>
#include <limits>

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
    registerVkBot();
    registerSubsystemHandler();
    registerTelegramBotHandler();
    registerProfileHandler();
    registerFileHandler();
    registerLogHandlers();

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

void HandlerRegistrator::registerVkBot() const
{
#if FT_ENABLED(FT_VK_BOT)
    _server.on("/rest/vk/config", HTTP_PUT,
      _securityManager->wrapCallback(
          [](AsyncWebServerRequest* request, JsonVariant& json) {
              VkBotHandler::updateSettings(request, json);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/vk/config", HTTP_GET,
    _securityManager->wrapRequest(
      [](AsyncWebServerRequest* request) {
          VkBotHandler::getSettings(request);
      },
      AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/vk/peers", HTTP_POST,
              _securityManager->wrapCallback(
                  [](AsyncWebServerRequest* request, JsonVariant& json) {
                      VkBotHandler::listConversationPeers(request, json);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
#endif
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

    _server.on("/rest/oc/hardware-config", HTTP_GET,
              _securityManager->wrapRequest(
                  [](AsyncWebServerRequest* request) {
                      OpenConnectHandler::getHardwareConfig(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/oc/hardware-config", HTTP_PUT,
              _securityManager->wrapCallback(
                  [](AsyncWebServerRequest* request, JsonVariant& json) {
                      OpenConnectHandler::putHardwareConfig(request, json);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/oc/relay/capabilities", HTTP_GET,
              _securityManager->wrapRequest(
                  [](AsyncWebServerRequest* request) {
                      OpenConnectHandler::getRelayCapabilities(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/oc/relay/metadata", HTTP_GET,
              _securityManager->wrapRequest(
                  [](AsyncWebServerRequest* request) {
                      OpenConnectHandler::getRelayMetadata(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/oc/relay/state", HTTP_GET,
              _securityManager->wrapRequest(
                  [](AsyncWebServerRequest* request) {
                      OpenConnectHandler::getRelayState(request);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/oc/relay/override", HTTP_POST,
              _securityManager->wrapCallback(
                  [](AsyncWebServerRequest* request, JsonVariant& json) {
                      OpenConnectHandler::postRelayOverride(request, json);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerProfileHandler() const
{
    // Важно: более длинные пути ДО /rest/profiles. Иначе ESPAsyncWebServer сопоставляет
    // GET /rest/profiles первым и запросы к /rest/profiles/content и /rest/profiles/active
    // попадают в handleGetProfiles (список метаданных как JSON-массив), а не в нужные обработчики.

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

    // GET /rest/profiles - Get list of all profiles (metadata)
    _server.on("/rest/profiles", HTTP_GET,
                _securityManager->wrapRequest(
                        [](AsyncWebServerRequest* request) {
                            ProfileHandler::handleGetProfiles(request);
                        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // Важно: все специфические POST-пути /rest/profiles/* регистрируются ДО общего
    // POST /rest/profiles, потому что AsyncURIMatcher в режиме BackwardCompatible
    // сопоставляет "/rest/profiles" с любым путём вида "/rest/profiles/*".
    // Если POST /rest/profiles зарегистрирован раньше, он перехватит запросы к
    // /rest/profiles/set-active, /rest/profiles/copy и т.д., вызывая ошибку 400.

    // POST /rest/profiles/content - Update profile content
    _server.on("/rest/profiles/content", HTTP_POST,
            _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
                ProfileHandler::handleUpdateProfileContent(request, json);
            }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/set-active - Set a profile as active and apply it (ID in body)
    _server.on("/rest/profiles/set-active", HTTP_POST,
        _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
            ProfileHandler::handleSetActiveAndApplyProfile(request, json);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/copy - Copy a profile (source ID and new name in body)
    _server.on("/rest/profiles/copy", HTTP_POST,
                _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
                    ProfileHandler::handleCopyProfile(request, json);
                }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles/save - Save current settings to a profile (ID in body)
    _server.on("/rest/profiles/save", HTTP_POST,
        _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
            ProfileHandler::handleSaveSettingsToProfile(request, json);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // POST /rest/profiles - Create a new profile (generic path — must be LAST among POSTs)
    _server.on("/rest/profiles", HTTP_POST,
                _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
                    ProfileHandler::handleCreateProfile(request, json);
                }, AuthenticationPredicates::IS_AUTHENTICATED));

    // PUT /rest/profiles/meta - Update profile metadata (e.g., name) (ID and new name in body)
    _server.on("/rest/profiles/meta", HTTP_PUT,
        _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
            ProfileHandler::handleUpdateProfileMeta(request, json);
        }, AuthenticationPredicates::IS_AUTHENTICATED));

    // DELETE /rest/profiles/delete - Delete a profile (ID in body)
    _server.on("/rest/profiles/delete", HTTP_DELETE,
            _securityManager->wrapCallback([](AsyncWebServerRequest* request, JsonVariant& json) {
                ProfileHandler::handleDeleteProfile(request, json);
            }, AuthenticationPredicates::IS_AUTHENTICATED));
}

void HandlerRegistrator::registerFileHandler() const
{
    _fileHandler.registerHandlers(_server, _securityManager);
}

void HandlerRegistrator::registerLogHandlers() const
{
    const auto statusName = [](const SsvcLogProtocol::Status status) {
        switch (status) {
        case SsvcLogProtocol::Status::LIST_RECEIVED:
            return "list";
        case SsvcLogProtocol::Status::RECEIVING:
            return "receiving";
        case SsvcLogProtocol::Status::COMPLETED:
            return "completed";
        case SsvcLogProtocol::Status::ERROR:
            return "error";
        case SsvcLogProtocol::Status::IDLE:
        default:
            return "idle";
        }
    };

    _server.on("/rest/logs", HTTP_GET,
               _securityManager->wrapRequest(
                   [statusName](AsyncWebServerRequest* request) {
                       auto& transfer = SsvcLogProtocol::getTransfer();
                       if (transfer.status() == SsvcLogProtocol::Status::IDLE) {
                           SsvcCommandsQueue::getQueue().getLog();
                       }

                       JsonDocument response;
                       response["status"] = statusName(transfer.status());
                       response["total"] = transfer.totalChunks();
                       response["received"] = transfer.receivedChunks();
                       if (!transfer.error().empty()) {
                           response["error"] = transfer.error();
                       }
                       if (transfer.status() == SsvcLogProtocol::Status::LIST_RECEIVED) {
                           JsonArray files = response["files"].to<JsonArray>();
                           for (const auto& file : transfer.files()) {
                               files.add(file);
                           }
                       }
                       String serialized;
                       serializeJson(response, serialized);
                       request->send(200, "application/json", serialized);
                   },
                   AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on("/rest/logs/*", HTTP_GET,
               _securityManager->wrapRequest(
                   [statusName](AsyncWebServerRequest* request) {
                       const String prefix = "/rest/logs/";
                       const String process = request->url().substring(prefix.length());
                       char* end = nullptr;
                       const long processNumber = std::strtol(process.c_str(), &end, 10);
                       if (end == process.c_str() || *end != '\0' || processNumber <= 0 ||
                           processNumber > std::numeric_limits<int>::max()) {
                           request->send(400, "application/json", R"({"error":"invalid_process_id"})");
                           return;
                       }

                       auto& transfer = SsvcLogProtocol::getTransfer();
                       const std::string expectedName = process.c_str() + std::string(".CSV");
                       if (transfer.status() == SsvcLogProtocol::Status::RECEIVING &&
                           !transfer.fileName().empty() &&
                           transfer.fileName() != expectedName) {
                           request->send(409, "application/json",
                                         R"({"error":"log_transfer_busy"})");
                           return;
                       }
                       if (transfer.status() == SsvcLogProtocol::Status::COMPLETED &&
                           transfer.fileName() == expectedName) {
                           auto* response = request->beginResponse(
                               200, "text/csv", transfer.data().c_str(), transfer.data().size());
                           response->addHeader("Content-Disposition",
                                               ("attachment; filename=\"" + expectedName + "\"").c_str());
                           request->send(response);
                           return;
                       }

                       if (transfer.status() != SsvcLogProtocol::Status::RECEIVING ||
                           transfer.fileName() != expectedName) {
                           SsvcCommandsQueue::getQueue().getLog(std::to_string(processNumber));
                       }

                       JsonDocument response;
                       response["status"] = statusName(transfer.status());
                       response["total"] = transfer.totalChunks();
                       response["received"] = transfer.receivedChunks();
                       if (!transfer.error().empty()) {
                           response["error"] = transfer.error();
                       }
                       String serialized;
                       serializeJson(response, serialized);
                       request->send(202, "application/json", serialized);
                   },
                   AuthenticationPredicates::IS_AUTHENTICATED));
}
