#include "OpenConnectSettingsManager.h"

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

void OpenConnectSettingsManager::read(const OpenConnectSettingsManager& settings, const JsonObject& root)
{
  ESP_LOGI("OpenConnectSettings", "Reading settings from JSON");
  if (settings.pid)
  {
    root["pid"] = settings.pid;
  }
  if (!settings.sensorsConfig.empty())
  {
    root["sensorsConfig"] = settings.sensorsConfig;
  }
}

StateUpdateResult OpenConnectSettingsManager::update(const JsonObject& root, OpenConnectSettingsManager& settings)
{
  ESP_LOGI("OpenConnectSettingsManager", "Updating settings from JSON");

  // Обновление pid с проверкой типа
  if (root["pid"].is<int>())
  {
    settings.pid = root["pid"].as<int>();
  }
  else
  {
    ESP_LOGW("OpenConnectSettingsManager", "Invalid pid type in JSON");
  }

  // Обновление sensorsConfig с современной проверкой
  const JsonVariant configVar = root["sensorsConfig"];
  if (!configVar.isNull())
  {
    if (configVar.is<const char*>())
    {
      settings.sensorsConfig = configVar.as<const char*>();
    }
    else if (configVar.is<std::string>())
    {
      settings.sensorsConfig = configVar.as<std::string>();
    }
    else
    {
      ESP_LOGW("OpenConnectSettingsManager", "Invalid sensorsConfig type");
      settings.sensorsConfig.clear();
    }
  }
  else
  {
    settings.sensorsConfig.clear();
  }

  return StateUpdateResult::CHANGED;
}

OpenConnectSettingsService::OpenConnectSettingsService(
    PsychicHttpServer *server, ESP32SvelteKit *_esp32sveltekit,
    SecurityManager *_securityManager)
    : _httpEndpoint(OpenConnectSettingsManager::read, OpenConnectSettingsManager::update,
                    this, server, OPEN_CONNECT_SETTINGS_ENDPOINT_PATH,
                    _securityManager,
                    AuthenticationPredicates::IS_AUTHENTICATED),
      _fsPersistence(OpenConnectSettingsManager::read, OpenConnectSettingsManager::update,
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

void OpenConnectSettingsService::onConfigUpdated()
{

}