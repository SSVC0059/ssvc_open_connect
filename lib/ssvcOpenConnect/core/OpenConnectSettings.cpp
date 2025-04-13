//
// Created by demoncat on 11.03.2025.
//

#include "OpenConnectSettings.h"

OpenConnectSettingsService::OpenConnectSettingsService(
    PsychicHttpServer *server, ESP32SvelteKit *_esp32sveltekit,
    SecurityManager *securityManager)
    : _httpEndpoint(OpenConnectSettings::read, OpenConnectSettings::update,
                    this, server, OPEN_CONNECT_SETTINGS_ENDPOINT_PATH,
                    securityManager,
                    AuthenticationPredicates::IS_AUTHENTICATED),
      _fsPersistence(OpenConnectSettings::read, OpenConnectSettings::update,
                     this, _esp32sveltekit->getFS(), OPEN_CONNECT_SETTINGS_FILE)

{
  // configure settings service update handler to update LED state
  addUpdateHandler([&](const String &originId) { onConfigUpdated(); }, false);
}

OpenConnectSettingsService::~OpenConnectSettingsService() = default;

void OpenConnectSettingsService::begin() {
  _httpEndpoint.begin();
  _fsPersistence.readFromFS();
}

void OpenConnectSettingsService::onConfigUpdated() {}