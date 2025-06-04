//
// Created by demoncat on 11.03.2025.
//

#include "ESP32SvelteKit.h"

#ifndef SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H
#define SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H

#define OPEN_CONNECT_SETTINGS_ENDPOINT_PATH "/rest/oc_settings"
#define OPEN_CONNECT_SETTINGS_FILE "/config/Settings.json"

class OpenConnectSettings
{
public:
    int pid;
    std::string sensorsConfig;

    static void read(OpenConnectSettings& settings, JsonObject& root)
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

    static StateUpdateResult update(JsonObject& root, OpenConnectSettings& settings)
    {
        ESP_LOGI("OpenConnectSettings", "Updating settings from JSON");

        // Обновление pid с проверкой типа
        if (root["pid"].is<int>())
        {
            settings.pid = root["pid"].as<int>();
        }
        else
        {
            ESP_LOGW("OpenConnectSettings", "Invalid pid type in JSON");
        }

        // Обновление sensorsConfig с современной проверкой
        JsonVariant configVar = root["sensorsConfig"];
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
                ESP_LOGW("OpenConnectSettings", "Invalid sensorsConfig type");
                settings.sensorsConfig.clear();
            }
        }
        else
        {
            settings.sensorsConfig.clear();
        }

        return StateUpdateResult::CHANGED;
    }
};

class OpenConnectSettingsService : public StatefulService<OpenConnectSettings>
{
public:
    OpenConnectSettingsService(PsychicHttpServer* server,
                               ESP32SvelteKit* _esp32sveltekit,
                               SecurityManager* _securityManager);

    ~OpenConnectSettingsService();

    void begin();

private:
    HttpEndpoint<OpenConnectSettings> _httpEndpoint;
    FSPersistence<OpenConnectSettings> _fsPersistence;

    static void onConfigUpdated();
};


#endif // SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H
