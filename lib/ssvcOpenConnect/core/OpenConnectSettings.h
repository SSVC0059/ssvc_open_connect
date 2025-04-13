//
// Created by demoncat on 11.03.2025.
//

#include "ESP32SvelteKit.h"

#ifndef SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H
#define SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H

#define OPEN_CONNECT_SETTINGS_ENDPOINT_PATH "/rest/oc_settings"
#define OPEN_CONNECT_SETTINGS_FILE "/config/Settings.json"

class OpenConnectSettings {
public:
    int pid;

    static void read(OpenConnectSettings &settings, JsonObject &root) {
        ESP_LOGI("OpenConnectSettings", "Reading settings from JSON");
        root["pid"] = settings.pid;
    }

    static StateUpdateResult update(JsonObject &root,
                                    OpenConnectSettings &settings) {
        ESP_LOGI("OpenConnectSettings", "Updating settings from JSON");
        if (root["pid"]) {
            settings.pid = root["pid"];
        } else {
            return StateUpdateResult::UNCHANGED;
        }
        return StateUpdateResult::CHANGED;
    }
};

class OpenConnectSettingsService : public StatefulService<OpenConnectSettings> {
public:
    OpenConnectSettingsService(PsychicHttpServer *server,
                               ESP32SvelteKit *_esp32sveltekit,
                               SecurityManager *_securityManager);

    ~OpenConnectSettingsService();

    void begin();

private:
    HttpEndpoint<OpenConnectSettings>  _httpEndpoint;
    FSPersistence<OpenConnectSettings> _fsPersistence;

    void onConfigUpdated();
};

#endif // SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H
